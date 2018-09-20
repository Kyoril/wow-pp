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
#include "aura_effect.h"
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
#include "aura_spell_slot.h"

namespace wowpp
{
	AuraEffect::AuraEffect(AuraSpellSlot &slot, const proto::SpellEffect &effect, Int32 basePoints, GameUnit *caster, GameUnit &target, SpellTargetMap targetMap, bool isPersistent)
		: m_spellSlot(slot)
		, m_effect(effect)
		, m_target(target)
		, m_tickCount(0)
		, m_applyTime(getCurrentTime())
		, m_basePoints(basePoints)
		, m_tickCountdown(target.getTimers())
		, m_isPeriodic(false)
		, m_expired(false)
		, m_totalTicks(0)
		, m_duration(slot.getSpell().duration())
		, m_targetMap(targetMap)
		, m_isPersistent(isPersistent)
	{
		auto &spell = m_spellSlot.getSpell();

		if (caster)
		{
			m_caster = std::static_pointer_cast<GameUnit>(caster->shared_from_this());
			if (spell.duration() != spell.maxduration() && m_caster->isGameCharacter())
			{
				m_duration += static_cast<Int32>((spell.maxduration() - m_duration) * (std::static_pointer_cast<GameCharacter>(m_caster)->getComboPoints() / 5.0f));
			}
		}

		// Subscribe to caster despawn event so that we don't hold an invalid pointer
		if (!m_isPersistent)
			m_onTick = m_tickCountdown.ended.connect(this, &AuraEffect::onTick);

		// Adjust aura duration
		if (m_caster &&
			m_caster->isGameCharacter())
		{
			auto casterChar = std::static_pointer_cast<GameCharacter>(m_caster);
			casterChar->applySpellMod(spell_mod_op::Duration, spell.id(), m_duration);
			if (m_effect.aura() == game::aura_type::PeriodicDamage ||
				m_effect.aura() == game::aura_type::PeriodicDamagePercent ||
				m_effect.aura() == game::aura_type::PeriodicHeal)
			{
				float basePoints = m_basePoints;
				casterChar->applySpellMod(spell_mod_op::Dot, spell.id(), basePoints);
				m_basePoints = static_cast<UInt32>(ceilf(basePoints));

				if (m_effect.aura() == game::aura_type::PeriodicHeal)
				{
					ASSERT(m_basePoints >= 0);
					casterChar->applyHealingDoneBonus(
						m_spellSlot.getSpell().spelllevel(),
						m_caster->getLevel(),
						(m_effect.amplitude() == 0 ? 0 : m_duration / m_effect.amplitude()),
						reinterpret_cast<UInt32&>(m_basePoints));
				}
			}
		}

		// Adjust amount of total ticks
		m_totalTicks = (m_effect.amplitude() == 0 ? 0 : m_duration / m_effect.amplitude());
	}

	void AuraEffect::setBasePoints(Int32 basePoints)
	{
		bool needsReapply = (m_basePoints != basePoints);
		if (needsReapply)
		{
			// First misapply the effect
			handleModifier(false);

			// Update base points now (if we would've done this before, errors would occur
			// because on misapply, stats may be altered based on base points - thus 
			// adding or subtracting more points than we did on apply
			m_basePoints = basePoints;
		}
		
		// Reset the tick count
		m_tickCount = 0;

		// Aura refresh also resets expiration status
		m_expired = false;

		// And now reapply the effect with the new base points
		if (needsReapply)
			handleModifier(true);
	}

	UInt32 AuraEffect::getEffectSchoolMask()
	{
		UInt32 effectSchoolMask = m_spellSlot.getSpell().schoolmask();
		if (m_effect.aura() == game::aura_type::ProcTriggerSpell ||
		        m_effect.aura() == game::aura_type::ProcTriggerSpellWithValue ||
		        m_effect.aura() == game::aura_type::AddTargetTrigger)
		{
			auto *triggerSpell = m_target.getProject().spells.getById(m_effect.triggerspell());
			if (triggerSpell)
			{
				effectSchoolMask = triggerSpell->schoolmask();
			}
		}
		return effectSchoolMask;
	}

