//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
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

#include "aura.h"
#include "common/clock.h"
#include "game_unit.h"
#include "game_character.h"
#include "each_tile_in_sight.h"
#include "world_instance.h"
#include "binary_io/vector_sink.h"
#include "game_protocol/game_protocol.h"
#include "universe.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	Aura::Aura(const SpellEntry &spell, const SpellEntry::Effect &effect, Int32 basePoints, GameUnit &caster, GameUnit &target, PostFunction post, std::function<void(Aura&)> onDestroy)
		: m_spell(spell)
		, m_effect(effect)
		, m_caster(&caster)
		, m_target(target)
		, m_tickCount(0)
		, m_applyTime(getCurrentTime())
		, m_basePoints(basePoints)
		, m_expireCountdown(target.getTimers())
		, m_tickCountdown(target.getTimers())
		, m_isPeriodic(false)
		, m_expired(false)
		, m_attackerLevel(caster.getLevel())
		, m_slot(0xFF)
		, m_post(std::move(post))
		, m_destroy(std::move(onDestroy))
	{
		// Subscribe to caster despawn event so that we don't hold an invalid pointer
		m_casterDespawned = caster.despawned.connect(
			std::bind(&Aura::onCasterDespawned, this, std::placeholders::_1));
		m_onExpire = m_expireCountdown.ended.connect(
			std::bind(&Aura::onExpired, this));
		m_onTick = m_tickCountdown.ended.connect(
			std::bind(&Aura::onTick, this));

		// Log duration
		DLOG("Aura " << spell.name << ": Duration " << spell.duration << " / " << spell.maxDuration);
	}

	Aura::~Aura()
	{
		// Cancel countdowns (if running)
		m_tickCountdown.cancel();
		m_expireCountdown.cancel();

		// Remove aura slot
		if (m_slot != 0xFF)
		{
			m_target.setUInt32Value(unit_fields::Aura + m_slot, 0);
		}

		// Remove aura modifier from target
		handleModifier(false);
	}

	void Aura::handleModNull(bool apply)
	{
		// Nothing to do here
		DLOG("AURA_TYPE_MOD_NULL: Nothing to do");
	}

	void Aura::handleModStat(bool apply)
	{
		Int32 stat = m_effect.miscValueA;
		if (stat < -2 || stat > 4)
		{
			WLOG("AURA_TYPE_MOD_STAT: Invalid stat index " << stat << " - skipped");
			return;
		}

		// Apply all stats
		for (Int32 i = 0; i < 5; ++i)
		{
			if (stat < 0 || stat == i)
			{
				m_target.updateModifierValue(GameUnit::getUnitModByStat(i), unit_mod_type::TotalValue, m_basePoints, apply);
			}
		}
	}

	void Aura::handleModResistance(bool apply)
	{
		// Apply all resistances
		for (UInt8 i = 0; i < 7; ++i)
		{
			if (m_effect.miscValueA & Int32(1 << i))
			{
				m_target.updateModifierValue(UnitMods(unit_mods::ResistanceStart + i), unit_mod_type::TotalValue, m_basePoints, apply);
			}
		}
	}

	bool Aura::isPositive() const
	{
		// Negative attribute
		if (m_spell.attributes & 0x04000000)
			return false;

		// TODO

		return true;
	}

	void Aura::onCasterDespawned(GameObject &object)
	{
		WLOG("Caster of aura just despawned");

		// Reset caster
		m_casterDespawned.disconnect();
		m_caster = nullptr;
	}

	void Aura::handleModifier(bool apply)
	{
		namespace aura = game::aura_type;

		switch (m_effect.auraName)
		{
		case aura::None:
			handleModNull(apply);
			break;
		case aura::ModStat:
			handleModStat(apply);
			break;
		case aura::ModTotalStatPercentage:
			handleModTotalStatPercentage(apply);
			break;
		case aura::ModResistance:
			handleModResistance(apply);
			break;
		case aura::PeriodicDamage:
			handlePeriodicDamage(apply);
			break;
		case aura::PeriodicHeal:
			handlePeriodicHeal(apply);
			break;
		case aura::ModShapeShift:
			handleModShapeShift(apply);
			break;
		case aura::ModStealth:
			handleModStealth(apply);
			break;
		default:
			WLOG("Unhandled aura type: " << m_effect.auraName);
			break;
		}
	}

	void Aura::onExpired()
	{
		// Expired
		m_expired = true;

		// Apply last tick if periodic
		if (m_isPeriodic)
		{
			DLOG("Tick on expired!");
			onTick();
		}

		// If the target died already, don't destroy this aura (it will be destroyed elsewhere)
		if (m_target.getUInt32Value(unit_fields::Health) > 0)
		{
			// Destroy this instance
			m_destroy(*this);
		}
	}

	void Aura::onTick()
	{
		// Increase tick counter
		m_tickCount++;

		namespace aura = game::aura_type;
		switch (m_effect.auraName)
		{
		case aura::PeriodicDamage:
		{
			UInt32 school = m_spell.schoolMask;
			UInt32 fullDamage = m_basePoints;
			UInt32 absorbed = 0;
			UInt32 resisted = 0;
			Int32 damage = fullDamage - absorbed - resisted;
			if (damage < 0) damage = 0;

			// Reduce by armor if physical
			if (school & 1 &&
				m_effect.mechanic != 15)	// Bleeding
			{
				calculateArmorReducedDamage(m_attackerLevel, m_target, damage);
			}

			// Send event to all subscribers in sight
			auto *world = m_target.getWorldInstance();
			if (world)
			{
				TileIndex2D tileIndex;
				m_target.getTileIndex(tileIndex);

				std::vector<char> buffer;
				io::VectorSink sink(buffer);
				wowpp::game::OutgoingPacket packet(sink);
				game::server_write::periodicAuraLog(packet, m_target.getGuid(), (m_caster ? m_caster->getGuid() : 0), m_spell.id, m_effect.auraName, damage, school, absorbed, resisted);

				forEachSubscriberInSight(world->getGrid(), tileIndex, [&packet, &buffer](ITileSubscriber &subscriber)
				{
					subscriber.sendPacket(packet, buffer);
				});
			}

			// Update health value
			m_target.dealDamage(damage, school, m_caster);
			break;
		}
		case aura::PeriodicDamagePercent:
		{
			DLOG("TODO");
			break;
		}
		case aura::PeriodicDummy:
		{
			DLOG("TODO");
			break;
		}
		case aura::PeriodicEnergize:
		{
			DLOG("TODO");
			break;
		}
		case aura::PeriodicHeal:
		{
			UInt32 heal = m_basePoints;

			// Send event to all subscribers in sight
			auto *world = m_target.getWorldInstance();
			if (world)
			{
				TileIndex2D tileIndex;
				m_target.getTileIndex(tileIndex);

				std::vector<char> buffer;
				io::VectorSink sink(buffer);
				wowpp::game::OutgoingPacket packet(sink);
				game::server_write::periodicAuraLog(packet, m_target.getGuid(), (m_caster ? m_caster->getGuid() : 0), m_spell.id, m_effect.auraName, heal);

				forEachSubscriberInSight(world->getGrid(), tileIndex, [&packet, &buffer](ITileSubscriber &subscriber)
				{
					subscriber.sendPacket(packet, buffer);
				});
			}

			// Update health value
			UInt32 health = m_target.getUInt32Value(unit_fields::Health);
			UInt32 maxHealth = m_target.getUInt32Value(unit_fields::MaxHealth);
			if (health + maxHealth > maxHealth)
				health = maxHealth;
			else
				health += heal;

			m_target.setUInt32Value(unit_fields::Health, health);
			break;
		}
		case aura::PeriodicHealthFunnel:
		{
			DLOG("TODO");
			break;
		}
		case aura::PeriodicLeech:
		{
			DLOG("TODO");
			break;
		}
		case aura::PeriodicManaFunnel:
		{
			DLOG("TODO");
			break;
		}
		case aura::PeriodicManaLeech:
		{
			DLOG("TODO");
			break;
		}
		case aura::PeriodicTriggerSpell:
		{
			DLOG("TODO");
			break;
		}
		case aura::PeriodicTriggerSpellWithValue:
		{
			DLOG("TODO");
			break;
		}
		default:
		{
			WLOG("Unhandled aura type for periodic tick: " << m_effect.auraName);
			break;
		}
		}

		// Should next tick be triggered?
		if (!m_expired)
		{
			startPeriodicTimer();
		}
	}

	void Aura::applyAura()
	{
		// Check if this aura is permanent (until user cancel's it)
		if (m_spell.maxDuration > 0)
		{
			// Get spell duration
			m_expireCountdown.setEnd(
				getCurrentTime() + m_spell.duration);
		}

		// Watch for unit's movement if the aura should interrupt in this case
		if ((m_spell.auraInterruptFlags & spell_aura_interrupt_flags::Move) != 0 ||
			(m_spell.auraInterruptFlags & spell_aura_interrupt_flags::Turning) != 0)
		{
			m_targetMoved = m_target.moved.connect(
				std::bind(&Aura::onTargetMoved, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
		}

		// Apply modifier
		handleModifier(true);
	}

	void Aura::handlePeriodicDamage(bool apply)
	{
		if (!apply)
			return;

		// Toggle periodic flag
		m_isPeriodic = true;

		// First tick at apply
		if (m_spell.attributesEx[4] & 0x00000200)
		{
			// First tick
			onTick();
		}
		else
		{
			// Start timer
			startPeriodicTimer();
		}
	}

	void Aura::handlePeriodicHeal(bool apply)
	{
		if (!apply)
			return;

		// Toggle periodic flag
		m_isPeriodic = true;

		// First tick at apply
		if (m_spell.attributesEx[4] & 0x00000200)
		{
			// First tick
			onTick();
		}
		else
		{
			// Start timer
			startPeriodicTimer();
		}
	}

	void Aura::handleModTotalStatPercentage(bool apply)
	{
		Int32 stat = m_effect.miscValueA;
		if (stat < -2 || stat > 4)
		{
			WLOG("AURA_TYPE_MOD_STAT_PERCENTAGE: Invalid stat index " << stat << " - skipped");
			return;
		}

		// Apply all stats
		for (Int32 i = 0; i < 5; ++i)
		{
			if (stat < 0 || stat == i)
			{
				m_target.updateModifierValue(GameUnit::getUnitModByStat(i), unit_mod_type::TotalPct, m_basePoints, apply);
			}
		}
	}

	void Aura::startPeriodicTimer()
	{
		// Start timer
		m_tickCountdown.setEnd(
			getCurrentTime() + m_effect.amplitude);
	}

	void Aura::setSlot(UInt8 newSlot)
	{
		if (newSlot != m_slot)
		{
			m_slot = newSlot;
		}
	}

	void Aura::handleModShapeShift(bool apply)
	{
		// This effect is still todo

		UInt8 form = m_effect.miscValueA;
		if (apply)
		{
			m_target.setByteValue(unit_fields::Bytes2, 3, form);
		}
		else
		{
			m_target.setByteValue(unit_fields::Bytes2, 3, 0);
		}
	}

	void Aura::handleModStealth(bool apply)
	{
		if (apply)
		{
			m_target.setByteValue(unit_fields::Bytes1, 2, 0x02);
			if (m_target.getTypeId() == object_type::Character)
			{
				UInt32 val = m_target.getUInt32Value(character_fields::CharacterBytes2);
				m_target.setUInt32Value(character_fields::CharacterBytes2, val | 0x20);
			}

			// TODO: Update visibility mode of unit
		}
		else
		{
			m_target.setByteValue(unit_fields::Bytes1, 2, 0x00);
			if (m_target.getTypeId() == object_type::Character)
			{
				UInt32 val = m_target.getUInt32Value(character_fields::CharacterBytes2);
				m_target.setUInt32Value(character_fields::CharacterBytes2, val & ~0x20);
			}

			// TODO: Update visibility mode of unit
		}
	}

	void Aura::onTargetMoved(GameObject & /*unused*/, float oldX, float oldY, float oldZ, float oldO)
	{
		// Determine flags
		const bool removeOnMove = (m_spell.auraInterruptFlags & spell_aura_interrupt_flags::Move) != 0;
		const bool removeOnTurn = (m_spell.auraInterruptFlags & spell_aura_interrupt_flags::Turning) != 0;

		float x, y, z, o;
		m_target.getLocation(x, y, z, o);

		if (removeOnMove)
		{
			if (x != oldX || y != oldY || z != oldZ)
			{
				// Moved - remove!
				DLOG("TODO: Remove aura due to move");
				return;
			}
		}

		if (removeOnTurn)
		{
			if (o != oldO)
			{
				// Turned - remove!
				DLOG("TODO: Remove aura due to turning");
				return;
			}
		}
	}

	void Aura::onForceRemoval()
	{
		setRemoved(nullptr);
	}

	void Aura::setRemoved(GameUnit *remover)
	{
		std::shared_ptr<Aura> strongThis = shared_from_this();
		m_post([strongThis]
		{
			// TODO: Notify about being removed...
		});
	}

}
