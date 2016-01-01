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
#include "shared/proto_data/items.pb.h"
#include "shared/proto_data/classes.pb.h"

namespace wowpp
{
	Aura::Aura(const proto::SpellEntry &spell, const proto::SpellEffect &effect, Int32 basePoints, GameUnit &caster, GameUnit &target, PostFunction post, std::function<void(Aura&)> onDestroy)
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
		, m_totalTicks(effect.amplitude() == 0 ? 0 : spell.duration() / effect.amplitude())
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
	
	void Aura::setBasePoints(Int32 basePoints) {
		m_basePoints = basePoints;

		if (basePoints < 1) {
			m_destroy(*this);
		}
	}

	void Aura::handleModifier(bool apply)
	{
		namespace aura = game::aura_type;

		switch (m_effect.aura())
		{
		case aura::None:
			handleModNull(apply);
			break;
		case aura::Dummy:
			handleDummy(apply);
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
		case aura::ProcTriggerSpell:
			//are added in applyAura
			break;
		case aura::DamageShield:
			handleDamageShield(apply);
			break;
		case aura::ModAttackPower:
			handleModAttackPower(apply);
			break;
		case aura::SchoolAbsorb:
			handleSchoolAbsorb(apply);
			break;
		case aura::ManaShield:
			handleManaShield(apply);
			break;
		default:
			WLOG("Unhandled aura type: " << m_effect.aura());
			break;
		}
	}