	bool AuraEffect::isStealthAura() const
	{
		auto &spell = m_spellSlot.getSpell();
		for (Int32 i = 0; i < spell.effects_size(); ++i)
		{
			if (spell.effects(i).type() == game::spell_effects::ApplyAura &&
				spell.effects(i).aura() == game::aura_type::ModStealth)
			{
				return true;
			}
		}

		return false;
	}

	void AuraEffect::update()
	{
		onTick();
	}

	void AuraEffect::handleModifier(bool apply, bool restoration/* = false*/)
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
		case aura::ModRangedAmmoHaste:
			handleModRangedAmmoHaste(apply);
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
			handleRunSpeedModifier(apply, restoration);
			break;
		case aura::ModFlightSpeed:
		case aura::ModFlightSpeedStacking:
		case aura::ModFlightSpeedNotStacking:
			handleFlySpeedModifier(apply, restoration);
			break;
		case aura::ModFlightSpeedMounted:
			handleModFlightSpeedMounted(apply, restoration);
			break;
		case aura::ModIncreaseSwimSpeed:
			handleSwimSpeedModifier(apply, restoration);
			break;
		case aura::ModDecreaseSpeed:
		case aura::ModSpeedNotStack:
			handleRunSpeedModifier(apply, restoration);
			handleSwimSpeedModifier(apply, restoration);
			handleFlySpeedModifier(apply, restoration);
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

