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

namespace wowpp
{
	Aura::Aura(const proto::SpellEntry &spell, const proto::SpellEffect &effect, Int32 basePoints, GameUnit &caster, GameUnit &target, SpellTargetMap targetMap, UInt64 itemGuid, bool isPersistent, PostFunction post, std::function<void(Aura &)> onDestroy)
		: m_spell(spell)
		, m_effect(effect)
		, m_target(target)
		, m_tickCount(0)
		, m_applyTime(getCurrentTime())
		, m_basePoints(basePoints)
		, m_procCharges(spell.proccharges())
		, m_expireCountdown(target.getTimers())
		, m_tickCountdown(target.getTimers())
		, m_isPeriodic(false)
		, m_expired(false)
		, m_slot(0xFF)
		, m_post(std::move(post))
		, m_destroy(std::move(onDestroy))
		, m_totalTicks(0)
		, m_duration(spell.duration())
		, m_itemGuid(itemGuid)
		, m_targetMap(targetMap)
		, m_isPersistent(isPersistent)
		, m_stackCount(1)
	{
		m_caster = std::static_pointer_cast<GameUnit>(caster.shared_from_this());

		if (spell.duration() != spell.maxduration() && m_caster->isGameCharacter())
		{
			m_duration += static_cast<Int32>((spell.maxduration() - m_duration) * (std::static_pointer_cast<GameCharacter>(m_caster)->getComboPoints() / 5.0f));
		}

		// Subscribe to caster despawn event so that we don't hold an invalid pointer
		m_onExpire = m_expireCountdown.ended.connect(this, &Aura::onExpired);
		if (!m_isPersistent)
			m_onTick = m_tickCountdown.ended.connect(this, &Aura::onTick);

		// Adjust aura duration
		if (m_caster &&
			m_caster->isGameCharacter())
		{
			auto casterChar = std::static_pointer_cast<GameCharacter>(m_caster);
			casterChar->applySpellMod(spell_mod_op::Duration, m_spell.id(), m_duration);
			if (m_effect.aura() == game::aura_type::PeriodicDamage ||
				m_effect.aura() == game::aura_type::PeriodicDamagePercent ||
				m_effect.aura() == game::aura_type::PeriodicHeal)
			{
				float basePoints = m_basePoints;
				casterChar->applySpellMod(spell_mod_op::Dot, m_spell.id(), basePoints);
				m_basePoints = static_cast<UInt32>(ceilf(basePoints));

				if (m_effect.aura() == game::aura_type::PeriodicHeal)
				{
					ASSERT(m_basePoints >= 0);
					casterChar->applyHealingDoneBonus(
						m_spell.spelllevel(),
						caster.getLevel(),
						(m_effect.amplitude() == 0 ? 0 : m_duration / m_effect.amplitude()),
						reinterpret_cast<UInt32&>(m_basePoints));
				}
			}
		}

		// Adjust amount of total ticks
		m_totalTicks = (m_effect.amplitude() == 0 ? 0 : m_duration / m_effect.amplitude());
	}

	Aura::~Aura()
	{
	}

