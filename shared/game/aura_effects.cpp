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
#include "game_creature.h"
#include "each_tile_in_sight.h"
#include "world_instance.h"
#include "binary_io/vector_sink.h"
#include "game_protocol/game_protocol.h"
#include "universe.h"
#include "log/default_log_levels.h"
#include "shared/proto_data/items.pb.h"
#include "shared/proto_data/classes.pb.h"
#include "experience.h"

namespace wowpp
{

	void Aura::handleModNull(bool apply)
	{
		// Nothing to do here
		DLOG("AURA_TYPE_MOD_NULL: Nothing to do");
	}

	void Aura::handlePeriodicDamage(bool apply)
	{
		if (apply)
		{
			handlePeriodicBase();
		}
	}

	void Aura::handleDummy(bool apply)
	{
		if (isSealSpell(m_spell))
		{
			m_target.modifyAuraState(game::aura_state::Judgement, apply);
		}
	}

	void Aura::handleModConfuse(bool apply)
	{
		m_target.notifyConfusedChanged();
	}

	void Aura::handleModFear(bool apply)
	{
		m_target.notifyFearChanged();
	}

	void Aura::handlePeriodicHeal(bool apply)
	{
		if (apply)
		{
			handlePeriodicBase();
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
		if (!m_target.isGameCharacter())
		{
			return;
		}

		//TODO: apply physical dmg (attack power)?

		for (UInt8 school = 1; school < 7; ++school)
		{
			if ((m_effect.miscvaluea() & (1 << school)) != 0)
			{
				UInt32 spellDmgPos = m_target.getUInt32Value(character_fields::ModDamageDonePos + school);
				UInt32 spellDmgNeg = m_target.getUInt32Value(character_fields::ModDamageDoneNeg + school);
				float spellDmgPct = m_target.getFloatValue(character_fields::ModDamageDonePct + school);
				Int32 spellDmg = (spellDmgPos - spellDmgNeg) * spellDmgPct + (apply ? m_basePoints : -m_basePoints);

				m_target.setUInt32Value(character_fields::ModDamageDonePos + school, spellDmg > 0 ? spellDmg : 0);
				m_target.setUInt32Value(character_fields::ModDamageDoneNeg + school, spellDmg < 0 ? spellDmg : 0);
			}
		}
	}

	void Aura::handleModDamageTaken(bool apply)
	{
		//TODO
	}

	void Aura::handleDamageShield(bool apply)
	{
		if (apply)
		{
			m_onTakenAutoAttack = m_caster->spellProcEvent.connect(
				[this](bool isVictim, GameUnit *target, UInt32 procFlag, UInt32 procEx, const proto::SpellEntry *procSpell, UInt32 amount, UInt8 attackType, bool canRemove) {
				if (procFlag & (game::spell_proc_flags::TakenMeleeAutoAttack | game::spell_proc_flags::TakenDamage))
				{
					handleDamageShieldProc(target);
				}
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

	void Aura::handleObsModHealth(bool apply)
	{
		if (apply)
		{
			handlePeriodicBase();
		}
	}

	void Aura::handleObsModMana(bool apply)
	{
		if (apply)
		{
			handlePeriodicBase();
		}
	}

	void Aura::handleModResistance(bool apply)
	{
		bool isAbility = m_spell.attributes(0) & game::spell_attributes::Ability;

		// Apply all resistances
		for (UInt8 i = 0; i < 7; ++i)
		{
			if (m_effect.miscvaluea() & Int32(1 << i))
			{
				m_target.updateModifierValue(UnitMods(unit_mods::ResistanceStart + i), isPassive() && isAbility ? unit_mod_type::BaseValue : unit_mod_type::TotalValue, m_basePoints, apply);
			}
		}
	}

	void Aura::handlePeriodicTriggerSpell(bool apply)
	{
		if (apply)
		{
			handlePeriodicBase();
		}
	}

	void Aura::handlePeriodicEnergize(bool apply)
	{
		if (apply)
		{
			handlePeriodicBase();
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

		bool isAbility = m_spell.attributes(0) & game::spell_attributes::Ability;

		// Apply all stats
		for (Int32 i = 0; i < 5; ++i)
		{
			if (stat < 0 || stat == i)
			{
				m_target.updateModifierValue(GameUnit::getUnitModByStat(i), isPassive() && isAbility ? unit_mod_type::BaseValue : unit_mod_type::TotalValue, m_basePoints, apply);
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
			const bool isAlliance = m_target.getRace() == 0 ? true :
				((game::race::Alliance & (1 << (m_target.getRace() - 1))) == (1 << (m_target.getRace() - 1)));

			UInt32 modelId = 0;
			UInt32 powerType = m_target.getByteValue(unit_fields::Bytes0, 3);
			switch (form)
			{
				case game::shapeshift_form::Cat:
				{
					modelId = (isAlliance ? 892 : 8571);
					powerType = game::power_type::Energy;
					break;
				}
				case game::shapeshift_form::Tree:
				{
					modelId = 864;
					break;
				}
				case game::shapeshift_form::Travel:
				{
					modelId = 632;
					break;
				}
				case game::shapeshift_form::Aqua:
				{
					modelId = 2428;
					break;
				}
				case game::shapeshift_form::Bear:
				case game::shapeshift_form::DireBear:
				{
					modelId = (isAlliance ? 2281 : 2289);
					powerType = game::power_type::Rage;
					break;
				}
				case game::shapeshift_form::Ghoul:
				{
					if (isAlliance) {
						modelId = 10045;
					}
					break;
				}
				case game::shapeshift_form::CreatureBear:
				{
					modelId = 902;
					break;
				}
				case game::shapeshift_form::GhostWolf:
				{
					modelId = 4613;
					break;
				}
				case game::shapeshift_form::BattleStance:
				case game::shapeshift_form::DefensiveStance:
				case game::shapeshift_form::BerserkerStance:
				{
					powerType = game::power_type::Rage;
					break;
				}
				case game::shapeshift_form::FlightEpic:
				{
					modelId = (isAlliance ? 21243 : 21244);
					break;
				}
				case game::shapeshift_form::Flight:
				{
					modelId = (isAlliance ? 20857 : 20872);
					break;
				}
				case game::shapeshift_form::Stealth:
				{
					powerType = game::power_type::Energy;
					break;
				}
				case game::shapeshift_form::Moonkin:
				{
					modelId = (isAlliance ? 15374 : 15375);
					break;
				}
			}

			if (modelId != 0)
			{
				m_target.setUInt32Value(unit_fields::DisplayId, modelId);
			}

			m_target.setByteValue(unit_fields::Bytes2, 3, form);

			// Reset rage and energy if power type changed. This also prevents rogues from loosing their
			// energy when entering or leaving stealth mode
			if (m_target.getByteValue(unit_fields::Bytes0, 3) != powerType)
			{
				m_target.setByteValue(unit_fields::Bytes0, 3, powerType);
				m_target.setUInt32Value(unit_fields::Power2, 0);
				m_target.setUInt32Value(unit_fields::Power4, 0);
			}

			// Talent procs
			switch (form)
			{
				// Druid: Furor
				case game::shapeshift_form::Cat:
				case game::shapeshift_form::Bear:
				case game::shapeshift_form::DireBear:
				{
					UInt32 ProcChance = 0;
					m_target.getAuras().forEachAuraOfType(game::aura_type::Dummy, [&ProcChance](Aura &aura) -> bool
					{
						switch (aura.getSpell().id())
						{
							// Furor ranks
							case 17056:	// Rank 1
							case 17058:	// Rank 2
							case 17059:	// Rank 3
							case 17060:	// Rank 4
							case 17061:	// Rank 5
								ProcChance = aura.getBasePoints();
								return false;
							default:
								return true;
						}
					});

					if (ProcChance > 0)
					{
						std::uniform_int_distribution<UInt32> procDist(1, 100);
						if (procDist(randomGenerator) <= ProcChance)
						{
							if (form == 1)	// Cat
							{
								SpellTargetMap targetMap;
								targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
								targetMap.m_unitTarget = m_target.getGuid();
								m_target.castSpell(targetMap, 17099, { 0, 0, 0 }, 0, true);
							}
							else	// Bears
							{
								SpellTargetMap targetMap;
								targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
								targetMap.m_unitTarget = m_target.getGuid();
								m_target.castSpell(targetMap, 17057, { 0, 0, 0 }, 0, true);
							}
						}
					}
				}
				break;
				// TODO: Warrior
				case game::shapeshift_form::BattleStance:
				case game::shapeshift_form::DefensiveStance:
				case game::shapeshift_form::BerserkerStance:
					m_target.setUInt32Value(unit_fields::Power2, 0);
					break;
			}
		}
		else
		{
			m_target.setUInt32Value(unit_fields::DisplayId, m_target.getUInt32Value(unit_fields::NativeDisplayId));
			if (m_target.getByteValue(unit_fields::Bytes0, 3) != m_target.getClassEntry()->powertype())
			{
				m_target.setByteValue(unit_fields::Bytes0, 3, m_target.getClassEntry()->powertype());
				m_target.setUInt32Value(unit_fields::Power2, 0);
				m_target.setUInt32Value(unit_fields::Power4, 0);
			}
			m_target.setByteValue(unit_fields::Bytes2, 3, 0);
		}

		m_target.updateAllStats();

		// TODO: We need to cast some additional spells here, or remove some auras
		// based on the form (for example, armor and stamina bonus in bear form)
		UInt32 spell1 = 0, spell2 = 0;
		switch (form)
		{
			case game::shapeshift_form::Cat:
				spell1 = 3025;
				break;
			case game::shapeshift_form::Tree:
				spell1 = 5420;
				spell2 = 34123;
				break;
			case game::shapeshift_form::Travel:
				spell1 = 5419;
				break;
			case game::shapeshift_form::Bear:
				spell1 = 1178;
				spell2 = 21178;
				break;
			case game::shapeshift_form::DireBear:
				spell1 = 9635;
				spell2 = 21178;
				break;
			case game::shapeshift_form::BattleStance:
				spell1 = 21156;
				break;
			case game::shapeshift_form::DefensiveStance:
				spell1 = 7376;
				break;
			case game::shapeshift_form::BerserkerStance:
				spell1 = 7381;
				break;
			case game::shapeshift_form::Moonkin:
				spell1 = 24905;
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
				m_target.castSpell(targetMap, spell1, { 0, 0, 0 }, 0, true);
			}
			if (spell2 != 0) {
				m_target.castSpell(targetMap, spell2, { 0, 0, 0 }, 0, true);
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

	void Aura::handleModParryPercent(bool apply)
	{
		m_target.updateParryPercentage();
	}

	void Aura::handleModDodgePercent(bool apply)
	{
		m_target.updateDodgePercentage();
	}

	void Aura::handleModCritPercent(bool apply)
	{
		if (m_target.isGameCharacter())
		{
			auto &character = reinterpret_cast<GameCharacter&>(m_target);

			// 1 for each attack type
			for (UInt8 i = 0; i < 3; ++i)
			{
				std::shared_ptr<GameItem> item = character.getInventory().getWeaponByAttackType(static_cast<game::WeaponAttack>(i), true, false);

				if (item)
				{
					character.applyWeaponCritMod(item, static_cast<game::WeaponAttack>(i), m_spell, static_cast<float>(m_basePoints), apply);
				}
			}

			if (m_spell.itemclass() == -1)
			{
				character.handleBaseCRMod(base_mod_group::CritPercentage, base_mod_type::Flat, static_cast<float>(m_basePoints), apply);
				character.handleBaseCRMod(base_mod_group::OffHandCritPercentage, base_mod_type::Flat, static_cast<float>(m_basePoints), apply);
				character.handleBaseCRMod(base_mod_group::RangedCritPercentage, base_mod_type::Flat, static_cast<float>(m_basePoints), apply);
			}
		}
	}

	void Aura::handlePeriodicLeech(bool apply)
	{
		if (apply)
		{
			handlePeriodicBase();
		}
	}

	void Aura::handleTransform(bool apply)
	{
		if (apply)
		{
			if (m_effect.miscvaluea() != 0)
			{
				const auto *unit = m_target.getProject().units.getById(m_effect.miscvaluea());
				if (unit)
				{
					m_target.setUInt32Value(unit_fields::DisplayId, unit->malemodel());
					m_target.setFloatValue(object_fields::ScaleX, unit->scale());
				}
			}
		}
		else
		{
			m_target.setUInt32Value(unit_fields::DisplayId,
				m_target.getUInt32Value(unit_fields::NativeDisplayId));

			float defaultScale = 1.0f;
			if (m_target.isCreature())
			{
				defaultScale = reinterpret_cast<GameCreature&>(m_target).getEntry().scale();
			}
			m_target.setFloatValue(object_fields::ScaleX, defaultScale);
		}
	}

	void Aura::handleModCastingSpeed(bool apply)
	{
		float castSpeed = m_target.getFloatValue(unit_fields::ModCastSpeed);
		float amount = apply ? (100.0f - m_basePoints) / 100.0f : 100.0f / (100.0f - m_basePoints);

		m_target.setFloatValue(unit_fields::ModCastSpeed, castSpeed * amount);
	}

	void Aura::handleModHealingPct(bool apply)
	{
		//TODO
	}

	void Aura::handleModTargetResistance(bool apply)
	{
		if (m_target.isGameCharacter() && (m_effect.miscvaluea() & game::spell_school_mask::Normal))
		{
			Int32 value = m_target.getInt32Value(character_fields::ModTargetPhysicalResistance);
			value += (apply ? m_basePoints : -m_basePoints);
			m_target.setInt32Value(character_fields::ModTargetPhysicalResistance, value);
		}

		if (m_target.isGameCharacter() && (m_effect.miscvaluea() & game::spell_school_mask::Spell))
		{
			Int32 value = m_target.getInt32Value(character_fields::ModTargetResistance);
			value += (apply ? m_basePoints : -m_basePoints);
			m_target.setInt32Value(character_fields::ModTargetResistance, value);
		}
	}

	void Aura::handleModEnergyPercentage(bool apply)
	{
		Int32 powerType = m_effect.miscvaluea();
		if (powerType < 0 || powerType >= game::power_type::Count_ - 1)
		{
			WLOG("AURA_TYPE_MOD_ENERGY_PERCENTAGE: Invalid power type" << stat << " - skipped");
			return;
		}

		// Apply energy
		m_target.updateModifierValue(UnitMods(unit_mods::PowerStart + powerType), unit_mod_type::TotalPct, m_basePoints, apply);
		m_target.updateMaxPower(game::PowerType(powerType));
	}

	void Aura::handleModHealthPercentage(bool apply)
	{
		m_target.updateModifierValue(UnitMods(unit_mods::Health), unit_mod_type::TotalPct, m_basePoints, apply);
		m_target.updateMaxHealth();
	}

	void Aura::handleModManaRegenInterrupt(bool apply)
	{
		if (m_target.isGameCharacter())
		{
			m_target.updateManaRegen();
		}
	}

	void Aura::handleModHealingDone(bool apply)
	{
		if (!m_target.isGameCharacter())
		{
			return;
		}

		UInt32 spellHeal = m_target.getUInt32Value(character_fields::ModHealingDonePos);
		m_target.setUInt32Value(character_fields::ModHealingDonePos, spellHeal + (apply ? m_basePoints : -m_basePoints));
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

	void Aura::handleModHaste(bool apply)
	{
		m_target.updateModifierValue(unit_mods::AttackSpeed, unit_mod_type::BasePct, -m_basePoints, apply);
	}

	void Aura::handleModRangedHaste(bool apply)
	{
		m_target.updateModifierValue(unit_mods::AttackSpeedRanged, unit_mod_type::BasePct, -m_basePoints, apply);
	}

	void Aura::handleModRangedAmmoHaste(bool apply)
	{
		m_target.updateModifierValue(unit_mods::AttackSpeedRanged, unit_mod_type::TotalPct, -m_basePoints, apply);
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

	void Aura::handleModResistanceOfStatPercent(bool apply)
	{
		if (!m_target.isGameCharacter())
		{
			return;
		}

		m_target.updateArmor();
	}

	void Aura::handleModRating(bool apply)
	{
		if (!m_target.isGameCharacter())
		{
			return;
		}

		for (UInt32 rating = 0; rating < combat_rating::End; ++rating)
		{
			if (m_effect.miscvaluea() & (1 << rating))
				reinterpret_cast<GameCharacter&>(m_target).applyCombatRatingMod(static_cast<CombatRatingType>(rating), m_basePoints, apply);
		}
	}

	void Aura::handleFly(bool apply)
	{
		// Determined to prevent falling when one aura is still left
		const bool hasFlyAura = m_target.getAuras().hasAura(game::aura_type::Fly);

		if (m_target.isCreature())
		{
			if (!apply && !hasFlyAura)
			{
				m_target.setFlightMode(apply);
			}
		}

		auto *world = m_target.getWorldInstance();
		if (world)
		{
			if (apply)
			{
				world->sendPacketToNearbyPlayers(m_target, std::bind(game::server_write::moveSetCanFly, std::placeholders::_1, m_target.getGuid()));
			}
			else if (!hasFlyAura)
			{
				world->sendPacketToNearbyPlayers(m_target, std::bind(game::server_write::moveUnsetCanFly, std::placeholders::_1, m_target.getGuid()));
			}
		}
	}

	void Aura::handleModAttackPower(bool apply)
	{
		bool isAbility = m_spell.attributes(0) & game::spell_attributes::Ability;

		if (isPassive() && isAbility)
		{
			m_target.updateModifierValue(unit_mods::AttackPower, unit_mod_type::BaseValue, m_basePoints, apply);
		}
		else
		{
			m_target.updateModifierValue(unit_mods::AttackPower, unit_mod_type::TotalValue, m_basePoints, apply);
		}
	}

	void Aura::handleModResistancePct(bool apply)
	{
		// Apply all resistances
		for (UInt8 i = 0; i < 7; ++i)
		{
			if (m_effect.miscvaluea() & Int32(1 << i))
			{
				m_target.updateModifierValue(UnitMods(unit_mods::ResistanceStart + i), unit_mod_type::TotalPct, m_basePoints, apply);
			}
		}
	}

	void Aura::handleModTotalThreat(bool apply)
	{
		//TODO
	}

	void Aura::handleWaterWalk(bool apply)
	{
		auto *world = m_target.getWorldInstance();
		if (world)
		{
			if (apply)
			{
				world->sendPacketToNearbyPlayers(m_target, std::bind(game::server_write::moveWaterWalk, std::placeholders::_1, m_target.getGuid()));
			}
			else
			{
				world->sendPacketToNearbyPlayers(m_target, std::bind(game::server_write::moveLandWalk, std::placeholders::_1, m_target.getGuid()));
			}
		}
	}

	void Aura::handleFeatherFall(bool apply)
	{
		auto *world = m_target.getWorldInstance();
		if (world)
		{
			if (apply)
			{
				world->sendPacketToNearbyPlayers(m_target, std::bind(game::server_write::moveFeatherFall, std::placeholders::_1, m_target.getGuid()));
			}
			else
			{
				world->sendPacketToNearbyPlayers(m_target, std::bind(game::server_write::moveNormalFall, std::placeholders::_1, m_target.getGuid()));
			}
		}
	}

	void Aura::handleHover(bool apply)
	{
		auto *world = m_target.getWorldInstance();
		if (world)
		{
			if (apply)
			{
				world->sendPacketToNearbyPlayers(m_target, std::bind(game::server_write::moveSetHover, std::placeholders::_1, m_target.getGuid()));
			}
			else
			{
				world->sendPacketToNearbyPlayers(m_target, std::bind(game::server_write::moveUnsetHover, std::placeholders::_1, m_target.getGuid()));
			}
		}
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

	void Aura::handleModPowerCostSchoolPct(bool apply)
	{
		const float amount = m_basePoints / 100.0f;
		for (UInt8 i = 0; i < 7; ++i)
		{
			if (m_effect.miscvaluea() & (1 << i)) {
				float value = m_target.getFloatValue(unit_fields::PowerCostMultiplier + i);
				value += (apply ? amount : -amount);

				m_target.setFloatValue(unit_fields::PowerCostMultiplier + i, value);
			}
		}
	}

	void Aura::handleMechanicImmunity(bool apply)
	{
		UInt32 mask = m_effect.miscvaluea();
		//if (mask == 0) mask = m_spell.mechanic();

		if (apply)
		{
			m_target.addMechanicImmunity(1 << mask);
		}
		else
		{
			// TODO: We need to check if there are still other auras which provide the same immunity
			m_target.removeMechanicImmunity(1 << mask);
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

	void Aura::handleModDamagePercentDone(bool apply)
	{
		if (apply)
		{
			if (m_target.isGameCharacter())
			{
				for (UInt8 i = 1; i < 7; ++i)
				{
					m_target.setFloatValue(character_fields::ModDamageDonePct + i, m_basePoints / 100.0f);
				}
			}
		}
	}

	void Aura::handleModPowerRegen(bool apply)
	{
		if (m_target.isGameCharacter())
		{
			m_target.updateManaRegen();
		}
	}

	void Aura::handleChannelDeathItem(bool apply)
	{
		if (!apply && !m_target.isAlive())
		{
			// Target died, create item for caster
			if (m_caster && m_caster->isGameCharacter())
			{
				// Only reward the caster with a soul shared if targets level isn't too low
				const UInt32 greyLevel = xp::getGrayLevel(m_caster->getLevel());
				if (m_target.getLevel() <= greyLevel) {
					return;
				}

				const auto *itemEntry = m_target.getProject().items.getById(m_effect.itemtype());
				if (!itemEntry)
					return;

				Inventory &inv = reinterpret_cast<GameCharacter&>(*m_caster).getInventory();
				auto result = inv.createItems(*itemEntry, m_basePoints);
				if (result != game::inventory_change_failure::Okay)
				{
					// TODO: Send error message to player?
				}
			}
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

}