	void AuraEffect::handleProcModifier(UInt8 attackType, bool canRemove, UInt32 amount, const proto::SpellEntry* procSpell/* = nullptr*/, GameUnit *target/* = nullptr*/)
	{
		namespace aura = game::aura_type;

		auto &spell = m_spellSlot.getSpell();

		// Determine if proc should happen
		UInt32 procChance = 0;
		if (m_effect.aura() == aura::AddTargetTrigger)
		{
			procChance = m_effect.basepoints();
		}
		else
		{
			procChance = spell.procchance();
		}

		if (spell.procpermin() && m_caster)
		{
			procChance = m_caster->getAttackTime(attackType) * spell.procpermin() / 600.0f;
		}

		if (m_caster && m_caster->isGameCharacter())
		{
			std::static_pointer_cast<GameCharacter>(m_caster)->applySpellMod(
				spell_mod_op::ChanceOfSuccess, spell.id(), procChance);
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
				handleDummyProc(target, amount, procSpell);
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

		// Remove one proc charge
		if (canRemove)
			m_spellSlot.removeProcCharges(1);
	}

	void AuraEffect::handleTakenDamage(GameUnit *attacker)
	{

	}

	void AuraEffect::handleDummyProc(GameUnit *victim, UInt32 amount, const proto::SpellEntry* procSpell/* = nullptr*/)
	{
		if (!victim)
		{
			return;
		}

		if (!m_caster)
		{
			WLOG(__FUNCTION__ << ": no caster set");
			return;
		}

		SpellTargetMap target;
		target.m_targetMap = game::spell_cast_target_flags::Unit;
		target.m_unitTarget = victim->getGuid();

		game::SpellPointsArray basePoints{};
		auto &spell = m_spellSlot.getSpell();
		if (spell.family() == game::spell_family::Mage)
		{
			// Magic Absorption
			if (spell.baseid() == 29441)
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
			if (spell.baseid() == 11119)
			{
				if (spell.id() == 11119)
				{
					basePoints[0] = static_cast<Int32>(0.04 * amount);
				}
				else if (spell.id() == 11120)
				{
					basePoints[0] = static_cast<Int32>(0.08 * amount);
				}
				else if (spell.id() == 12846)
				{
					basePoints[0] = static_cast<Int32>(0.12 * amount);
				}
				else if (spell.id() == 12847)
				{
					basePoints[0] = static_cast<Int32>(0.16 * amount);
				}
				else if (spell.id() == 12848)
				{
					basePoints[0] = static_cast<Int32>(0.20 * amount);
				}
			}
		}
		else if (spell.family() == game::spell_family::Warrior)
		{
			switch (spell.baseid())
			{
				// Second wind
				case 29834:
					// Don't do anything if this didn't proc by a spell (for example, auto attack)
					if (!procSpell)
						return;

					// No mechanic = irrelevant
					const auto mechanic = static_cast<game::SpellMechanic>(procSpell->mechanic());
					if (mechanic == game::SpellMechanic::None)
						return;

					// The triggering spell has to apply a root and/or stun effect
					if (mechanic != game::SpellMechanic::Stun &&
						mechanic != game::SpellMechanic::Root)
						return;

					break;
			}
		}

		// Cast the triggered spell with custom basepoints value
		if (m_effect.triggerspell() != 0)
			m_caster->castSpell(std::move(target), m_effect.triggerspell(), std::move(basePoints), 0, true);

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

	void AuraEffect::handleDamageShieldProc(GameUnit *attacker)
	{
		attacker->dealDamage(m_basePoints, m_spellSlot.getSpell().schoolmask(), game::DamageType::Indirect, &m_target, 0.0f);

		auto *world = attacker->getWorldInstance();
		if (world)
		{
			TileIndex2D tileIndex;
			attacker->getTileIndex(tileIndex);

			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			wowpp::game::OutgoingPacket packet(sink);
			wowpp::game::server_write::spellDamageShield(packet, m_target.getGuid(), attacker->getGuid(), m_spellSlot.getSpell().id(), m_basePoints, m_spellSlot.getSpell().schoolmask());

			forEachSubscriberInSight(world->getGrid(), tileIndex, [&packet, &buffer](ITileSubscriber & subscriber)
			{
				subscriber.sendPacket(packet, buffer);
			});
		}
	}

	void AuraEffect::handleTriggerSpellProc(GameUnit *target, UInt32 amount)
	{
		if (!target)
		{
			return;
		}

		if (!m_caster)
		{
			WLOG(__FUNCTION__ << ": no caster available");
			return;
		}

		SpellTargetMap targetMap;
		targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
		targetMap.m_unitTarget = target->getGuid();

		game::SpellPointsArray basePoints{};

		if (m_spellSlot.getSpell().family() == game::spell_family::Priest)
		{
			// Blessed Recovery
			if (m_spellSlot.getSpell().baseid() == 27811)
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
			//WLOG("WARNING: PROC_TRIGGER_SPELL aura of spell " << m_spellSlot.getSpell().id() << " does not have a trigger spell provided");
			return;
		}
	}

	void AuraEffect::calculatePeriodicDamage(UInt32 & out_damage, UInt32 & out_absorbed, UInt32 & out_resisted)
	{
		out_damage = m_basePoints;
		
		if (!m_caster)
			WLOG(__FUNCTION__ << ": no caster available!");

		// Calculate absorption and resistance
		out_resisted = m_caster ? out_damage * (m_target.getResiPercentage(m_spellSlot.getSpell(), *m_caster, false) / 100.0f) : 0;
		out_absorbed = m_target.consumeAbsorb(out_damage - out_resisted, m_spellSlot.getSpell().schoolmask());

		// Armor reduction for physical, non-bleeding spells
		if (m_spellSlot.getSpell().schoolmask() & 1 && m_effect.mechanic() != 15 && m_caster)
			m_target.calculateArmorReducedDamage(m_caster->getLevel(), out_damage);
	}

	void AuraEffect::periodicLeechEffect()
	{
		// If target is immune to this damage school, do nothing
		if (m_spellSlot.getSpell().schoolmask() != 0 && m_target.isImmune(m_spellSlot.getSpell().schoolmask()))
			return;

		// Calculate the damage
		UInt32 damage = 0, resisted = 0, absorbed = 0;
		calculatePeriodicDamage(damage, absorbed, resisted);

		// Apply damage bonus
		m_target.applyDamageTakenBonus(m_spellSlot.getSpell().schoolmask(), m_totalTicks, reinterpret_cast<UInt32&>(damage));

		if (!m_caster)
			WLOG(__FUNCTION__ << ": no caster available!");

		// Apply damage done bonus
		if (m_caster)
			m_caster->applyDamageDoneBonus(m_spellSlot.getSpell().schoolmask(), m_totalTicks, damage);

		// Check if target is still alive
		UInt32 targetHp = m_target.getUInt32Value(unit_fields::Health);
		if (targetHp == 0)
			return;

		// Clamp damage to target health value (do not consume more than possible)
		if (damage > targetHp)
			damage = targetHp;

		// Deal damage to the target
		m_target.dealDamage(damage, m_spellSlot.getSpell().schoolmask(), game::DamageType::Dot, m_caster.get(), damage);

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
			game::server_write::spellNonMeleeDamageLog(
				packet, 
				m_target.getGuid(), 
				(m_caster ? m_caster->getGuid() : 0), 
				m_spellSlot.getSpell().id(), 
				damage, 
				m_spellSlot.getSpell().schoolmask(), 
				absorbed, 
				resisted, 
				false, 
				0, 
				false);

			std::vector<char> healBuffer;
			io::VectorSink healSink(healBuffer);
			wowpp::game::OutgoingPacket healPacket(healSink);
			game::server_write::spellHealLog(healPacket, m_caster->getGuid(), (m_caster ? m_caster->getGuid() : 0), m_spellSlot.getSpell().id(), damage, false);

			forEachSubscriberInSight(world->getGrid(), tileIndex, [&](ITileSubscriber & subscriber)
			{
				subscriber.sendPacket(packet, buffer);
				subscriber.sendPacket(healPacket, healBuffer);
			});
		}
	}

	void AuraEffect::onExpired()
	{
		ASSERT(!m_expired && "AuraEffect already expired");

		// Expired
		m_expired = true;

		// Apply last tick if periodic
		if (m_isPeriodic)
		{
			onTick();
		}
	}

	void AuraEffect::onTick()
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
		for (const auto &effect : m_spellSlot.getSpell().effects())
		{
			if (effect.type() == game::spell_effects::PersistentAreaAura)
			{
				isArea = true;
			}
		}

		auto &spell = m_spellSlot.getSpell();

		if (!m_caster)
			WLOG(__FUNCTION__ << ": no caster available!");

		namespace aura = game::aura_type;
		switch (m_effect.aura())
		{
		case aura::PeriodicDamage:
			{
				// HACK: if m_caster is nullptr (because the caster of this aura is longer available in this world instance),
				// we use m_target (the target itself) as the level calculation. This should be used otherwise however.
				UInt32 school = m_spellSlot.getSpell().schoolmask();
				Int32 damage = m_basePoints;
				if (m_caster)
					m_caster->applyDamageDoneBonus(school, m_totalTicks, reinterpret_cast<UInt32&>(damage));

				UInt32 resisted = damage * (m_target.getResiPercentage(spell, *m_caster, false) / 100.0f);
				UInt32 absorbed = m_target.consumeAbsorb(damage - resisted, spell.schoolmask());

				// Reduce by armor if physical
				if (school & 1 &&
				    m_effect.mechanic() != 15)	// Bleeding
				{
					m_target.calculateArmorReducedDamage(m_caster->getLevel(), damage);
				}

				// Curse of Agony damage-per-tick calculation
				if (spell.family() == game::spell_family::Warlock &&
					spell.familyflags() & 0x0000000000000400ULL)
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
					game::server_write::periodicAuraLog(packet, m_target.getGuid(), (m_caster ? m_caster->getGuid() : 0), spell.id(), m_effect.aura(), damage, school, absorbed, resisted);

					forEachSubscriberInSight(world->getGrid(), tileIndex, [&packet, &buffer](ITileSubscriber & subscriber)
					{
						subscriber.sendPacket(packet, buffer);
					});
				}

				// Update health value
				const bool noThreat = ((spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);

				float threat = noThreat ? 0.0f : damage - resisted - absorbed;
				if (!noThreat && m_caster && m_caster->isGameCharacter())
				{
					std::static_pointer_cast<GameCharacter>(m_caster)->applySpellMod(spell_mod_op::Threat, spell.id(), threat);
				}

				UInt32 procAttacker = game::spell_proc_flags::DonePeriodic;
				UInt32 procVictim = game::spell_proc_flags::TakenPeriodic;

				if (damage - resisted - absorbed)
				{
					procVictim |= game::spell_proc_flags::TakenDamage;
				}

				if (m_caster)
				{
					m_caster->procEvent(&m_target, procAttacker, procVictim, game::spell_proc_flags_ex::NormalHit, damage - resisted - absorbed, game::weapon_attack::BaseAttack, &spell, false /*check this*/);
				}				
				
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
					game::server_write::periodicAuraLog(packet, m_target.getGuid(), (m_caster ? m_caster->getGuid() : 0), spell.id(), m_effect.aura(), powerType, power);

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
					game::server_write::periodicAuraLog(packet, m_target.getGuid(), (m_caster ? m_caster->getGuid() : 0), spell.id(), m_effect.aura(), heal);

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
					game::server_write::periodicAuraLog(packet, m_target.getGuid(), (m_caster ? m_caster->getGuid() : 0), spell.id(), m_effect.aura(), 0, value);

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
					game::server_write::periodicAuraLog(packet, m_target.getGuid(), (m_caster ? m_caster->getGuid() : 0), spell.id(), m_effect.aura(), value);

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

	UInt64 AuraEffect::getCasterGuid() const
	{
		return (m_caster.get() ? m_caster->getGuid() : 0);
	}

	void AuraEffect::applyAura(bool restoration/* = false*/)
	{
		auto *world = m_target.getWorldInstance();
		if (!world)
			return;

		auto &spell = m_spellSlot.getSpell();
		if (spell.attributes(0) & game::spell_attributes::BreakableByDamage)
		{
			// Delay the signal connection since spells like frost nova also apply damage. This damage
			// is applied AFTER the root aura (which can break by damage) and thus leads to frost nova 
			// breaking it's own root effect immediatly
			world->getUniverse().post([this]() {
				m_onDamageBreak = m_target.spellProcEvent.connect(
				[this](bool isVictim, GameUnit *target, UInt32 procFlag, UInt32 procEx, const proto::SpellEntry *procSpell, UInt32 amount, UInt8 attackType, bool canRemove)
				{
					if (!target)
						return;

					auto &spell = m_spellSlot.getSpell();
					if (procSpell && spell.id() == procSpell->id())
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
					m_target.getAuras().removeAura(m_spellSlot);
				});
			});
		}

		if ((spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Damage) != 0)
		{
			auto strongThis = shared_from_this();
			std::weak_ptr<AuraEffect> weakThis(strongThis);

			// Subscribe for damage event in next pass, since we don't want to break this aura by it's own damage
			// (this happens for rogue spell Gouge as it deals damage)
			world->getUniverse().post([weakThis]() {
				auto strong = weakThis.lock();
				if (strong)
				{
					strong->m_takenDamage = strong->m_target.takenDamage.connect(
						[strong](GameUnit * victim, UInt32 damage, game::DamageType type) 
						{
							if (type == game::DamageType::Direct)
							{
								strong->getTarget().getAuras().removeAura(strong->getSlot());
							}
						}
					);
				}
			});
		}

		if ((spell.aurainterruptflags() & game::spell_aura_interrupt_flags::NotAboveWater) != 0)
		{
			m_targetEnteredWater = m_target.enteredWater.connect(
			[&]() {
				this->getTarget().getAuras().removeAura(this->getSlot());
			});
		}

		if ((spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Attack) != 0)
		{
			m_targetStartedAttacking = m_target.doneMeleeAttack.connect(
			[&](GameUnit* attacker, game::VictimState) {
				this->getTarget().getAuras().removeAura(this->getSlot());
			});
		}

		if ((spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Cast) != 0)
		{
			auto strongThis = shared_from_this();
			std::weak_ptr<AuraEffect> weakThis(strongThis);

			world->getUniverse().post([weakThis]() {
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

						strong->getTarget().getAuras().removeAura(strong->getSlot());
					});
				}
			});
		}

		if ((spell.attributes(6) & game::spell_attributes_ex_f::IsMountSpell) != 0)
		{
			m_targetStartedAttacking = m_target.startedAttacking.connect([&]() {
				m_target.getAuras().removeAura(m_spellSlot);
			});
			m_targetStartedCasting = m_target.startedCasting.connect(
			[&](const proto::SpellEntry & spell) {
				if ((spell.attributes(0) & game::spell_attributes::CastableWhileMounted) == 0) {
					m_target.getAuras().removeAura(m_spellSlot);
				}
			});
		}

		if (m_caster)
		{
			if (spell.procflags() != game::spell_proc_flags::None ||
				spell.proccustomflags() != game::spell_proc_flags::None)
			{
				m_onProc = m_caster->spellProcEvent.connect(
					[this](bool isVictim, GameUnit *target, UInt32 procFlag, UInt32 procEx, const proto::SpellEntry *procSpell, UInt32 amount, UInt8 attackType, bool canRemove) {
					if (checkProc(amount != 0, target, procFlag, procEx, procSpell, attackType, isVictim))
					{
						handleProcModifier(attackType, canRemove, amount, procSpell, target);
					}
				});

				if ((spell.procflags() & game::spell_proc_flags::TakenDamage))
				{
					m_takenDamage = m_caster->takenDamage.connect(
						[&](GameUnit * victim, UInt32 damage, game::DamageType type) {
						handleTakenDamage(victim);
					});
				}

				if ((spell.procflags() & game::spell_proc_flags::Killed))
				{
					m_procKilled = m_caster->killed.connect(
						[&](GameUnit * killer) {
						handleProcModifier(0, true, 0, nullptr/*TODO: What if killed by spell is required?*/, killer);
					});
				}

			}
			else if (m_effect.aura() == game::aura_type::AddTargetTrigger)
			{
				m_onProc = m_caster->spellProcEvent.connect(
					[this](bool isVictim, GameUnit *target, UInt32 procFlag, UInt32 procEx, const proto::SpellEntry *procSpell, UInt32 amount, UInt8 attackType, bool canRemove) {
					if (procSpell && m_spellSlot.getSpell().family() == procSpell->family() && (m_effect.itemtype() ? m_effect.itemtype() : m_effect.affectmask()) & procSpell->familyflags())
					{
						handleProcModifier(attackType, canRemove, amount, procSpell, target);
					}
				});
			}

			// If an aura of this spell may only be applied on one target at a time...
			if (spell.attributes(5) & game::spell_attributes_ex_e::SingleTargetSpell)
			{
				auto &trackedAuras = m_caster->getTrackedAuras();

				auto it = trackedAuras.find(spell.mechanic());
				if (it != trackedAuras.end())
				{
					if (it->second != &m_target)
					{
						it->second->getAuras().removeAllAurasDueToMechanic(1 << spell.mechanic());
					}
				}

				trackedAuras[spell.mechanic()] = &m_target;
			}
		}

		// Apply modifiers now
		handleModifier(true, restoration);
	}