	void Aura::setBasePoints(Int32 basePoints) {
		m_basePoints = basePoints;
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

	bool Aura::isStealthAura() const
	{
		for (Int32 i = 0; i < m_spell.effects_size(); ++i)
		{
			if (m_spell.effects(i).type() == game::spell_effects::ApplyAura &&
				m_spell.effects(i).aura() == game::aura_type::ModStealth)
			{
				return true;
			}
		}

		return false;
	}

	void Aura::update()
	{
		onTick();
	}

	void Aura::updateStackCount(Int32 points)
	{
		if (m_spell.stackamount() != m_stackCount)
		{
			m_stackCount++;
			updateAuraApplication();

			Int32 basePoints = m_stackCount * points;

			if (basePoints != m_basePoints)
			{
				setBasePoints(basePoints);
			}
		}

		handleModifier(false);

		m_tickCount = 0;

		if (m_duration > 0)
		{
			// Get spell duration
			m_expireCountdown.setEnd(
				getCurrentTime() + m_duration);
		}

		handleModifier(true);
	}

	void Aura::updateAuraApplication()
	{
		UInt32 stackCount = m_procCharges > 0 ? m_procCharges * m_stackCount : m_stackCount;

		UInt32 index = m_slot / 4;
		UInt32 byte = (m_slot % 4) * 8;
		UInt32 val = m_target.getUInt32Value(unit_fields::AuraApplications + index);
		val &= ~(0xFF << byte);
		val |= ((UInt8(stackCount <= 255 ? stackCount - 1 : 255 - 1)) << byte);
		m_target.setUInt32Value(unit_fields::AuraApplications + index, val);
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
		case aura::ChannelDeathItem:
			handleChannelDeathItem(apply);
			break;
		case aura::PeriodicLeech:
			handlePeriodicLeech(apply);
			break;
		case aura::TrackResources:
			handleTrackResources(apply);
			break;
		case aura::Dummy:
			handleDummy(apply);
			break;
		case aura::ModRating:
			handleModRating(apply);
			break;
		case aura::ModFear:
			handleModFear(apply);
			break;
		case aura::ModConfuse:
			handleModConfuse(apply);
			break;
		case aura::ModStat:
			handleModStat(apply);
			break;
		case aura::ModThreat:
			handleModThreat(apply);
			break;
		case aura::Transform:
			handleTransform(apply);
			break;
		case aura::ObsModMana:
			handleObsModMana(apply);
			break;
		case aura::ObsModHealth:
			handleObsModHealth(apply);
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
		case aura::ModDamagePercentDone:
			handleModDamagePercentDone(apply);
			break;
		case aura::ModPowerRegen:
			handleModPowerRegen(apply);
			break;
		case aura::ModRegen:
			break;
		case aura::ModHealingDone:
			handleModHealingDone(apply);
			break;
		case aura::ModTotalStatPercentage:
			handleModTotalStatPercentage(apply);
			break;
		case aura::ModCastingSpeed:
			handleModCastingSpeed(apply);
			break;
		case aura::ModHaste:
			handleModHaste(apply);
			break;
		case aura::ModRangedHaste:
			handleModRangedHaste(apply);
			break;
		case aura::ModTargetResistance:
			handleModTargetResistance(apply);
			break;
		case aura::ModIncreaseEnergyPercent:
			handleModEnergyPercentage(apply);
			break;
		case aura::ModIncreaseHealthPercent:
			handleModHealthPercentage(apply);
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
		case aura::ModResistancePct:
			handleModResistancePct(apply);
			break;
		case aura::ModTotalThreat:
			handleModTotalThreat(apply);
			break;
		case aura::SchoolAbsorb:
			handleSchoolAbsorb(apply);
			break;
		case aura::ModPowerCostSchoolPct:
			handleModPowerCostSchoolPct(apply);
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
			handleAddModifier(apply);
			break;
		case aura::AddPctModifier:
			handleAddModifier(apply);
			break;
		case aura::Fly:
			handleFly(apply);
			break;
		case aura::FeatherFall:
			handleFeatherFall(apply);
			break;
		case aura::Hover:
			handleHover(apply);
			break;
		case aura::WaterWalk:
			handleWaterWalk(apply);
			break;
		case aura::ModResistanceOfStatPercent:
			handleModResistanceOfStatPercent(apply);
			break;
		case aura::ModCritPercent:
			handleModCritPercent(apply);
			break;
		case aura::ModDodgePercent:
			handleModDodgePercent(apply);
			break;
		case aura::ModParryPercent:
			handleModParryPercent(apply);
			break;
		default:
			//			WLOG("Unhandled aura type: " << m_effect.aura());
			break;
		}
	}

