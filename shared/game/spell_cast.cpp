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
#include "spell_cast.h"
#include "game_unit.h"
#include "game_character.h"
#include "common/countdown.h"
#include "no_cast_state.h"
#include "single_cast_state.h"
#include "log/default_log_levels.h"
#include "world_instance.h"
#include "each_tile_in_region.h"
#include "each_tile_in_sight.h"

namespace wowpp
{
	SpellCast::SpellCast(TimerQueue &timers, GameUnit &executer)
		: m_timers(timers)
		, m_executer(executer)
		, m_castState(new NoCastState)
	{
	}

	std::pair<game::SpellCastResult, SpellCasting *> SpellCast::startCast(const proto::SpellEntry &spell, SpellTargetMap target, Int32 basePoints, GameTime castTime, bool isProc, UInt64 itemGuid)
	{
		assert(m_castState);

		GameUnit *unitTarget = nullptr;
		target.resolvePointers(*m_executer.getWorldInstance(), &unitTarget, nullptr, nullptr, nullptr);

		// Check if caster is dead
		if ((spell.attributes(0) & game::spell_attributes::CastableWhileDead) == 0)
		{
			if (!m_executer.isAlive())
			{
				return std::make_pair(game::spell_cast_result::FailedCasterDead, nullptr);
			}
		}

		if (m_executer.isStunned() &&
			(spell.attributes(5) & game::spell_attributes_ex_e::UsableWhileStunned) == 0)
		{
			return std::make_pair(game::spell_cast_result::FailedStunned, nullptr);
		}

		if (m_executer.isInCombat() &&
			spell.attributes(0) & game::spell_attributes::NotInCombat)
		{
			return std::make_pair(game::spell_cast_result::FailedAffectingCombat, nullptr);
		}

		// Check for cooldown
		if (m_executer.hasCooldown(spell.id()))
		{
			return std::make_pair(game::spell_cast_result::FailedNotReady, nullptr);
		}

		auto *instance = m_executer.getWorldInstance();
		if (!instance)
		{
			ELOG("Caster is not in a world instance");
			return std::make_pair(game::spell_cast_result::FailedError, nullptr);
		}

		auto *map = instance->getMapData();
		if (!map)
		{
			ELOG("World instance has no map data loaded");
			return std::make_pair(game::spell_cast_result::FailedError, nullptr);
		}

		// Check for focus object
		if (spell.focusobject() != 0)
		{
			TileIndex2D tileIndex;
			m_executer.getTileIndex(tileIndex);

			bool foundObject = false;

			forEachTileInSight(
				instance->getGrid(),
				tileIndex,
				[this, &foundObject, &spell](VisibilityTile &tile)
			{
				for (auto &object : tile.getGameObjects())
				{
					if (!object->isWorldObject())
					{
						return;
					}

					WorldObject *worldObject = reinterpret_cast<WorldObject*>(object);
					if (worldObject->getUInt32Value(world_object_fields::TypeID) != world_object_type::SpellFocus)
					{
						return;
					}

					if (worldObject->getEntry().data(0) != spell.focusobject())
					{
						return;
					}

					float distance = float(worldObject->getEntry().data(1));
					if (worldObject->getDistanceTo(getExecuter()) <= distance)
					{
						foundObject = true;
						return;
					}
				}
			});

			if (!foundObject)
			{
				return std::make_pair(game::spell_cast_result::FailedRequiresSpellFocus, nullptr);
			}
		}

		if (spell.mechanic() != 0)
		{
			if (unitTarget &&
				unitTarget->isImmuneAgainstMechanic(spell.mechanic()))
			{
				return std::make_pair(game::spell_cast_result::FailedPreventedByMechanic, nullptr);
			}
		}

		//Must be behind the target.
		if (unitTarget)
		{
			math::Vector3 location(m_executer.getLocation());

			if (spell.attributes(2) == 0x100000 &&
			        (spell.attributes(1) & game::spell_attributes_ex_a::MeleeCombatStart) == game::spell_attributes_ex_a::MeleeCombatStart &&
			        unitTarget->isInArc(3.1415927f, location.x, location.y))
			{
				return std::make_pair(game::spell_cast_result::FailedNotBehind, nullptr);
			}
		}

		// Check power
		Int32 powerCost = itemGuid ? 0 : calculatePowerCost(spell);
		if (powerCost > 0)
		{
			if (spell.powertype() == game::power_type::Health)
			{
				UInt32 currentPower = m_executer.getUInt32Value(unit_fields::Health);
				if (powerCost > 0 &&
					currentPower < UInt32(powerCost))
				{
					return std::make_pair(game::spell_cast_result::FailedNoPower, nullptr);
				}
			}
			else
			{
				UInt32 currentPower = m_executer.getUInt32Value(unit_fields::Power1 + spell.powertype());
				if (powerCost > 0 &&
					currentPower < UInt32(powerCost))
				{
					return std::make_pair(game::spell_cast_result::FailedNoPower, nullptr);
				}
			}
		}

		// Range check
		if (spell.minrange() != 0.0f || spell.maxrange() != 0.0f)
		{
			if (unitTarget)
			{
				float maxrange = spell.maxrange();
				if (m_executer.isGameCharacter())
				{
					reinterpret_cast<GameCharacter&>(m_executer).applySpellMod(spell_mod_op::Range, spell.id(), maxrange);
				}

				if (maxrange < spell.minrange()) maxrange = spell.minrange();

				const float combatReach = unitTarget->getFloatValue(unit_fields::CombatReach) + m_executer.getFloatValue(unit_fields::CombatReach);
				const float distance = m_executer.getDistanceTo(*unitTarget);
				if (spell.minrange() > 0.0f && distance < spell.minrange())
				{
					return std::make_pair(game::spell_cast_result::FailedTooClose, nullptr);
				}
				else if (maxrange > 0.0f && distance > maxrange + combatReach)
				{
					return std::make_pair(game::spell_cast_result::FailedOutOfRange, nullptr);
				}
			}
		}

		// Check facing (Need to have the target in front of us)
		if (spell.facing() & 0x01 &&
			!(spell.attributes(2) & game::spell_attributes_ex_b::CantReflect))
		{
			const auto *world = m_executer.getWorldInstance();
			if (world)
			{
				if (unitTarget)
				{
					math::Vector3 location(unitTarget->getLocation());

					// 120 degree field of view
					if (!m_executer.isInArc(2.0f * 3.1415927f / 3.0f, location.x, location.y))
					{
						return std::make_pair(game::spell_cast_result::FailedUnitNotInfront, nullptr);
					}
				}
				else
				{
					WLOG("Couldn't find unit target for spell with facing requirement!");
				}
			}
		}

		// Check if we have enough resources for that spell
		if (isProc)
		{
			std::shared_ptr<SingleCastState> newState(
			    new SingleCastState(*this, spell, std::move(target), basePoints, castTime, true, itemGuid)
			);
			newState->activate();

			return std::make_pair(game::spell_cast_result::CastOkay, nullptr);
		}
		else
		{
			// Check for line of sight
			{
				if (unitTarget && unitTarget != &m_executer)
				{
					if (!m_executer.isInLineOfSight(*unitTarget))
					{
						return std::make_pair(game::spell_cast_result::FailedLineOfSight, nullptr);
					}
				}
			}

			return m_castState->startCast
			       (*this,
			        spell,
			        std::move(target),
			        basePoints,
			        castTime,
			        false,
			        itemGuid);
		}
	}