	void AuraEffect::misapplyAura()
	{
		// Stop watching for these
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

		handleModifier(false);

		if (m_caster)
		{
			auto &spell = m_spellSlot.getSpell();
			if ((spell.attributes(0) & game::spell_attributes::DisabledWhileActive) != 0)
			{
				// Raise cooldown event
				m_caster->setCooldown(spell.id(), spell.cooldown() ? spell.cooldown() : spell.categorycooldown());
				m_caster->cooldownEvent(spell.id());
			}

			if (spell.attributes(5) & game::spell_attributes_ex_e::SingleTargetSpell)
			{
				m_caster->getTrackedAuras().erase(spell.mechanic());
			}
		}

		misapplied();
	}

	void AuraEffect::startPeriodicTimer()
	{
		// Start timer
		m_tickCountdown.setEnd(
			getCurrentTime() + m_effect.amplitude());
	}

	void AuraEffect::onTargetMoved(const math::Vector3 &oldPosition, float oldO)
	{
		// Determine flags
		auto &spell = m_spellSlot.getSpell();
		const bool removeOnMove = (spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Move) != 0;
		const bool removeOnTurn = (spell.aurainterruptflags() & game::spell_aura_interrupt_flags::Turning) != 0;

		const auto &location = m_target.getLocation();
		float orientation = m_target.getOrientation();

		if (removeOnMove)
		{
			if (location != oldPosition)
			{
				// Moved - remove!
				m_target.getAuras().removeAura(m_spellSlot);
				return;
			}
		}

		if (removeOnTurn)
		{
			if (orientation != oldO)
			{
				// Turned - remove!
				m_target.getAuras().removeAura(m_spellSlot);
				return;
			}
		}
	}