	void Aura::handleProcModifier(UInt8 attackType, bool canRemove, UInt32 amount, GameUnit *target/* = nullptr*/)
	{
		namespace aura = game::aura_type;

		// Determine if proc should happen
		UInt32 procChance = 0;
		if (m_effect.aura() == aura::AddTargetTrigger)
		{
			procChance = m_effect.basepoints();
		}
		else
		{
			procChance = m_spell.procchance();
		}

		if (m_spell.procpermin())
		{
			procChance = m_caster->getAttackTime(attackType) * m_spell.procpermin() / 600.0f;
		}

		if (m_caster && m_caster->isGameCharacter())
		{
			std::static_pointer_cast<GameCharacter>(m_caster)->applySpellMod(
				spell_mod_op::ChanceOfSuccess, m_spell.id(), procChance);
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
				handleDummyProc(target, amount);
				break;
			}
		case aura::DamageShield:
			{
				handleDamageShieldProc(target);
				break;
			}
		case aura::ProcTriggerSpell:
			{
				handleTriggerSpellProc(target, amount);
				break;
			}
		case aura::AddTargetTrigger:
			{
				handleTriggerSpellProc(target, amount);
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

		if (m_procCharges > 0 && canRemove)
		{
			m_procCharges--;
			if (m_procCharges == 0) 
			{
				m_destroy(*this);
			}
			else if (m_slot != 0xFF)
			{
				updateAuraApplication();
			}
		}
	}

	void Aura::handleTakenDamage(GameUnit *attacker)
	{

	}

	void Aura::handleDummyProc(GameUnit *victim, UInt32 amount)
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

		game::SpellPointsArray basePoints{};

		if (m_spell.family() == game::spell_family::Mage)
		{
			// Magic Absorption
			if (m_spell.baseid() == 29441)
			{
				if (m_caster->getPowerTypeByUnitMod(unit_mods::Mana) != game::power_type::Mana)
				{
					return;
				}

				UInt32 maxMana = m_caster->getUInt32Value(unit_fields::MaxPower1 + game::power_type::Mana);
				basePoints[0] = m_basePoints * maxMana  / 100;
				target.m_targetMap = game::spell_cast_target_flags::Self;
				target.m_unitTarget = m_caster->getGuid();
			}

			// Ignite
			if (m_spell.baseid() == 11119)
			{
				if (m_spell.id() == 11119)
				{
					basePoints[0] = static_cast<Int32>(0.04 * amount);
				}
				else if (m_spell.id() == 11120)
				{
					basePoints[0] = static_cast<Int32>(0.08 * amount);
				}
				else if (m_spell.id() == 12846)
				{
					basePoints[0] = static_cast<Int32>(0.12 * amount);
				}
				else if (m_spell.id() == 12847)
				{
					basePoints[0] = static_cast<Int32>(0.16 * amount);
				}
				else if (m_spell.id() == 12848)
				{
					basePoints[0] = static_cast<Int32>(0.20 * amount);
				}
			}
		}

		// Cast the triggered spell with custom basepoints value
		if (m_effect.triggerspell() != 0)
		{
			m_caster->castSpell(std::move(target), m_effect.triggerspell(), std::move(basePoints), 0, true);
		}

		/*
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
		}
		*/
	}

	void Aura::handleDamageShieldProc(GameUnit *attacker)
	{
		attacker->dealDamage(m_basePoints, m_spell.schoolmask(), game::DamageType::Indirect, &m_target, 0.0f);

		auto *world = attacker->getWorldInstance();
		if (world)
		{
			TileIndex2D tileIndex;
			attacker->getTileIndex(tileIndex);

			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			wowpp::game::OutgoingPacket packet(sink);
			wowpp::game::server_write::spellDamageShield(packet, m_target.getGuid(), attacker->getGuid(), m_spell.id(), m_basePoints, m_spell.schoolmask());

			forEachSubscriberInSight(world->getGrid(), tileIndex, [&packet, &buffer](ITileSubscriber & subscriber)
			{
				subscriber.sendPacket(packet, buffer);
			});
		}
	}

	void Aura::handleTriggerSpellProc(GameUnit *target, UInt32 amount)
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

		game::SpellPointsArray basePoints{};

		if (m_spell.family() == game::spell_family::Priest)
		{
			// Blessed Recovery
			if (m_spell.baseid() == 27811)
			{
				targetMap.m_targetMap = game::spell_cast_target_flags::Self;
				targetMap.m_unitTarget = m_caster->getGuid();

				basePoints[0] *= amount / 100 / 3;
			}
		}

