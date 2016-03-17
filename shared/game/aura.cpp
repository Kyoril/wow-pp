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
	Aura::Aura(const proto::SpellEntry &spell, const proto::SpellEffect &effect, Int32 basePoints, GameUnit &caster, GameUnit &target, PostFunction post, std::function<void(Aura &)> onDestroy)
		: m_spell(spell)
		, m_effect(effect)
		, m_caster(&caster)
		, m_target(target)
		, m_tickCount(0)
		, m_applyTime(getCurrentTime())
		, m_basePoints(basePoints)
		, m_procCharges(spell.proccharges())
		, m_expireCountdown(target.getTimers())
		, m_tickCountdown(target.getTimers())
		, m_isPeriodic(false)
		, m_expired(false)
		, m_attackerLevel(caster.getLevel())
		, m_slot(0xFF)
		, m_post(std::move(post))
		, m_destroy(std::move(onDestroy))
		, m_totalTicks(0) //effect.amplitude() == 0 ? 0 : spell.duration() / effect.amplitude())
		, m_duration(spell.duration())
	{
		// Subscribe to caster despawn event so that we don't hold an invalid pointer
		m_casterDespawned = caster.despawned.connect(
		                        std::bind(&Aura::onCasterDespawned, this, std::placeholders::_1));
		m_onExpire = m_expireCountdown.ended.connect(
		                 std::bind(&Aura::onExpired, this));
		m_onTick = m_tickCountdown.ended.connect(
		               std::bind(&Aura::onTick, this));

		// Adjust aura duration
		if (m_caster &&
			m_caster->isGameCharacter())
		{
			reinterpret_cast<GameCharacter*>(m_caster)->applySpellMod(spell_mod_op::Duration, m_spell.id(), m_duration);
		}

		// Adjust amount of total ticks
		m_totalTicks = (m_effect.amplitude() == 0 ? 0 : m_duration / m_effect.amplitude());
	}

	Aura::~Aura()
	{
	}

	void Aura::setBasePoints(Int32 basePoints) {
		m_basePoints = basePoints;

		if (basePoints < 1) {
			m_destroy(*this);
		}
	}

	UInt32 Aura::getEffectSchoolMask()
	{
		UInt32 effectSchoolMask = m_spell.schoolmask();
		if (m_effect.aura() == game::aura_type::ProcTriggerSpell ||
		        m_effect.aura() == game::aura_type::ProcTriggerSpellWithValue ||
		        m_effect.aura() == game::aura_type::AddTargetTrigger)
		{
			auto *triggerSpell = m_caster->getProject().spells.getById(m_effect.triggerspell());
			if (triggerSpell)
			{
				effectSchoolMask = triggerSpell->schoolmask();
			}
		}
		return effectSchoolMask;
	}

	void Aura::handleModifier(bool apply)
	{
		namespace aura = game::aura_type;

		switch (m_effect.aura())
		{
		case aura::None:
			handleModNull(apply);
			break;
		case aura::TrackCreatures:
			handleTrackCreatures(apply);
			break;
		case aura::TrackResources:
			handleTrackResources(apply);
			break;
		case aura::Dummy:
			handleDummy(apply);
			break;
		case aura::ModStat:
			handleModStat(apply);
			break;
		case aura::ModThreat:
			handleModThreat(apply);
			break;
		case aura::ModIncreaseSpeed:
		case aura::ModSpeedAlways:
		//mounted speed
		case aura::ModIncreaseMountedSpeed:
			handleRunSpeedModifier(apply);
			break;
		case aura::ModFlightSpeed:
		case aura::ModFlightSpeedStacking:
		case aura::ModFlightSpeedNotStacking:
			handleFlySpeedModifier(apply);
			break;
		case aura::ModIncreaseSwimSpeed:
			handleSwimSpeedModifier(apply);
			break;
		case aura::ModDecreaseSpeed:
		case aura::ModSpeedNotStack:
			handleRunSpeedModifier(apply);
			handleSwimSpeedModifier(apply);
			handleFlySpeedModifier(apply);
			break;
		case aura::MechanicImmunity:
			handleMechanicImmunity(apply);
			break;
		case aura::Mounted:
			handleMounted(apply);
			break;
		case aura::ModPowerRegen:
			handleModPowerRegen(apply);
			break;
		case aura::ModRegen:
			break;
		case aura::ModTotalStatPercentage:
			handleModTotalStatPercentage(apply);
			break;
		case aura::ModStun:
			handleModStun(apply);
			break;
		case aura::ModRoot:
			handleModRoot(apply);
			break;
		case aura::ModResistance:
			handleModResistance(apply);
			break;
		case aura::PeriodicTriggerSpell:
			handlePeriodicTriggerSpell(apply);
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
		case aura::ModDamageDone:
			handleModDamageDone(apply);
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
		case aura::ModManaRegenInterrupt:
			handleModManaRegenInterrupt(apply);
			break;
		case aura::AddTargetTrigger:
			//are added in applyAura
			break;
		case aura::ModBaseResistancePct:
			handleModBaseResistancePct(apply);
			break;
		case aura::ModResistanceExclusive:
			handleModResistanceExclusive(apply);
			break;
		case aura::PeriodicDummy:
			handlePeriodicDummy(apply);
			break;
		case aura::AddFlatModifier:
		case aura::AddPctModifier:
			handleAddModifier(apply);
			break;
		default:
			//			WLOG("Unhandled aura type: " << m_effect.aura());
			break;
		}
	}

	void Aura::handleProcModifier(game::spell_proc_flags::Type procType, GameUnit *target/* = nullptr*/)
	{
		namespace aura = game::aura_type;

		// Determine if proc should happen
		UInt32 procChance = 0;
		if (m_effect.aura() == aura::AddTargetTrigger)
		{
			procChance = m_effect.basepoints();
			WLOG("triggerProc chance: " << procChance);
		}
		else
		{
			procChance = m_spell.procchance();
		}

		if (procChance < 100)
		{
			std::uniform_int_distribution<UInt32> dist(1, 100);
			UInt32 chanceRoll = dist(randomGenerator);
			if (chanceRoll > procChance) {
				return;
			}
		}

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
		case aura::AddTargetTrigger:
			{
				handleTriggerSpellProc(target);
				break;
			}
		case aura::ModResistance:
			{
				// nothing to do
				break;
			}
		default:
			{
				WLOG("Unhandled aura proc: " << m_effect.aura());
				break;
			}
		}

		if (m_procCharges > 0)
		{
			m_procCharges--;
			if (m_procCharges == 0) {
				m_destroy(*this);
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
		if (!apply) {
			return;
		}

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
		if (!apply) {
			return;
		}


	}

	void Aura::handlePeriodicHeal(bool apply)
	{
		if (!apply) {
			return;
		}

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

	void Aura::handleModThreat(bool apply)
	{
		if (!m_target.isGameCharacter())
		{
			return;
		}

		GameCharacter &character = reinterpret_cast<GameCharacter&>(m_target);
		character.modifyThreatModifier(m_effect.miscvaluea(), static_cast<float>(m_basePoints) / 100.0f, apply);
	}

	void Aura::handleModStun(bool apply)
	{
		m_target.notifyStunChanged();
	}

	void Aura::handleModDamageDone(bool apply)
	{
		if (m_target.isGameCharacter())
		{
			UInt8 schoolMask = m_effect.miscvaluea();
			for (UInt8 i = 1; i < 7; i++)
			{
				if (schoolMask & Int32(1 << i))
				{
					UInt32 bonus = m_target.getUInt32Value(character_fields::ModDamageDonePos + i);
					if (apply) {
						bonus += m_basePoints;
					}
					else {
						bonus -= m_basePoints;
					}

					m_target.setUInt32Value(character_fields::ModDamageDonePos + i, bonus);
				}
			}
		}
	}
	
	void Aura::handleModDamageTaken(bool apply)
	{
		
	}

	void Aura::handleDamageShield(bool apply)
	{
		if (apply)
		{
			m_procTakenAutoAttack = m_caster->takenMeleeAutoAttack.connect(
			[&](GameUnit * victim) {
				handleDamageShieldProc(victim);
			});
		}
	}

	void Aura::handleModStealth(bool apply)
	{
		if (apply)
		{
			m_target.setByteValue(unit_fields::Bytes1, 2, 0x02);
			if (m_target.isGameCharacter())
			{
				UInt32 val = m_target.getUInt32Value(character_fields::CharacterBytes2);
				m_target.setUInt32Value(character_fields::CharacterBytes2, val | 0x20);
			}
		}
		else
		{
			m_target.setByteValue(unit_fields::Bytes1, 2, 0x00);
			if (m_target.isGameCharacter())
			{
				UInt32 val = m_target.getUInt32Value(character_fields::CharacterBytes2);
				m_target.setUInt32Value(character_fields::CharacterBytes2, val & ~0x20);
			}
		}

		m_target.notifyStealthChanged();
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

	void Aura::handlePeriodicTriggerSpell(bool apply)
	{
		float amplitude = m_effect.amplitude() / 1000.0f;
		UInt32 triggerSpell = m_effect.triggerspell();
		SpellTargetMap targetMap;
		m_onTick = m_tickCountdown.ended.connect([this, amplitude, triggerSpell, targetMap]()
		{
			m_target.castSpell(targetMap, triggerSpell, -1, 0, true);

			if (!m_expired)
			{
				startPeriodicTimer();
			}
		});
		startPeriodicTimer();
	}

	void Aura::handlePeriodicEnergize(bool apply)
	{
		if (!apply) {
			return;
		}

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

	void Aura::handleModRoot(bool apply)
	{
		m_target.notifyRootChanged();
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

	void Aura::handleRunSpeedModifier(bool apply)
	{
		m_target.notifySpeedChanged(movement_type::Run);
	}

	void Aura::handleSwimSpeedModifier(bool apply)
	{
		m_target.notifySpeedChanged(movement_type::Swim);
	}

	void Aura::handleFlySpeedModifier(bool apply)
	{
		m_target.notifySpeedChanged(movement_type::Flight);
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
			case 2:			// Tree
				{
					modelId = 864;
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
					if (isAlliance) {
						modelId = 10045;
					}
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
			//				case 17:		// Battle Stance
			//				case 18:		// Defensive Stance
			//				case 19:		// Berserker Stance
			case 27:		// Epic flight form
				{
					modelId = (isAlliance ? 21243 : 21244);
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
		case 2:
			spell1 = 5420;
			spell2 = 34123;
			break;
		case 5:
			spell1 = 1178;
			spell2 = 21178;
			break;
		case 8:
			spell1 = 9635;
			spell2 = 21178;
			break;
		case 17:		// Battle stance
			spell1 = 21156;
			break;
		case 18:		// Defensive stance
			spell1 = 7376;
			break;
		case 19:		// Berserker stance
			spell1 = 7381;
			break;
		}

		auto *world = m_target.getWorldInstance();
		if (!world) {
			return;
		}

		auto strongUnit = m_target.shared_from_this();
		if (apply)
		{
			SpellTargetMap targetMap;
			targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
			targetMap.m_unitTarget = m_target.getGuid();

			if (spell1 != 0) {
				m_target.castSpell(targetMap, spell1, -1, 0, true);
			}
			if (spell2 != 0) {
				m_target.castSpell(targetMap, spell2, -1, 0, true);
			}
		}
		else
		{
			if (spell1 != 0) world->getUniverse().post([spell1, strongUnit]()
			{
				std::dynamic_pointer_cast<GameUnit>(strongUnit)->getAuras().removeAllAurasDueToSpell(spell1);
			});
			if (spell2 != 0) world->getUniverse().post([spell2, strongUnit]()
			{
				std::dynamic_pointer_cast<GameUnit>(strongUnit)->getAuras().removeAllAurasDueToSpell(spell2);
			});
		}
	}

	void Aura::handleTrackCreatures(bool apply)
	{
		// Only affects player characters
		if (!m_target.isGameCharacter())
			return;

		const UInt32 creatureType = UInt32(m_effect.miscvaluea() - 1);
		m_target.setUInt32Value(character_fields::Track_Creatures, 
			apply ? UInt32(UInt32(1) << creatureType) : 0);
	}

	void Aura::handleTrackResources(bool apply)
	{
		// Only affects player characters
		if (!m_target.isGameCharacter())
			return;

		const UInt32 resourceType = UInt32(m_effect.miscvaluea() - 1);
		m_target.setUInt32Value(character_fields::Track_Resources,
			apply ? UInt32(UInt32(1) << resourceType) : 0);
	}

	void Aura::handleModHealingPct(bool apply)
	{
		//TODO
	}

	void Aura::handleModManaRegenInterrupt(bool apply)
	{
		if (m_target.isGameCharacter())
		{
			m_target.updateManaRegen();
		}
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

	void Aura::handleModResistanceExclusive(bool apply)
	{
		handleModResistance(apply);
	}

	void Aura::handleTakenDamage(GameUnit *attacker)
	{

	}

	void Aura::handleDummyProc(GameUnit *victim)
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
			if (m_caster->isGameCharacter())
			{
				GameCharacter *character = reinterpret_cast<GameCharacter *>(m_caster);
				auto weapon = character->getInventory().getItemAtSlot(
				                  Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Mainhand));
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

	void Aura::handleDamageShieldProc(GameUnit *attacker)
	{
		std::uniform_int_distribution<int> distribution(m_effect.basedice(), m_effect.diesides());
		const Int32 randomValue = (m_effect.basedice() >= m_effect.diesides() ? m_effect.basedice() : distribution(randomGenerator));
		UInt32 damage = m_effect.basepoints() + randomValue;
		attacker->dealDamage(damage, m_spell.schoolmask(), &m_target, 0.0f);

		auto *world = attacker->getWorldInstance();
		if (world)
		{
			TileIndex2D tileIndex;
			attacker->getTileIndex(tileIndex);

			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			wowpp::game::OutgoingPacket packet(sink);
			wowpp::game::server_write::spellDamageShield(packet, m_target.getGuid(), attacker->getGuid(), m_spell.id(), damage, m_spell.schoolmask());

			forEachSubscriberInSight(world->getGrid(), tileIndex, [&packet, &buffer](ITileSubscriber & subscriber)
			{
				subscriber.sendPacket(packet, buffer);
			});
		}
	}

	void Aura::handleTriggerSpellProc(GameUnit *target)
	{
		if (!target)
		{
			return;
		}

		if (!m_caster)
		{
			return;
		}

		SpellTargetMap targetMap;
		targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
		targetMap.m_unitTarget = target->getGuid();

		UInt32 triggerSpell = m_effect.triggerspell();
		if (triggerSpell != 0)
		{
			m_target.castSpell(targetMap, m_effect.triggerspell(), -1, 0, true);
		}
		else
		{
			WLOG("WARNING: PROC_TRIGGER_SPELL aura of spell " << m_spell.id() << " does not have a trigger spell provided");
			return;
		}
	}

	void Aura::handleModAttackPower(bool apply)
	{
		m_target.updateModifierValue(unit_mods::AttackPower, unit_mod_type::TotalValue, m_basePoints, apply);
	}

	void Aura::handleAddModifier(bool apply)
	{
		if (m_effect.miscvaluea() >= spell_mod_op::Max_)
		{
			WLOG("Invalid spell mod operation " << m_effect.miscvaluea());
			return;
		}

		if (!m_target.isGameCharacter())
		{
			WLOG("AddFlatModifier only works on GameCharacter!");
			return;
		}

		// Setup spell mod
		SpellModifier mod;
		mod.op = SpellModOp(m_effect.miscvaluea());
		mod.value = m_basePoints;
		mod.type = SpellModType(m_effect.aura());
		mod.spellId = m_spell.id();
		mod.effectId = m_effect.index();
		mod.charges = 0;
		mod.mask = m_effect.affectmask();
		if (mod.mask == 0) mod.mask = m_effect.itemtype();
		if (mod.mask == 0)
		{
			WLOG("INVALID MOD MASK FOR SPELL " << m_spell.id() << " / EFFECT " << m_effect.index());
		}
		reinterpret_cast<GameCharacter&>(m_target).modifySpellMod(mod, apply);
	}

	void Aura::handleSchoolAbsorb(bool apply)
	{
		//ToDo: Add talent modifiers
	}

	void Aura::handleMechanicImmunity(bool apply)
	{
		if (apply)
		{
			m_target.addMechanicImmunity(m_spell.mechanic());
		}
		else
		{
			// TODO: We need to check if there are still other auras which provide the same immunity
			WLOG("TODO");
			m_target.removeMechanicImmunity(m_spell.mechanic());
		}
	}

	void Aura::handleMounted(bool apply)
	{
		if (apply)
		{
			const auto *unitEntry = m_caster->getProject().units.getById(m_effect.miscvaluea());
			m_target.setUInt32Value(unit_fields::MountDisplayId, unitEntry->malemodel());
		}
		else
		{
			m_target.setUInt32Value(unit_fields::MountDisplayId, 0);
		}
	}

	void Aura::handleModPowerRegen(bool apply)
	{
		if (m_target.isGameCharacter())
		{
			m_target.updateManaRegen();
		}
	}

	void Aura::handleManaShield(bool apply)
	{
		//ToDo: Add talent modifiers
	}

	void Aura::handlePeriodicDummy(bool apply)
	{
		// if drinking
		for (int i = 0; i < m_spell.effects_size(); ++i)
		{
			auto effect = m_spell.effects(i);
			if (effect.type() == game::spell_effects::ApplyAura && effect.aura() == game::aura_type::ModPowerRegen)
			{
				float amplitude = m_effect.amplitude() / 1000.0f;
				m_onTick = m_tickCountdown.ended.connect([this, amplitude]()
				{
					Int32 reg = m_basePoints * (amplitude / 5.0f);
					m_target.addPower(game::power_type::Mana, reg);

					if (!m_expired)
					{
						startPeriodicTimer();
					}
				});
				startPeriodicTimer();
				break;
			}
		}
	}

	bool Aura::hasPositiveTarget(const proto::SpellEffect &effect)
	{
		if (effect.targetb() == game::targets::UnitAreaEnemySrc) {
			return false;
		}

		switch (effect.targeta())
		{
		case game::targets::UnitTargetEnemy:
		case game::targets::UnitTargetAny:
		case game::targets::UnitAreaEnemyDst:
			return false;
		default:
			return true;
		}
	}

	bool Aura::isPositive() const
	{
		for (const auto &effect : m_spell.effects())
		{
			if (!isPositive(m_spell, effect)) {
				return false;
			}
		}

		return true;
	}

	bool Aura::isPositive(const proto::SpellEntry &spell, const proto::SpellEffect &effect)
	{
		// Negative attribute
		if (spell.attributes(0) & 0x04000000) {
			return false;
		}

		switch (effect.aura())
		{
		//always positive
		case game::aura_type::PeriodicHeal:
		case game::aura_type::ModThreat:
		case game::aura_type::DamageShield:
		case game::aura_type::ModStealth:
		case game::aura_type::ModStealthDetect:
		case game::aura_type::ModInvisibility:
		case game::aura_type::ModInvisibilityDetection:
		case game::aura_type::ObsModHealth:
		case game::aura_type::ObsModMana:
		case game::aura_type::ReflectSpells:
		case game::aura_type::ProcTriggerDamage:
		case game::aura_type::TrackCreatures:
		case game::aura_type::TrackResources:
		case game::aura_type::ModBlockSkill:
		case game::aura_type::ModDamageDoneCreature:
		case game::aura_type::PeriodicHealthFunnel:
		case game::aura_type::FeignDeath:
		case game::aura_type::SchoolAbsorb:
		case game::aura_type::ExtraAttacks:
		case game::aura_type::ModSpellCritChanceSchool:
		case game::aura_type::ModPowerCostSchool:
		case game::aura_type::ReflectSpellsSchool:
		case game::aura_type::FarSight:
		case game::aura_type::Mounted:
		case game::aura_type::SplitDamagePct:
		case game::aura_type::WaterBreathing:
		case game::aura_type::ModBaseResistance:
		case game::aura_type::ModRegen:
		case game::aura_type::ModPowerRegen:
		case game::aura_type::InterruptRegen:
		case game::aura_type::SpellMagnet:
		case game::aura_type::ManaShield:
		case game::aura_type::ModSkillTalent:
		case game::aura_type::ModMeleeAttackPowerVersus:
		case game::aura_type::ModTotalThreat:
		case game::aura_type::WaterWalk:
		case game::aura_type::FeatherFall:
		case game::aura_type::Hover:
		case game::aura_type::AddTargetTrigger:
		case game::aura_type::AddCasterHitTrigger:
		case game::aura_type::ModRangedDamageTaken:
		case game::aura_type::ModRegenDurationCombat:
		case game::aura_type::Untrackable:
		case game::aura_type::ModOffhandDamagePct:
		case game::aura_type::ModMeleeDamageTaken:
		case game::aura_type::ModMeleeDamageTakenPct:
		case game::aura_type::ModPossessPet:
		case game::aura_type::ModMountedSpeedAlways:
		case game::aura_type::ModRangedAttackPowerVersus:
		case game::aura_type::ModManaRegenInterrupt:
		case game::aura_type::ModRangedAmmoHaste:
		case game::aura_type::RetainComboPoints:
		case game::aura_type::SpiritOfRedemption:
		case game::aura_type::ModResistanceOfStatPercent:
		case game::aura_type::AuraType_222:
		case game::aura_type::PrayerOfMending:
		case game::aura_type::DetectStealth:
		case game::aura_type::ModAOEDamageAvoidance:
		case game::aura_type::ModIncreaseHealth_3:
		case game::aura_type::AuraType_233:
		case game::aura_type::ModSpellDamageOfAttackPower:
		case game::aura_type::ModSpellHealingOfAttackPower:
		case game::aura_type::ModScale_2:
		case game::aura_type::ModExpertise:
		case game::aura_type::ModSpellDamageFromHealing:
		case game::aura_type::ComprehendLanguage:
		case game::aura_type::ModIncreaseHealth_2:
		case game::aura_type::AuraType_261:
			return true;
		//always negative
		case game::aura_type::BindSight:
		case game::aura_type::ModPossess:
		case game::aura_type::ModConfuse:
		case game::aura_type::ModCharm:
		case game::aura_type::ModFear:
		case game::aura_type::ModTaunt:
		case game::aura_type::ModStun:
		case game::aura_type::ModRoot:
		case game::aura_type::ModSilence:
		case game::aura_type::PeriodicLeech:
		case game::aura_type::ModPacifySilence:
		case game::aura_type::PeriodicManaLeech:
		case game::aura_type::ModDisarm:
		case game::aura_type::ModStalked:
		case game::aura_type::ChannelDeathItem:
		case game::aura_type::ModDetectRange:
		case game::aura_type::Ghost:
		case game::aura_type::AurasVisible:
		case game::aura_type::Empathy:
		case game::aura_type::ModTargetResistance:
		case game::aura_type::RangedAttackPowerAttackerBonus:
		case game::aura_type::PowerBurnMana:
		case game::aura_type::ModCritDamageBonusMelee:
		case game::aura_type::AOECharm:
		case game::aura_type::UseNormalMovementSpeed:
		case game::aura_type::ArenaPreparation:
		case game::aura_type::ModDetaunt:
		case game::aura_type::AuraType_223:
		case game::aura_type::ModForceMoveForward:
		case game::aura_type::AuraType_243:
			return false;
		//depends on basepoints (more is better)
		case game::aura_type::ModRangedHaste:
		case game::aura_type::ModBaseResistancePct:
		case game::aura_type::ModResistanceExclusive:
		case game::aura_type::SafeFall:
		case game::aura_type::ResistPushback:
		case game::aura_type::ModShieldBlockValuePct:
		case game::aura_type::ModDetectedRange:
		case game::aura_type::SplitDamageFlat:
		case game::aura_type::ModStealthLevel:
		case game::aura_type::ModWaterBreathing:
		case game::aura_type::ModReputationGain:
		case game::aura_type::PetDamageMulti:
		case game::aura_type::ModShieldBlockValue:
		case game::aura_type::ModAOEAvoidance:
		case game::aura_type::ModHealthRegenInCombat:
		case game::aura_type::ModAttackPowerPct:
		case game::aura_type::ModRangedAttackPowerPct:
		case game::aura_type::ModDamageDoneVersus:
		case game::aura_type::ModCritPercentVersus:
		case game::aura_type::ModSpeedNotStack:
		case game::aura_type::ModMountedSpeedNotStack:
		case game::aura_type::ModSpellDamageOfStatPercent:
		case game::aura_type::ModSpellHealingOfStatPercent:
		case game::aura_type::ModDebuffResistance:
		case game::aura_type::ModAttackerSpellCritChance:
		case game::aura_type::ModFlatSpellDamageVersus:
		case game::aura_type::ModRating:
		case game::aura_type::ModFactionReputationGain:
		case game::aura_type::HasteMelee:
		case game::aura_type::MeleeSlow:
		case game::aura_type::ModIncreaseSpellPctToHit:
		case game::aura_type::ModXpPct:
		case game::aura_type::ModFlightSpeed:
		case game::aura_type::ModFlightSpeedMounted:
		case game::aura_type::ModFlightSpeedStacking:
		case game::aura_type::ModFlightSpeedMountedStacking:
		case game::aura_type::ModFlightSpeedMountedNotStacking:
		case game::aura_type::ModRangedAttackPowerOfStatPercent:
		case game::aura_type::ModRageFromDamageDealt:
		case game::aura_type::AuraType_214:
		case game::aura_type::HasteSpells:
		case game::aura_type::HasteRanged:
		case game::aura_type::ModManaRegenFromStat:
		case game::aura_type::ModDispelResist:
		//depends on basepoints (less is better)
		case game::aura_type::ModCriticalThreat:
		case game::aura_type::ModAttackerMeleeHitChance:
		case game::aura_type::ModAttackerRangedHitChance:
		case game::aura_type::ModAttackerSpellHitChance:
		case game::aura_type::ModAttackerMeleeCritChance:
		case game::aura_type::ModAttackerRangedCritChance:
		case game::aura_type::ModCooldown:
		case game::aura_type::ModAttackerSpellAndWeaponCritChance:
		case game::aura_type::ModAttackerMeleeCritDamage:
		case game::aura_type::ModAttackerRangedCritDamage:
		case game::aura_type::ModAttackerSpellCritDamage:
		case game::aura_type::MechanicDurationMod:
		case game::aura_type::MechanicDurationModNotStack:
		case game::aura_type::ModDurationOfMagicEffects:
		case game::aura_type::ModCombatResultChance:
		case game::aura_type::ModEnemyDodge:
		//depends
		case game::aura_type::PeriodicDamage:
		case game::aura_type::Dummy:
		case game::aura_type::ModAttackSpeed:
		case game::aura_type::ModDamageDone:
		case game::aura_type::ModDamageTaken:
		case game::aura_type::ModResistance:
		case game::aura_type::PeriodicTriggerSpell:
		case game::aura_type::PeriodicEnergize:
		case game::aura_type::ModPacify:
		case game::aura_type::ModStat:
		case game::aura_type::ModSkill:
		case game::aura_type::ModIncreaseSpeed:
		case game::aura_type::ModIncreaseMountedSpeed:
		case game::aura_type::ModDecreaseSpeed:
		case game::aura_type::ModIncreaseHealth:
		case game::aura_type::ModIncreaseEnergy:
		case game::aura_type::ModShapeShift:
		case game::aura_type::EffectImmunity:
		case game::aura_type::StateImmunity:
		case game::aura_type::SchoolImmunity:
		case game::aura_type::DamageImmunity:
		case game::aura_type::DispelImmunity:
		case game::aura_type::ProcTriggerSpell:
		case game::aura_type::ModParryPercent:
		case game::aura_type::ModDodgePercent:
		case game::aura_type::ModBlockPercent:
		case game::aura_type::ModCritPercent:
		case game::aura_type::ModHitChance:
		case game::aura_type::ModSpellHitChance:
		case game::aura_type::Transform:
		case game::aura_type::ModSpellCritChance:
		case game::aura_type::ModIncreaseSwimSpeed:
		case game::aura_type::ModScale:
		case game::aura_type::ModCastingSpeed:
		case game::aura_type::ModPowerCostSchoolPct:
		case game::aura_type::ModLanguage:
		case game::aura_type::MechanicImmunity:
		case game::aura_type::ModDamagePercentDone:
		case game::aura_type::ModPercentStat:
		case game::aura_type::ModDamagePercentTaken:
		case game::aura_type::ModHealthRegenPercent:
		case game::aura_type::PeriodicDamagePercent:
		case game::aura_type::PreventsFleeing:
		case game::aura_type::ModUnattackable:
		case game::aura_type::ModAttackPower:
		case game::aura_type::ModResistancePct:
		case game::aura_type::AddFlatModifier:
		case game::aura_type::AddPctModifier:
		case game::aura_type::ModPowerRegenPercent:
		case game::aura_type::OverrideClassScripts:
		case game::aura_type::ModHealing:
		case game::aura_type::ModMechanicResistance:
		case game::aura_type::ModHealingPct:
		case game::aura_type::ModRangedAttackPower:
		case game::aura_type::ModSpeedAlways:
		case game::aura_type::ModIncreaseEnergyPercent:
		case game::aura_type::ModIncreaseHealthPercent:
		case game::aura_type::ModHealingDone:
		case game::aura_type::ModHealingDonePct:
		case game::aura_type::ModTotalStatPercentage:
		case game::aura_type::ModHaste:
		case game::aura_type::ForceReaction:
		case game::aura_type::MechanicImmunityMask:
		case game::aura_type::TrackStealthed:
		case game::aura_type::NoPvPCredit:
		case game::aura_type::MeleeAttackPowerAttackerBonus:
		case game::aura_type::DetectAmore:
		case game::aura_type::Fly:
		case game::aura_type::PeriodicDummy:
		case game::aura_type::PeriodicTriggerSpellWithValue:
		case game::aura_type::ProcTriggerSpellWithValue:
		case game::aura_type::AuraType_247:
		default:
			if (hasPositiveTarget(effect)) {
				return true;
			}
			else {
				return false;
			}
		}
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
		if (m_tickCount >= m_totalTicks) {
			return;
		}

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
				UInt32 resisted = damage * (m_target.getResiPercentage(school, *m_caster, false) / 100.0f);
				UInt32 absorbed = m_target.consumeAbsorb(damage - resisted, m_spell.schoolmask());

				// Reduce by armor if physical
				if (school & 1 &&
				        m_effect.mechanic() != 15)	// Bleeding
				{
					m_target.calculateArmorReducedDamage(m_attackerLevel, damage);
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

					forEachSubscriberInSight(world->getGrid(), tileIndex, [&packet, &buffer](ITileSubscriber & subscriber)
					{
						subscriber.sendPacket(packet, buffer);
					});
				}

				// Update health value
				const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);

				float threat = noThreat ? 0.0f : damage - resisted - absorbed;
				if (!noThreat && m_caster->isGameCharacter())
				{
					reinterpret_cast<GameCharacter*>(m_caster)->applySpellMod(spell_mod_op::Threat, m_spell.id(), threat);
				}

				// If spell is channeled, it can cause procs
				if (m_caster &&
				        m_spell.attributes(1) & game::spell_attributes_ex_a::Channeled_1)
				{
					m_caster->doneSpellMagicDmgClassNeg(&m_target, school);
				}
				m_target.dealDamage(damage - resisted - absorbed, school, m_caster, threat);
				break;
			}
		case aura::PeriodicDamagePercent:
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

					forEachSubscriberInSight(world->getGrid(), tileIndex, [&packet, &buffer](ITileSubscriber & subscriber)
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

					forEachSubscriberInSight(world->getGrid(), tileIndex, [&packet, &buffer](ITileSubscriber & subscriber)
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
		if (m_duration > 0)
		{
			// Get spell duration
			m_expireCountdown.setEnd(
			    getCurrentTime() + m_duration);
		}

		if (m_spell.attributes(0) & game::spell_attributes::BreakableByDamage)
		{
			// Delay the signal connection since spells like frost nova also apply damage. This damage
			// is applied AFTER the root aura (which can break by damage) and thus leads to frost nova 
			// breaking it's own root effect immediatly
			m_post([this]() {
				m_onDamageBreak = m_target.takenDamage.connect([this](GameUnit *attacker, UInt32 damage)
				{
					if (!attacker)
						return;

					UInt32 maxDmg = m_target.getLevel() > 8 ? 30 * m_target.getLevel() - 100 : 50;
					float chance = float(damage) / maxDmg * 100.0f;
					if (chance < 100.0f)
					{
						std::uniform_real_distribution<float> roll(0.0f, 99.99f);
						if (chance <= roll(randomGenerator))
						{
							return;
						}
					}

					// Remove aura
					setRemoved(nullptr);
				});
			});
		}

		// Watch for unit's movement if the aura should interrupt in this case
		if ((m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Move) != 0 ||
		        (m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Turning) != 0)
		{
			m_targetMoved = m_target.moved.connect(
			                    std::bind(&Aura::onTargetMoved, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		}

		if ((m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Damage) != 0)
		{
			m_takenDamage = m_target.takenDamage.connect(
			[&](GameUnit * victim, UInt32 damage) {
				setRemoved(victim);
			});
		}

		if ((m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::NotAboveWater) != 0)
		{
			m_targetEnteredWater = m_target.enteredWater.connect(
			[&]() {
				m_destroy(*this);
			});
		}

		if ((m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Attack) != 0)
		{
			m_targetStartedAttacking = m_target.startedAttacking.connect(
			[&]() {
				m_destroy(*this);
			});
		}

		if ((m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Cast) != 0)
		{
			m_targetStartedCasting = m_target.startedCasting.connect(
			[&](const proto::SpellEntry & spell) {
				m_destroy(*this);
			});
		}

		if ((m_spell.attributes(6) & game::spell_attributes_ex_f::IsMountSpell) != 0)
		{
			m_targetStartedAttacking = m_target.startedAttacking.connect(
			[&]() {
				m_destroy(*this);
			});
			m_targetStartedCasting = m_target.startedCasting.connect(
			[&](const proto::SpellEntry & spell) {
				if ((spell.attributes(0) & game::spell_attributes::CastableWhileMounted) == 0) {
					m_destroy(*this);
				}
			});
		}

		if (m_spell.procflags() != game::spell_proc_flags::None)
		{
			if ((m_spell.procflags() & game::spell_proc_flags::TakenDamage) != 0)
			{
				m_takenDamage = m_caster->takenDamage.connect(
				[&](GameUnit * victim, UInt32 damage) {
					handleTakenDamage(victim);
				});
			}

			if ((m_spell.procflags() & game::spell_proc_flags::DoneMeleeAutoAttack) != 0)
			{
				m_procAutoAttack = m_caster->doneMeleeAutoAttack.connect(
				[&](GameUnit * victim) {
					handleProcModifier(game::spell_proc_flags::DoneMeleeAutoAttack, victim);
				});
			}

			if ((m_spell.procflags() & game::spell_proc_flags::TakenMeleeAutoAttack) != 0)
			{
				m_procTakenAutoAttack = m_caster->takenMeleeAutoAttack.connect(
				[&](GameUnit * attacker) {
					handleProcModifier(game::spell_proc_flags::TakenMeleeAutoAttack, attacker);
				});
			}

			if ((m_spell.procflags() & game::spell_proc_flags::DoneSpellMagicDmgClassNeg) != 0)
			{
				m_doneSpellMagicDmgClassNeg = m_caster->doneSpellMagicDmgClassNeg.connect(
				[&](GameUnit * victim, UInt32 schoolMask) {
					if ((schoolMask & getEffectSchoolMask()) != 0)
					{
						handleProcModifier(game::spell_proc_flags::DoneSpellMagicDmgClassNeg, victim);
					}
				});
			}

			if ((m_spell.procflags() & game::spell_proc_flags::Killed) != 0)
			{
				m_procKilled = m_caster->killed.connect(
				[&](GameUnit * killer) {
					handleProcModifier(game::spell_proc_flags::Killed, killer);
				});
			}

			if ((m_spell.procflags() & game::spell_proc_flags::Kill) != 0)
			{
				m_procKill = m_caster->procKilledTarget.connect(
				[&](GameUnit & killed) {
					handleProcModifier(game::spell_proc_flags::Kill, &killed);
				});
			}
		}
		else if (m_effect.aura() == game::aura_type::AddTargetTrigger)
		{
			m_doneSpellMagicDmgClassNeg = m_caster->doneSpellMagicDmgClassNeg.connect(
			[&](GameUnit * victim, UInt32 schoolMask) {
				if ((schoolMask & getEffectSchoolMask()) != 0)
				{
					handleProcModifier(game::spell_proc_flags::DoneSpellMagicDmgClassNeg, victim);
				}
			});
		}

		// Apply modifiers now
		handleModifier(true);
	}

	void Aura::misapplyAura()
	{
		// Cancel countdowns (if running)
		m_tickCountdown.cancel();
		m_expireCountdown.cancel();

		// Remove aura slot
		if (m_slot != 0xFF)
		{
			m_target.setUInt32Value(unit_fields::Aura + m_slot, 0);
		}

		handleModifier(false);

		if ((m_spell.attributes(0) & game::spell_attributes::DisabledWhileActive) != 0)
		{
			// Raise cooldown event
			m_caster->setCooldown(m_spell.id(), m_spell.cooldown() ? m_spell.cooldown() : m_spell.categorycooldown());
			m_caster->cooldownEvent(m_spell.id());
		}
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

	void Aura::onTargetMoved(GameObject & /*unused*/, math::Vector3 oldPosition, float oldO)
	{
		// Determine flags
		const bool removeOnMove = (m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Move) != 0;
		const bool removeOnTurn = (m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Turning) != 0;

		const auto &location = m_target.getLocation();
		float orientation = m_target.getOrientation();

		if (removeOnMove)
		{
			if (location != oldPosition)
			{
				// Moved - remove!
				onForceRemoval();
				return;
			}
		}

		if (removeOnTurn)
		{
			if (orientation != oldO)
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