	bool AuraEffect::checkProc(bool active, GameUnit *target, UInt32 procFlag, UInt32 procEx, proto::SpellEntry const *procSpell, UInt8 attackType, bool isVictim)
	{
		auto &spell = m_spellSlot.getSpell();

		UInt32 eventProcFlag;
		if (spell.proccustomflags())
		{
			eventProcFlag = spell.proccustomflags();
		}
		else
		{
			eventProcFlag = spell.procflags();
		}

		if (procSpell && procSpell->id() == spell.id() && !(eventProcFlag & game::spell_proc_flags::TakenPeriodic))
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

		if (spell.procschool() && !(spell.procschool() & game::spell_school_mask::Normal))
		{
			return false;
		}
			
		if (!isVictim && m_caster && m_caster->isGameCharacter())
		{
			auto casterChar = std::static_pointer_cast<GameCharacter>(m_caster);
			if (spell.itemclass() == game::item_class::Weapon)
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

				if (!item || item->getEntry().itemclass() != game::item_class::Weapon || !(spell.itemsubclassmask() & (1 << item->getEntry().subclass())))
				{
					return false;
				}
			}
			else if (spell.itemclass() == game::item_class::Armor)
			{
				//Shield
				std::shared_ptr<GameItem> item;

				item = casterChar->getInventory().getItemAtSlot(Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Offhand));

				if (!item || item->getEntry().itemclass() != game::item_class::Armor || !(spell.itemsubclassmask() & (1 << item->getEntry().subclass())))
				{
					return false;
				}
			}
		}
		if (procSpell)
		{
			if (spell.procschool() && !(spell.procschool() & procSpell->schoolmask()))
			{
				return false;
			}

			if (spell.procfamily() && (spell.procfamily() != procSpell->family()))
			{
				return false;
			}
			
			if (spell.procfamilyflags () && !(spell.procfamilyflags() & procSpell->familyflags()))
			{
				return false;
			}
		}

		if (spell.procexflags() != game::spell_proc_flags_ex::None &&
			!(spell.procexflags() & game::spell_proc_flags_ex::TriggerAlways))
		{
			if (!(spell.procexflags() & procEx))
			{
				return false;
			}
		}

		if (!m_effect.affectmask())
		{
			if (!(spell.procexflags() & game::spell_proc_flags_ex::TriggerAlways))
			{
				if (spell.procexflags() == game::spell_proc_flags_ex::None)
				{
					if (!((procEx & (game::spell_proc_flags_ex::NormalHit | game::spell_proc_flags_ex::CriticalHit)) && active))
					{
						return false;
					}
				}
				else
				{
					if ((spell.procexflags() & (game::spell_proc_flags_ex::NormalHit | game::spell_proc_flags_ex::CriticalHit) & procEx) && !active)
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
	
	void wowpp::AuraEffect::handlePeriodicBase()
	{
		// Toggle periodic flag
		m_isPeriodic = true;

		// First tick at apply
		if (m_spellSlot.getSpell().attributes(5) & game::spell_attributes_ex_e::StartPeriodicAtApply)
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