		UInt32 triggerSpell = m_effect.triggerspell();
		if (triggerSpell != 0)
		{
			m_target.castSpell(targetMap, m_effect.triggerspell(), basePoints, 0, true);
		}
		else
		{
			WLOG("WARNING: PROC_TRIGGER_SPELL aura of spell " << m_spell.id() << " does not have a trigger spell provided");
			return;
		}
	}

	void Aura::calculatePeriodicDamage(UInt32 & out_damage, UInt32 & out_absorbed, UInt32 & out_resisted)
	{
		out_damage = m_basePoints;

		// Calculate absorption and resistance
		out_resisted = out_damage * (m_target.getResiPercentage(m_spell, *m_caster, false) / 100.0f);
		out_absorbed = m_target.consumeAbsorb(out_damage - out_resisted, m_spell.schoolmask());

		// Armor reduction for physical, non-bleeding spells
		if (m_spell.schoolmask() & 1 && m_effect.mechanic() != 15)
			m_target.calculateArmorReducedDamage(m_caster->getLevel(), out_damage);
	}

	void Aura::periodicLeechEffect()
	{
		// If target is immune to this damage school, do nothing
		if (m_spell.schoolmask() != 0 && m_target.isImmune(m_spell.schoolmask()))
			return;

		// Calculate the damage
		UInt32 damage = 0, resisted = 0, absorbed = 0;
		calculatePeriodicDamage(damage, absorbed, resisted);

		// Apply damage bonus
		m_target.applyDamageTakenBonus(m_spell.schoolmask(), m_totalTicks, reinterpret_cast<UInt32&>(damage));

		// Apply damage done bonus
		if (m_caster)
			m_caster->applyDamageDoneBonus(m_spell.schoolmask(), m_totalTicks, damage);

		// Check if target is still alive
		UInt32 targetHp = m_target.getUInt32Value(unit_fields::Health);
		if (targetHp == 0)
			return;

		// Clamp damage to target health value (do not consume more than possible)
		if (damage > targetHp)
			damage = targetHp;

		// Deal damage to the target
		m_target.dealDamage(damage, m_spell.schoolmask(), game::DamageType::Dot, m_caster.get(), damage);

		// Heal the caster for the same amount
		if (m_caster)
			m_caster->heal(damage, m_caster.get());

		// Notify subscribers
		TileIndex2D tileIndex;
		auto *world = m_target.getWorldInstance();
		if (world && m_target.getTileIndex(tileIndex))
		{
			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			wowpp::game::OutgoingPacket packet(sink);
			game::server_write::spellNonMeleeDamageLog(packet, m_target.getGuid(), (m_caster ? m_caster->getGuid() : 0), m_spell.id(), damage, m_spell.schoolmask(), absorbed, resisted, false, 0, false);

			std::vector<char> healBuffer;
			io::VectorSink healSink(healBuffer);
			wowpp::game::OutgoingPacket healPacket(healSink);
			game::server_write::spellHealLog(healPacket, m_caster->getGuid(), (m_caster ? m_caster->getGuid() : 0), m_spell.id(), damage, false);

			forEachSubscriberInSight(world->getGrid(), tileIndex, [&](ITileSubscriber & subscriber)
			{
				subscriber.sendPacket(packet, buffer);
				subscriber.sendPacket(healPacket, healBuffer);
			});
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
		case game::targets::UnitAreaEnemyDst:
		//case game::targets::UnitTargetAny:
		case game::targets::UnitConeEnemy:
		case game::targets::DestDynObjEnemy:
			return false;
		default:
			return true;
		}
	}

	bool Aura::isPositive() const
	{
		return isPositive(m_spell, m_effect);

		/*for (const auto &effect : m_spell.effects())
		{
			if (!isPositive(m_spell, effect)) {
				return false;
			}
		}

		return true;*/
	}

	bool Aura::isPositive(const proto::SpellEntry &spell, const proto::SpellEffect &effect)
	{
		// Passive spells are always considered positive
		if (spell.attributes(0) & game::spell_attributes::Passive)
			return true;

		// Negative attribute
		if (spell.attributes(0) & game::spell_attributes::Negative)
			return false;

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
			return hasPositiveTarget(effect);
		}
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
		if (m_totalTicks > 0 &&
			m_tickCount >= m_totalTicks) {
			return;
		}

		// Prevent this aura from being deleted. This can happen in the
		// dealDamage-Methods, when a creature dies from that damage.
		auto strongThis = shared_from_this();

		// Increase tick counter
		if (m_totalTicks > 0)
		{
			m_tickCount++;
		}

		bool isArea = false;
		for (const auto &effect : m_spell.effects())
		{
			if (effect.type() == game::spell_effects::PersistentAreaAura)
			{
				isArea = true;
			}
		}

		namespace aura = game::aura_type;
		switch (m_effect.aura())
		{
		case aura::PeriodicDamage:
			{
				// HACK: if m_caster is nullptr (because the caster of this aura is longer available in this world instance),
				// we use m_target (the target itself) as the level calculation. This should be used otherwise however.
				UInt32 school = m_spell.schoolmask();
				Int32 damage = m_basePoints;
				UInt32 resisted = damage * (m_target.getResiPercentage(m_spell, *m_caster, false) / 100.0f);
				UInt32 absorbed = m_target.consumeAbsorb(damage - resisted, m_spell.schoolmask());

				// Reduce by armor if physical
				if (school & 1 &&
				    m_effect.mechanic() != 15)	// Bleeding
				{
					m_target.calculateArmorReducedDamage(m_caster->getLevel(), damage);
				}

				// Curse of Agony damage-per-tick calculation
				if (m_spell.family() == game::spell_family::Warlock &&
					m_spell.familyflags() & 0x0000000000000400ULL)
				{
					if (m_tickCount <= 4)
						damage /= 2;
					else if (m_tickCount >= 9)
						damage += (damage + 1) / 2;
				}

				m_target.applyDamageTakenBonus(school, m_totalTicks, reinterpret_cast<UInt32&>(damage));

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
					std::static_pointer_cast<GameCharacter>(m_caster)->applySpellMod(spell_mod_op::Threat, m_spell.id(), threat);
				}

				UInt32 procAttacker = game::spell_proc_flags::DonePeriodic;
				UInt32 procVictim = game::spell_proc_flags::TakenPeriodic;

				if (damage - resisted - absorbed)
				{
					procVictim |= game::spell_proc_flags::TakenDamage;
				}

				m_caster->procEvent(&m_target, procAttacker, procVictim, game::spell_proc_flags_ex::NormalHit, damage - resisted - absorbed, game::weapon_attack::BaseAttack, &m_spell, false /*check this*/);
				m_target.dealDamage(damage - resisted - absorbed, school, game::DamageType::Dot, m_caster.get(), threat);
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
				m_target.applyHealingTakenBonus(m_totalTicks, heal);

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
				m_target.heal(heal, m_caster.get());
				break;
			}
		case aura::ObsModMana:
			{
				if (!m_target.isAlive())
					break;

				// ignore non positive values (can be result apply spellmods to aura damage
				UInt32 amount = getBasePoints() > 0 ? getBasePoints() : 0;
				UInt32 maxPower = m_target.getUInt32Value(unit_fields::MaxPower1);
				if (maxPower == 0)
					break;

				UInt32 currentPower = m_target.getUInt32Value(unit_fields::Power1);
				if (currentPower == maxPower)
					break;

				UInt32 value = UInt32(maxPower * amount / 100);
				
				// Send event to all subscribers in sight
				auto *world = m_target.getWorldInstance();
				if (world)
				{
					TileIndex2D tileIndex;
					m_target.getTileIndex(tileIndex);

					std::vector<char> buffer;
					io::VectorSink sink(buffer);
					wowpp::game::OutgoingPacket packet(sink);
					game::server_write::periodicAuraLog(packet, m_target.getGuid(), (m_caster ? m_caster->getGuid() : 0), m_spell.id(), m_effect.aura(), 0, value);

					forEachSubscriberInSight(world->getGrid(), tileIndex, [&packet, &buffer](ITileSubscriber & subscriber)
					{
						subscriber.sendPacket(packet, buffer);
					});
				}

				m_target.setUInt32Value(unit_fields::Power1,
					std::min(maxPower, currentPower + value));
				break;
			}
		case aura::ObsModHealth:
			{
				if (!m_target.isAlive())
				{
					break;
				}

				// ignore non positive values (can be result apply spellmods to aura damage
				UInt32 amount = getBasePoints() > 0 ? getBasePoints() : 0;
				UInt32 maxHealth = m_target.getUInt32Value(unit_fields::MaxHealth);
				if (maxHealth == 0)
				{
					break;
				}

				UInt32 currentHealth = m_target.getUInt32Value(unit_fields::Health);
				if (currentHealth == maxHealth)
				{
					break;
				}

				UInt32 value = UInt32(maxHealth * amount / 100);

				// Send event to all subscribers in sight
				auto *world = m_target.getWorldInstance();
				if (world)
				{
					TileIndex2D tileIndex;
					m_target.getTileIndex(tileIndex);

					std::vector<char> buffer;
					io::VectorSink sink(buffer);
					wowpp::game::OutgoingPacket packet(sink);
					game::server_write::periodicAuraLog(packet, m_target.getGuid(), (m_caster ? m_caster->getGuid() : 0), m_spell.id(), m_effect.aura(), value);

					forEachSubscriberInSight(world->getGrid(), tileIndex, [&packet, &buffer](ITileSubscriber & subscriber)
					{
						subscriber.sendPacket(packet, buffer);
					});
				}

				m_target.setUInt32Value(unit_fields::Health,
					std::min(maxHealth, currentHealth + value));
				break;
			}
		case aura::PeriodicHealthFunnel:
			{
				DLOG("TODO");
				break;
			}
		case aura::PeriodicLeech:
			periodicLeechEffect();
			break;
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
				SpellTargetMap targetMap;
				if (isArea)
				{
					targetMap = m_targetMap;
				}
				else
				{
					if (m_effect.targeta() == game::targets::UnitCaster)
					{
						targetMap.m_targetMap = game::spell_cast_target_flags::Self;
					}
					else
					{
						targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
						targetMap.m_unitTarget = m_caster->getUInt64Value(unit_fields::ChannelObject);
					}
				}

				m_target.castSpell(targetMap, m_effect.triggerspell(), { 0, 0, 0 }, 0, true);

				if (!m_expired)
				{
					startPeriodicTimer();
				}
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
		if (!m_expired && !m_isPersistent)
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
				m_onDamageBreak = m_target.spellProcEvent.connect(
				[this](bool isVictim, GameUnit *target, UInt32 procFlag, UInt32 procEx, const proto::SpellEntry *procSpell, UInt32 amount, UInt8 attackType, bool canRemove)
				{
					if (!target)
						return;

					if (procSpell && m_spell.id() == procSpell->id()) 
					{
						return;
					}

					if (!(procFlag & game::spell_proc_flags::TakenDamage &&
						procEx & (game::spell_proc_flags_ex::NormalHit | game::spell_proc_flags_ex::CriticalHit)))
					{
						return;
					}

					UInt32 maxDmg = m_target.getLevel() > 8 ? 30 * m_target.getLevel() - 100 : 50;
					float chance = float(amount) / maxDmg * 100.0f;
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

		if ((m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Damage) != 0)
		{
			auto strongThis = shared_from_this();
			std::weak_ptr<Aura> weakThis(strongThis);

			// Subscribe for damage event in next pass, since we don't want to break this aura by it's own damage
			// (this happens for rogue spell Gouge as it deals damage)
			m_post([weakThis]() {
				auto strong = weakThis.lock();
				if (strong)
				{
					strong->m_takenDamage = strong->m_target.takenDamage.connect(
						[strong](GameUnit * victim, UInt32 damage, game::DamageType type) 
						{
							if (type == game::DamageType::Direct)
							{
								strong->setRemoved(victim);
							}
						}
					);
				}
			});
		}

		if ((m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::NotAboveWater) != 0)
		{
			m_targetEnteredWater = m_target.enteredWater.connect(
			[&]() {
				this->setRemoved(nullptr);
			});
		}

		if ((m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Attack) != 0)
		{
			m_targetStartedAttacking = m_target.doneMeleeAttack.connect(
			[&](GameUnit* attacker, game::VictimState) {
				this->setRemoved(attacker);
			});
		}

		if ((m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Cast) != 0)
		{
			auto strongThis = shared_from_this();
			std::weak_ptr<Aura> weakThis(strongThis);

			m_post([weakThis]() {
				auto strong = weakThis.lock();
				if (strong)
				{
					strong->m_targetStartedCasting = strong->m_target.startedCasting.connect(
						[strong](const proto::SpellEntry & spell) {
						if (spell.attributes(1) & game::spell_attributes_ex_a::NotBreakStealth &&
							strong->isStealthAura())
						{
							return;
						}

						strong->setRemoved(nullptr);
					});
				}
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

		if (m_spell.procflags() != game::spell_proc_flags::None ||
			m_spell.proccustomflags() != game::spell_proc_flags::None)
		{
			m_onProc = m_caster->spellProcEvent.connect(
			[this](bool isVictim, GameUnit *target, UInt32 procFlag, UInt32 procEx, const proto::SpellEntry *procSpell, UInt32 amount, UInt8 attackType, bool canRemove) {
				if (checkProc(amount != 0, target, procFlag, procEx, procSpell, attackType, isVictim))
				{
					handleProcModifier(attackType, canRemove, amount, target);
				}
			});
			
			if ((m_spell.procflags() & game::spell_proc_flags::TakenDamage))
			{
				m_takenDamage = m_caster->takenDamage.connect(
				[&](GameUnit * victim, UInt32 damage, game::DamageType type) {
					handleTakenDamage(victim);
				});
			}
			
			if ((m_spell.procflags() & game::spell_proc_flags::Killed))
			{
				m_procKilled = m_caster->killed.connect(
				[&](GameUnit * killer) {
					handleProcModifier(0, true, 0, killer);
				});
			}
			
		}
		else if (m_effect.aura() == game::aura_type::AddTargetTrigger)
		{
			m_onProc = m_caster->spellProcEvent.connect(
			[this](bool isVictim, GameUnit *target, UInt32 procFlag, UInt32 procEx, const proto::SpellEntry *procSpell, UInt32 amount, UInt8 attackType, bool canRemove) {
				if (procSpell && m_spell.family() == procSpell->family() && (m_effect.itemtype() ? m_effect.itemtype() : m_effect.affectmask()) & procSpell->familyflags())
				{
					handleProcModifier(attackType, canRemove, amount, target);
				}
				else
				{
					WLOG("Unhandled AddTargetTrigger aura with id " << m_spell.id());
				}
			});
		}

		// If an aura of this spell may only be applied on one target at a time...
		if (m_spell.attributes(5) & game::spell_attributes_ex_e::SingleTargetSpell)
		{
			auto &trackedAuras = m_caster->getTrackedAuras();

			auto it = trackedAuras.find(m_spell.mechanic());
			if (it != trackedAuras.end())
			{
				if (it->second != &m_target)
				{
					it->second->getAuras().removeAllAurasDueToMechanic(1 << m_spell.mechanic());
				}
			}

			trackedAuras[m_spell.mechanic()] = &m_target;
		}

		// Apply modifiers now
		handleModifier(true);
	}

	void Aura::misapplyAura()
	{
		// Stop watching for these
		m_onExpire.disconnect();
		m_onTick.disconnect();

		// Disconnect signals
		m_onProc.disconnect();
		m_procKilled.disconnect();
		m_takenDamage.disconnect();
		m_targetStartedCasting.disconnect();
		m_targetStartedAttacking.disconnect();
		m_targetEnteredWater.disconnect();
		m_targetMoved.disconnect();
		m_onDamageBreak.disconnect();

		// Do this to prevent the aura from starting another tick just in case (shouldn't happen though)
		m_expired = true;

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

		if (m_spell.attributes(5) & game::spell_attributes_ex_e::SingleTargetSpell)
		{
			m_caster->getTrackedAuras().erase(m_spell.mechanic());
		}

		misapplied();
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

			if (m_slot != 0xFF)
			{
				updateAuraApplication();
			}
		}
	}

	void Aura::onTargetMoved(const math::Vector3 &oldPosition, float oldO)
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
		std::weak_ptr<Aura> weak(strongThis);
		m_post([weak]
		{
			auto strong = weak.lock();
			if (strong)
			{
				strong->m_destroy(*strong);
			}
		});
	}

	bool Aura::checkProc(bool active, GameUnit *target, UInt32 procFlag, UInt32 procEx, proto::SpellEntry const *procSpell, UInt8 attackType, bool isVictim)
	{
		UInt32 eventProcFlag;
		if (m_spell.proccustomflags())
		{
			eventProcFlag = m_spell.proccustomflags();
		}
		else
		{
			eventProcFlag = m_spell.procflags();
		}

		if (procSpell && procSpell->id() == m_spell.id() && !(eventProcFlag & game::spell_proc_flags::TakenPeriodic))
		{
			return false;
		}

		if (!(procFlag & eventProcFlag))
		{
			return false;
		}

		/*
		if (eventProcFlag & (game::spell_proc_flags::Killed | game::spell_proc_flags::Kill | game::spell_proc_flags::DoneTrapActivation))
		{
			return true;
		}
		*/

		if (m_spell.procschool() && !(m_spell.procschool() & game::spell_school_mask::Normal))
		{
			return false;
		}
			
		if (!isVictim && m_caster->isGameCharacter())
		{
			auto casterChar = std::static_pointer_cast<GameCharacter>(m_caster);
			if (m_spell.itemclass() == game::item_class::Weapon)
			{
				std::shared_ptr<GameItem> item;

				if (attackType == game::weapon_attack::OffhandAttack)
				{
					item = casterChar->getInventory().getItemAtSlot(Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Offhand));
				}
				else if (attackType == game::weapon_attack::RangedAttack)
				{
					item = casterChar->getInventory().getItemAtSlot(Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Ranged));
				}
				else if (attackType == game::weapon_attack::BaseAttack)
				{
						item = casterChar->getInventory().getItemAtSlot(Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Mainhand));
				}

				if (!item || item->getEntry().itemclass() != game::item_class::Weapon || !(m_spell.itemsubclassmask() & (1 << item->getEntry().subclass())))
				{
					return false;
				}
			}
			else if (m_spell.itemclass() == game::item_class::Armor)
			{
				//Shield
				std::shared_ptr<GameItem> item;

				item = casterChar->getInventory().getItemAtSlot(Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Offhand));

				if (!item || item->getEntry().itemclass() != game::item_class::Armor || !(m_spell.itemsubclassmask() & (1 << item->getEntry().subclass())))
				{
					return false;
				}
			}
		}
		if (procSpell)
		{
			if (m_spell.procschool() && !(m_spell.procschool() & procSpell->schoolmask()))
			{
				return false;
			}

			if (m_spell.procfamily() && (m_spell.procfamily() != procSpell->family()))
			{
				return false;
			}
			
			if (m_spell.procfamilyflags () && !(m_spell.procfamilyflags() & procSpell->familyflags()))
			{
				return false;
			}
		}

		if (m_spell.procexflags() != game::spell_proc_flags_ex::None &&
			!(m_spell.procexflags() & game::spell_proc_flags_ex::TriggerAlways))
		{
			if (!(m_spell.procexflags() & procEx))
			{
				return false;
			}
		}

		if (!m_effect.affectmask())
		{
			if (!(m_spell.procexflags() & game::spell_proc_flags_ex::TriggerAlways))
			{
				if (m_spell.procexflags() == game::spell_proc_flags_ex::None)
				{
					if (!((procEx & (game::spell_proc_flags_ex::NormalHit | game::spell_proc_flags_ex::CriticalHit)) && active))
					{
						return false;
					}
				}
				else
				{
					if ((m_spell.procexflags() & (game::spell_proc_flags_ex::NormalHit | game::spell_proc_flags_ex::CriticalHit) & procEx) && !active)
					{
						return false;
					}
				}
			}
		}
		else if (procSpell && !(m_effect.affectmask() & procSpell->familyflags()))
		{
			return false;
		}

		return true;
	}
	
	void wowpp::Aura::handlePeriodicBase()
	{
		// Toggle periodic flag
		m_isPeriodic = true;

		// First tick at apply
		if (m_spell.attributes(5) & game::spell_attributes_ex_e::StartPeriodicAtApply)
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
}