	void Aura::handleProcModifier(game::spell_proc_flags::Type procType, GameUnit *target/* = nullptr*/)
	{
		// Determine if proc should happen
		if (m_spell.procchance() < 100)
		{
			std::uniform_int_distribution<UInt32> dist(1, 100);
			UInt32 chanceRoll = dist(randomGenerator);
			if (chanceRoll > m_spell.procchance())
				return;
		}

		namespace aura = game::aura_type;

		switch (m_effect.aura())
		{
			case aura::Dummy:
			{
				handleDummyProc(target);
				break;
			}
			case aura::DamageShield:
			{
				handleDamageShieldProc(target);
				break;
			}
			case aura::ProcTriggerSpell:
			{
				handleTriggerSpellProc(target);
				break;
			}
			default:
			{
				WLOG("Unhandled aura proc: " << m_effect.aura());
				break;
			}
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
		if (m_spell.attributes(5) & 0x00000200)
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

	void Aura::handleDummy(bool apply)
	{
		if (!apply)
			return;


	}

	void Aura::handlePeriodicHeal(bool apply)
	{
		if (!apply)
			return;

		// Toggle periodic flag
		m_isPeriodic = true;

		// First tick at apply
		if (m_spell.attributes(5) & 0x00000200)
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
		if (m_effect.targeta() != game::targets::UnitTargetEnemy)
		{
			WLOG("AURA_TYPE_MOD_STUN: Target of type " << m_effect.targeta() << " is not allowed!");
			return;
		}
		
		// TODO: prevent movment, attacks and spells
	}

	void Aura::handleDamageShield(bool apply)
	{
		if (apply)
		{
			m_procTakenAutoAttack = m_caster->takenMeleeAutoAttack.connect(
				[&](GameUnit *victim) {
				handleDamageShieldProc(victim);
			});
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

	void Aura::handleModResistance(bool apply)
	{
		// Apply all resistances
		for (UInt8 i = 0; i < 7; ++i)
		{
			if (m_effect.miscvaluea() & Int32(1 << i))
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
		if (m_spell.attributes(5) & 0x00000200)
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
		Int32 stat = m_effect.miscvaluea();
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
		UInt8 form = m_effect.miscvaluea();
		if (apply)
		{
			const bool isAlliance = ((game::race::Alliance & (1 << (m_target.getRace() - 1))) == (1 << (m_target.getRace() - 1)));

			UInt32 modelId = 0;
			UInt32 powerType = game::power_type::Mana;
			switch (form)
			{
				case 1:			// Cat
				{
					modelId = (isAlliance ? 892 : 8571);
					powerType = game::power_type::Energy;
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
					powerType = game::power_type::Rage;
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

			if (powerType != game::power_type::Mana)
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
			m_target.setByteValue(unit_fields::Bytes0, 3, m_target.getClassEntry()->powertype());

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

							if (spell1 != 0) unit->castSpell(targetMap, spell1, -1, 0, true);
							if (spell2 != 0) unit->castSpell(targetMap, spell2, -1, 0, true);
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
	
	void Aura::handleModHealingPct(bool apply)
	{
		//TODO
	}

	void Aura::handleModTotalStatPercentage(bool apply)
	{
		Int32 stat = m_effect.miscvaluea();
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

	void Aura::handleModBaseResistancePct(bool apply)
	{
		// Apply all resistances
		for (UInt8 i = 0; i < 7; ++i)
		{
			if (m_effect.miscvaluea() & Int32(1 << i))
			{
				m_target.updateModifierValue(UnitMods(unit_mods::ResistanceStart + i), unit_mod_type::BasePct, m_basePoints, apply);
			}
		}
	}
	
	void Aura::handleTakenDamage(GameUnit * attacker)
	{
		
	}

	void Aura::handleDummyProc(GameUnit * victim)
	{
		if (!victim)
		{
			return;
		}

		if (!m_caster)
		{
			return;
		}

		SpellTargetMap target;
		target.m_targetMap = game::spell_cast_target_flags::Unit;
		target.m_unitTarget = victim->getGuid();

		if (m_effect.triggerspell() != 0)
		{
			// TODO: Do this only for Seal of Righteousness, however: I have no clude about
			// how to know when to trigger this, so it might be necessary to hard-code it

			// TODO: Replace these constants
			float weaponSpeed = 0.0f;
			float maxWeaponDmg = 0.0f;
			float minWeaponDmg = 0.0f;
			float scaleFactor = 0.85f;
			if (m_caster->getTypeId() == object_type::Character)
			{
				GameCharacter *character = reinterpret_cast<GameCharacter*>(m_caster);
				auto *weapon = character->getItemByPos(player_inventory_slots::Bag_0, player_equipment_slots::Mainhand);
				if (weapon)
				{
					scaleFactor = 1.2f;
					minWeaponDmg = weapon->getEntry().damage(0).mindmg();
					maxWeaponDmg = weapon->getEntry().damage(0).maxdmg();
					weaponSpeed = weapon->getEntry().delay() / 1000.0f;
				}
			}

			// Calculate damage based on blizzards formula
			const Int32 damage =
				scaleFactor * (m_basePoints * 1.2f * 1.03f * weaponSpeed / 100) + 0.03f * (maxWeaponDmg + minWeaponDmg) / 2 + 1;

			// Cast the triggered spell with custom damage value
			m_caster->castSpell(std::move(target), m_effect.triggerspell(), damage, 0, true);
		}
	}

	void Aura::handleDamageShieldProc(GameUnit * attacker)
	{
		std::uniform_int_distribution<int> distribution(m_effect.basedice(), m_effect.diesides());
		const Int32 randomValue = (m_effect.basedice() >= m_effect.diesides() ? m_effect.basedice() : distribution(randomGenerator));
		UInt32 damage = m_effect.basepoints() + randomValue;
		attacker->dealDamage(damage, m_spell.schoolmask(), &m_target);

		auto *world = attacker->getWorldInstance();
		if (world)
		{
			TileIndex2D tileIndex;
			attacker->getTileIndex(tileIndex);

			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			wowpp::game::OutgoingPacket packet(sink);
			wowpp::game::server_write::spellDamageShield(packet, m_target.getGuid(), attacker->getGuid(), m_spell.id(), damage, m_spell.schoolmask());

			forEachSubscriberInSight(world->getGrid(), tileIndex, [&packet, &buffer](ITileSubscriber &subscriber)
			{
				subscriber.sendPacket(packet, buffer);
			});
		}
	}

	void Aura::handleTriggerSpellProc(GameUnit * attacker)
	{
		if (!attacker)
		{
			return;
		}

		if (!m_caster)
		{
			return;
		}

		SpellTargetMap target;
		target.m_targetMap = game::spell_cast_target_flags::Unit;
		target.m_unitTarget = attacker->getGuid();

		if (m_effect.triggerspell() != 0)
		{
			if (!m_effect.triggerspell())
			{
				WLOG("WARNING: PROC_TRIGGER_SPELL aura of spell " << m_spell.id() << " does not have a trigger spell provided");
				return;
			}

			SpellTargetMap targetMap;
			m_target.castSpell(target, m_effect.triggerspell(), -1, 0, true);
		}
	}

	void Aura::handleModAttackPower(bool apply)
	{
		m_target.updateModifierValue(unit_mods::AttackPower, unit_mod_type::TotalValue, m_basePoints, apply);
	}
	
	void Aura::handleSchoolAbsorb(bool apply)
	{
		//ToDo: Add talent modifiers
	}
	
	void Aura::handleManaShield(bool apply)
	{
		//ToDo: Add talent modifiers
	}

	bool Aura::isPositive() const
	{
		// Negative attribute
		if (m_spell.attributes(0) & 0x04000000)
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
		// No more ticks
		if (m_tickCount >= m_totalTicks)
			return;

		// Prevent this aura from being deleted. This can happen in the
		// dealDamage-Methods, when a creature dies from that damage.
		auto strongThis = shared_from_this();

		// Increase tick counter
		m_tickCount++;

		namespace aura = game::aura_type;
		switch (m_effect.aura())
		{
			case aura::PeriodicDamage:
			{
				UInt32 school = m_spell.schoolmask();
				Int32 damage = m_basePoints;
				UInt32 resisted = 0;
				UInt32 absorbed = m_target.consumeAbsorb(damage, m_spell.schoolmask());
				damage -= absorbed;
				if (damage < 0) damage = 0;

				// Reduce by armor if physical
				if (school & 1 &&
					m_effect.mechanic() != 15)	// Bleeding
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
					game::server_write::periodicAuraLog(packet, m_target.getGuid(), (m_caster ? m_caster->getGuid() : 0), m_spell.id(), m_effect.aura(), damage, school, absorbed, resisted);

					forEachSubscriberInSight(world->getGrid(), tileIndex, [&packet, &buffer](ITileSubscriber &subscriber)
					{
						subscriber.sendPacket(packet, buffer);
					});
				}

				// Update health value
				const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
				// If spell is channeled, it can cause procs
				if (m_caster && 
					m_spell.attributes(1) & game::spell_attributes_ex_a::Channeled_1)
				{
					m_caster->doneSpellMagicDmgClassNeg(&m_target, school);
				}
				m_target.dealDamage(damage, school, m_caster, noThreat);
				m_target.takenDamage(m_caster);
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
				Int32 powerType = m_effect.miscvaluea();
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
					game::server_write::periodicAuraLog(packet, m_target.getGuid(), (m_caster ? m_caster->getGuid() : 0), m_spell.id(), m_effect.aura(), powerType, power);

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
					game::server_write::periodicAuraLog(packet, m_target.getGuid(), (m_caster ? m_caster->getGuid() : 0), m_spell.id(), m_effect.aura(), heal);

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
				WLOG("Unhandled aura type for periodic tick: " << m_effect.aura());
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
		if (m_spell.maxduration() > 0)
		{
			// Get spell duration
			m_expireCountdown.setEnd(
				getCurrentTime() + m_spell.duration());
		}

		// Watch for unit's movement if the aura should interrupt in this case
		if ((m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Move) != 0 ||
			(m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Turning) != 0)
		{
			m_targetMoved = m_target.moved.connect(
				std::bind(&Aura::onTargetMoved, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
		}
		
		if ((m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Damage) != 0)
		{
			m_takenDamage = m_target.takenDamage.connect(
				[&](GameUnit *victim) {
				setRemoved(victim);
			});
		}

		if (m_spell.procflags() != game::spell_proc_flags::None)
		{
			// Melee auto attack
			if ((m_spell.procflags() & game::spell_proc_flags::DoneMeleeAutoAttack) != 0)
			{
				m_procAutoAttack = m_caster->procMeleeAutoAttack.connect(
					[&](GameUnit *victim) {
					handleProcModifier(game::spell_proc_flags::DoneMeleeAutoAttack, victim);
				});
			}

			if ((m_spell.procflags() & game::spell_proc_flags::Killed) != 0)
			{
				m_procKilled = m_caster->killed.connect(
					[&](GameUnit *killer) {
					handleProcModifier(game::spell_proc_flags::Killed, killer);
				});
			}

			if ((m_spell.procflags() & game::spell_proc_flags::Kill) != 0)
			{
				m_procKill = m_caster->procKilledTarget.connect(
					[&](GameUnit &killed) {
					handleProcModifier(game::spell_proc_flags::Kill, &killed);
				});
			}
			
			if ((m_spell.procflags() & game::spell_proc_flags::TakenMeleeAutoAttack) != 0)
			{
				m_procTakenAutoAttack = m_caster->takenMeleeAutoAttack.connect(
					[&](GameUnit *victim) {
					handleProcModifier(game::spell_proc_flags::TakenMeleeAutoAttack, victim);
				});
			}
			
			if ((m_spell.procflags() & game::spell_proc_flags::DoneSpellMagicDmgClassNeg) != 0)
			{
				m_doneSpellMagicDmgClassNeg = m_caster->doneSpellMagicDmgClassNeg.connect(
					[&](GameUnit *victim, UInt32 schoolMask) {
					UInt32 spellSchoolMask = m_spell.schoolmask();
					if (m_effect.aura() == game::aura_type::ProcTriggerSpell ||
						m_effect.aura() == game::aura_type::ProcTriggerSpellWithValue)
					{
						auto *triggerSpell = m_caster->getProject().spells.getById(m_effect.triggerspell());
						if (triggerSpell)
						{
							spellSchoolMask = triggerSpell->schoolmask();
						}
					}

					if ((schoolMask & spellSchoolMask) != 0)
					{
						handleProcModifier(game::spell_proc_flags::DoneSpellMagicDmgClassNeg, victim);
					}
				});
			}
			
			if ((m_spell.procflags() & game::spell_proc_flags::TakenDamage) != 0)
			{
				m_takenDamage = m_caster->takenDamage.connect(
					[&](GameUnit *victim) {
					handleTakenDamage(victim);
				});
			}
		}

		// Apply modifiers now
		handleModifier(true);
	}

	void Aura::startPeriodicTimer()
	{
		// Start timer
		m_tickCountdown.setEnd(
			getCurrentTime() + m_effect.amplitude());
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
		const bool removeOnMove = (m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Move) != 0;
		const bool removeOnTurn = (m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Turning) != 0;

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
