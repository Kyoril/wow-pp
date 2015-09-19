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
		case aura::ModStun:
			handleModStun(apply);
			break;
		case aura::ProcTriggerSpell:
			handleProcTriggerSpell(apply);
			break;
		case aura::ModResistance:
			handleModResistance(apply);
			break;
		case aura::ModBaseResistancePct:
			handleModBaseResistancePct(apply);
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
		case aura::PeriodicEnergize:
			handlePeriodicEnergize(apply);
			break;
		default:
			WLOG("Unhandled aura type: " << m_effect.auraName);
			break;
		}
	}

	void Aura::handleModNull(bool apply)
	{
		// Nothing to do here
		DLOG("AURA_TYPE_MOD_NULL: Nothing to do");
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

	void Aura::handleModStun(bool apply)
	{
		if (m_effect.targetA != game::targets::UnitTargetEnemy)
		{
			WLOG("AURA_TYPE_MOD_STUN: Target of type " << m_effect.targetA << " is not allowed!");
			return;
		}
		
		// TODO: prevent movment, attacks and spells
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

	void Aura::handlePeriodicEnergize(bool apply)
	{
		if (!apply)
			return;

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

	void Aura::handleModShapeShift(bool apply)
	{
		UInt8 form = m_effect.miscValueA;
		if (apply)
		{
			const bool isAlliance = ((game::race::Alliance & (1 << (m_target.getRace() - 1))) == (1 << (m_target.getRace() - 1)));

			UInt32 modelId = 0;
			UInt32 powerType = power_type::Mana;
			switch (form)
			{
				case 1:			// Cat
				{
					modelId = (isAlliance ? 892 : 8571);
					powerType = power_type::Energy;
					break;
				}
				case 3:			// Travel
				{
					modelId = 632;
					break;
				}
				case 4:			// Aqua
				{
					modelId = 2428;
					break;
				}
				case 5:			// Bear
				case 8:
				{
					modelId = (isAlliance ? 2281 : 2289);
					powerType = power_type::Rage;
					break;
				}
				case 7:			// Ghoul
				{
					if (isAlliance) modelId = 10045;
					break;
				}
				case 14:		// Creature Bear
				{
					modelId = 902;
					break;
				}
				case 16:		// Ghost wolf
				{
					modelId = 4613;
					break;
				}
				case 29:		// Flight form
				{
					modelId = (isAlliance ? 20857 : 20872);
					break;
				}
				case 31:		// Moonkin
				{
					modelId = (isAlliance ? 15374 : 15375);
					break;
				}
				case 27:		// Epic flight form
				{
					modelId = (isAlliance ? 21243 : 21244);
					break;
				}
			}

			if (modelId != 0)
			{
				m_target.setUInt32Value(unit_fields::DisplayId, modelId);
			}

			if (powerType != power_type::Mana)
			{
				m_target.setByteValue(unit_fields::Bytes0, 3, powerType);
			}

			m_target.setByteValue(unit_fields::Bytes2, 3, form);

			// Reset rage and energy
			m_target.setUInt32Value(unit_fields::Power2, 0);
			m_target.setUInt32Value(unit_fields::Power4, 0);
		}
		else
		{
			m_target.setUInt32Value(unit_fields::DisplayId, m_target.getUInt32Value(unit_fields::NativeDisplayId));
			m_target.setByteValue(unit_fields::Bytes0, 3, m_target.getClassEntry()->powerType);

			m_target.setByteValue(unit_fields::Bytes2, 3, 0);
		}

		m_target.updateAllStats();

		// TODO: We need to cast some additional spells here, or remove some auras
		// based on the form (for example, armor and stamina bonus in bear form)
		UInt32 spell1 = 0, spell2 = 0;
		switch (form)
		{
			case 5:
			{
				spell1 = 1178;
				spell2 = 21178;
				break;
			}
			case 8:
			{
				spell1 = 9635;
				spell2 = 21178;
				break;
			}
		}

		auto *world = m_target.getWorldInstance();
		if (!world)
			return;

		auto strongUnit = m_target.shared_from_this();
		std::weak_ptr<GameObject> weakUnit(strongUnit);

		if (spell1 != 0 || spell2 != 0)
		{
			world->getUniverse().post([apply, spell1, spell2, weakUnit]()
			{
				if (auto ptr = weakUnit.lock())
				{
					GameUnit *unit = dynamic_cast<GameUnit*>(ptr.get());
					if (unit)
					{
						if (apply)
						{
							SpellTargetMap targetMap;
							targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
							targetMap.m_unitTarget = unit->getGuid();

							if (spell1 != 0) unit->castSpell(targetMap, spell1, 0, GameUnit::SpellSuccessCallback());
							if (spell2 != 0) unit->castSpell(targetMap, spell2, 0, GameUnit::SpellSuccessCallback());
						}
						else
						{
							if (spell1 != 0) unit->getAuras().removeAllAurasDueToSpell(spell1);
							if (spell2 != 0) unit->getAuras().removeAllAurasDueToSpell(spell2);
						}
					}
				}
			});
		}
	}

	void Aura::handleProcTriggerSpell(bool apply)
	{
		
		
//		Int32 stat = m_effect.miscValueA;
//		if (stat < -2 || stat > 4)
//		{
//			WLOG("AURA_TYPE_MOD_STAT: Invalid stat index " << stat << " - skipped");
//			return;
//		}
//
//		// Apply all stats
//		for (Int32 i = 0; i < 5; ++i)
//		{
//			if (stat < 0 || stat == i)
//			{
//				m_target.updateModifierValue(GameUnit::getUnitModByStat(i), unit_mod_type::TotalValue, m_basePoints, apply);
//			}
//		}
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
				DLOG("APPLY STAT " << stat << " TOTAL PCT " << m_basePoints);
				m_target.updateModifierValue(GameUnit::getUnitModByStat(i), unit_mod_type::TotalPct, m_basePoints, apply);
			}
		}
	}

	void Aura::handleModBaseResistancePct(bool apply)
	{
		// Apply all resistances
		for (UInt8 i = 0; i < 7; ++i)
		{
			if (m_effect.miscValueA & Int32(1 << i))
			{
				m_target.updateModifierValue(UnitMods(unit_mods::ResistanceStart + i), unit_mod_type::BasePct, m_basePoints, apply);
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
		// Reset caster
		m_casterDespawned.disconnect();
		m_caster = nullptr;
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
				const bool noThreat = ((m_spell.attributesEx[0] & spell_attributes_ex_a::NoThreat) != 0);
				m_target.dealDamage(damage, school, m_caster, noThreat);
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
				UInt32 power = m_basePoints;
				Int32 powerType = m_effect.miscValueA;
				if (powerType < 0 || powerType > 5)
				{
					break;
				}

				const UInt32 maxPower = m_target.getUInt32Value(unit_fields::MaxPower1 + powerType);
				if (maxPower == 0)
				{
					break;
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
					game::server_write::periodicAuraLog(packet, m_target.getGuid(), (m_caster ? m_caster->getGuid() : 0), m_spell.id, m_effect.auraName, powerType, power);

					forEachSubscriberInSight(world->getGrid(), tileIndex, [&packet, &buffer](ITileSubscriber &subscriber)
					{
						subscriber.sendPacket(packet, buffer);
					});
				}

				UInt32 curPower = m_target.getUInt32Value(unit_fields::Power1 + powerType);
				if (curPower + power > maxPower)
				{
					curPower = maxPower;
				}
				else
				{
					curPower += power;
				}
				m_target.setUInt32Value(unit_fields::Power1 + powerType, curPower);
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
				m_target.heal(heal, m_caster);
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
				onForceRemoval();
				return;
			}
		}

		if (removeOnTurn)
		{
			if (o != oldO)
			{
				// Turned - remove!
				onForceRemoval();
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
			strongThis->m_destroy(*strongThis);
		});
	}

}