	void SpellCast::stopCast(game::SpellInterruptFlags reason, UInt64 interruptCooldown/* = 0*/)
	{
		assert(m_castState);
		m_castState->stopCast(reason, interruptCooldown);
	}

	void SpellCast::onUserStartsMoving()
	{
		assert(m_castState);
		m_castState->onUserStartsMoving();
	}

	void SpellCast::setState(std::shared_ptr<CastState> castState)
	{
		assert(castState);
		assert(m_castState);

		m_castState = std::move(castState);
		m_castState->activate();
	}

	void SpellCast::finishChanneling(bool cancel)
	{
		assert(m_castState);

		m_castState->finishChanneling(cancel);
	}

	Int32 SpellCast::calculatePowerCost(const proto::SpellEntry & spell) const
	{
		// DrainAllPower flag handler (used for Lay on Hands)
		if (spell.attributes(1) & game::spell_attributes_ex_a::DrainAllPower)
		{
			auto powerType = static_cast<game::PowerType>(spell.powertype());
			switch (powerType)
			{
				case game::power_type::Health:
					return m_executer.getUInt32Value(unit_fields::Health);
				default:
					return m_executer.getUInt32Value(unit_fields::Power1 + spell.powertype());
			}
		}

		// Adjust power cost per level if needed
		Int32 cost = spell.cost();
		if (spell.costpct() > 0)
		{
			auto powerType = static_cast<game::PowerType>(spell.powertype());
			switch (powerType)
			{
				case game::power_type::Health:
					cost += m_executer.getUInt32Value(unit_fields::BaseHealth) * spell.costpct() / 100;
					break;
				case game::power_type::Mana:
					cost += m_executer.getUInt32Value(unit_fields::BaseMana) * spell.costpct() / 100;
					break;
				default:
					cost += m_executer.getUInt32Value(unit_fields::MaxPower1 + spell.powertype()) * spell.costpct() / 100;
					break;
			}
		}

		if (spell.attributes(0) & game::spell_attributes::LevelDamageCalc)
		{
			cost = Int32(cost / (1.117f * spell.spelllevel() / m_executer.getLevel() - 0.1327f));
		}
		
		UInt8 school = 0;
		for (UInt8 i = 0; i < 7; ++i)
		{
			if (spell.schoolmask() & (1 << i)) {
				school = i;
			}
		}

		cost = Int32(cost * (float(m_executer.getFloatValue(unit_fields::PowerCostMultiplier + school) + 100) / 100.0f));

		if (m_executer.isGameCharacter())
		{
			reinterpret_cast<GameCharacter&>(m_executer).applySpellMod(
				spell_mod_op::Cost, spell.id(), cost);
		}

		return cost;
	}

	SpellCasting &castSpell(SpellCast &cast, const proto::SpellEntry &spell, SpellTargetMap target, Int32 basePoints, GameTime castTime, UInt64 itemGuid)
	{
		std::shared_ptr<SingleCastState> newState(
		    new SingleCastState(cast, spell, std::move(target), basePoints, castTime, false, itemGuid)
		);

		auto &casting = newState->getCasting();
		cast.setState(std::move(newState));
		return casting;
	}
	void SpellCast::CastState::finishChanneling(bool cancel)
	{
	}
}
