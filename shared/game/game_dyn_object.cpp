//
// This file is part of the WoW++ project.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// World of Warcraft, and all World of Warcraft or Warcraft art, images,
// and lore are copyrighted by Blizzard Entertainment, Inc.
//

#include "pch.h"
#include "game_dyn_object.h"
#include "log/default_log_levels.h"
#include "binary_io/vector_sink.h"
#include "common/clock.h"
#include "visibility_tile.h"
#include "world_instance.h"
#include "universe.h"
#include "proto_data/project.h"
#include "game_character.h"
#include "common/make_unique.h"
#include "unit_finder.h"
#include "aura_spell_slot.h"

namespace wowpp
{
	DynObject::DynObject(
	    proto::Project &project,
	    TimerQueue &timers,
		GameUnit &caster,
		const proto::SpellEntry &entry,
		const proto::SpellEffect &effect)
		: GameObject(project)
		, m_timers(timers)
		, m_caster(caster)
		, m_entry(entry)
		, m_effect(effect)
		, m_despawnTimer(timers)
		, m_tickCountdown(timers)
	{
		m_values.resize(dyn_object_fields::DynObjectFieldCount, 0);
		m_valueBitset.resize((dyn_object_fields::DynObjectFieldCount + 31) / 32, 0);

		m_onDespawn = m_despawnTimer.ended.connect([this]()
		{
			for (const auto &target : m_auras)
			{
				target.first->getAuras().removeAllAurasDueToSpell(m_entry.id());
			}

			m_caster.removeDynamicObject(getGuid());
		});
	}

	DynObject::~DynObject()
	{
	}

	void DynObject::initialize()
	{
		GameObject::initialize();

		setUInt32Value(object_fields::Type, 65);
		setUInt32Value(object_fields::Entry, m_entry.id());
		setFloatValue(object_fields::ScaleX, 1.0f);

		auto &loc = getLocation();
		setUInt64Value(dyn_object_fields::Caster, m_caster.getGuid());
		setUInt32Value(dyn_object_fields::Bytes, 0x00000001);
		setUInt32Value(dyn_object_fields::SpellId, m_entry.id());
		setFloatValue(dyn_object_fields::Radius, m_effect.radius());
		setFloatValue(dyn_object_fields::PosX, loc.x);
		setFloatValue(dyn_object_fields::PosY, loc.y);
		setFloatValue(dyn_object_fields::PosZ, loc.z);
		setFloatValue(dyn_object_fields::Facing, 0.0f);
		setUInt32Value(dyn_object_fields::CastTime, TimeStamp());

		// TODO: Also need to toggle despawn timer?
	}

	void DynObject::relocate(const math::Vector3 & position, float o, bool fire)
	{
		GameObject::relocate(position, o, fire);

		setFloatValue(dyn_object_fields::PosX, position.x);
		setFloatValue(dyn_object_fields::PosY, position.y);
		setFloatValue(dyn_object_fields::PosZ, position.z);
		setFloatValue(dyn_object_fields::Facing, o);
	}

	void DynObject::writeCreateObjectBlocks(std::vector<std::vector<char>> &out_blocks, bool creation /*= true*/) const
	{
		// TODO
	}

	void DynObject::triggerDespawnTimer(UInt64 duration)
	{
		m_despawnTimer.setEnd(getCurrentTime() + duration);
	}

	void DynObject::startUnitWatcher()
	{
		auto *worldInstance = m_caster.getWorldInstance();
		ASSERT(worldInstance);

		Circle circle(m_position.x, m_position.y, getFloatValue(dyn_object_fields::Radius));
		m_unitWatcher = worldInstance->getUnitFinder().watchUnits(circle, [this](GameUnit & target, bool isInside) -> bool
		{
			// Make sure that we don't apply the same aura twice
			if (isInside && m_auras.find(&target) != m_auras.end())
				return false;

			// Don't apply this aura on dead targets
			if (isInside && !target.isAlive())
				return false;

			if (target.hasFlag(unit_fields::UnitFlags, game::unit_flags::NotAttackable | game::unit_flags::NotSelectable | game::unit_flags::OOCNotAttackable))
				return false;

			if (m_effect.targetb() == game::targets::DestDynObjAlly || m_effect.targetb() == game::targets::UnitAreaAllyDst)
			{
				if (!m_caster.isFriendlyTo(target))
					return false;
			}
			else if (m_caster.isGameCharacter())
			{
				if (m_caster.isFriendlyTo(target))
					return false;
			}
			else
			{
				if (!m_caster.isHostileTo(target))
					return false;
			}

			if (target.isImmune(m_entry.schoolmask()) ||
				target.isImmuneAgainstMechanic(1 << m_entry.mechanic()) ||
				target.isImmuneAgainstMechanic(1 << m_effect.mechanic()))
			{
				return false;
			}

			// Don't apply if target is not in line of sight
			if (isInside && !target.isInLineOfSight(*this))
				return false;

			if (isInside)
			{
				SpellTargetMap targetMap;
				targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
				targetMap.m_unitTarget = target.getGuid();

				auto *world = m_caster.getWorldInstance();
				auto &universe = world->getUniverse();

				// Create an aura spell slot
				auto auraSlot = std::make_shared<AuraSpellSlot>(universe.getTimers(), m_entry);
				auraSlot->setOwner(std::static_pointer_cast<GameUnit>(target.shared_from_this()));
				auraSlot->setCaster(std::static_pointer_cast<GameUnit>(m_caster.shared_from_this()));

				std::shared_ptr<AuraEffect> aura = std::make_shared<AuraEffect>(*auraSlot, m_effect, m_effect.basepoints(), &m_caster, target, targetMap, true);
				auraSlot->addAuraEffect(aura);

				if (target.getAuras().addAura(auraSlot))
				{
					m_auras[&target] = std::move(auraSlot);
				}
			}
			else
			{
				auto auraIt = m_auras.find(&target);
				if (auraIt != m_auras.end())
				{
					target.getAuras().removeAura(*auraIt->second);
					auraIt = m_auras.erase(auraIt);
				}
			}

			return false;
		});
		m_unitWatcher->start();
		
		m_onTick = m_tickCountdown.ended.connect([this]()
		{
			for (auto const &aura : m_auras)
			{
				aura.second->forEachEffect([](std::shared_ptr<AuraEffect> effect) -> bool {
					effect->update();
					return true;
				});
			}

			updatePeriodicTimer();
		});

		updatePeriodicTimer();
	}

	void DynObject::updatePeriodicTimer()
	{
		m_tickCountdown.setEnd(
			getCurrentTime() + m_effect.amplitude());
	}

	io::Writer &operator<<(io::Writer &w, DynObject const &object)
	{
		// Write the bitset and values
		return w
		       << reinterpret_cast<GameObject const &>(object);
	}

	io::Reader &operator>>(io::Reader &r, DynObject &object)
	{
		// Read the bitset and values
		return r
		       >> reinterpret_cast<GameObject &>(object);
	}
}
