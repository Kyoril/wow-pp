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

#include "single_cast_state.h"
#include "game_unit.h"
#include "game_character.h"
#include "no_cast_state.h"
#include "common/clock.h"
#include "log/default_log_levels.h"
#include "world_instance.h"
#include "common/utilities.h"
#include "game/defines.h"
#include "visibility_grid.h"
#include "visibility_tile.h"
#include "each_tile_in_sight.h"
#include "binary_io/vector_sink.h"
#include "game_protocol/game_protocol.h"
#include <boost/iterator/indirect_iterator.hpp>
#include "common/make_unique.h"
//TODO
#include "aura.h"
#include <random>

namespace wowpp
{
	namespace
	{
		template <class T>
		void sendPacketFromCaster(GameUnit &caster, T generator)
		{
			auto *worldInstance = caster.getWorldInstance();
			if (!worldInstance)
			{
				return;
			}

			float x, y, z, o;
			caster.getLocation(x, y, z, o);

			TileIndex2D tileIndex;
			worldInstance->getGrid().getTilePosition(x, y, z, tileIndex[0], tileIndex[1]);

			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);
			generator(packet);

			forEachSubscriberInSight(
				worldInstance->getGrid(),
				tileIndex,
				[&buffer, &packet](ITileSubscriber &subscriber)
			{
				subscriber.sendPacket(
					packet,
					buffer
					);
			});
		}
	}
	
	SingleCastState::SingleCastState(SpellCast &cast, const SpellEntry &spell, SpellTargetMap target, GameTime castTime)
		: m_cast(cast)
		, m_spell(spell)
		, m_target(std::move(target))
		, m_hasFinished(false)
		, m_countdown(cast.getTimers())
		, m_isAlive(std::make_shared<char>(0))
	{
		// Check if the executer is in the world
		auto &executer = m_cast.getExecuter();
		auto *worldInstance = executer.getWorldInstance();

		auto const casterId = executer.getGuid();
		auto const targetId = target.getUnitTarget();
		auto const spellId = spell.id;

		if (worldInstance && !(m_spell.attributes & 0x40))
		{
			sendPacketFromCaster(executer,
				std::bind(game::server_write::spellStart, std::placeholders::_1,
				casterId,
				casterId,
				std::cref(m_spell),
				std::cref(m_target),
				game::spell_cast_flags::Unknown1,
				castTime,
				0));
		}

		m_countdown.ended.connect([this]()
		{
			this->onCastFinished();
		});

		if (castTime > 0)
		{
			m_countdown.setEnd(getCurrentTime() + castTime);

			// Subscribe to damage events if the spell is cancelled on damage
			m_onUserMoved = executer.moved.connect(
				std::bind(&SingleCastState::onUserStartsMoving, this));
		}
		else
		{
			onCastFinished();
		}

		// TODO: Subscribe to target removed and died events (in both cases, the cast may be interrupted)

	}

	std::pair<game::SpellCastResult, SpellCasting *> SingleCastState::startCast(SpellCast &cast, const SpellEntry &spell, SpellTargetMap target, GameTime castTime, bool doReplacePreviousCast)
	{
		if (!m_hasFinished &&
			!doReplacePreviousCast)
		{
			WLOG("SPELL CAST IS STILL IN PROGRESS!");
			return std::make_pair(game::spell_cast_result::FailedSpellInProgress, &m_casting);
		}

		SpellCasting &casting = castSpell(
			cast,
			spell,
			std::move(target),
			castTime);

		return std::make_pair(game::spell_cast_result::CastOkay, &casting);
	}

	void SingleCastState::stopCast()
	{
		m_countdown.cancel();

		if (!m_hasFinished)
		{
			sendEndCast(false);
			m_hasFinished = true;
		}

		const std::weak_ptr<char> isAlive = m_isAlive;
		m_casting.ended(false);

		if (isAlive.lock())
		{
			m_cast.setState(std::unique_ptr<SpellCast::CastState>(
				new NoCastState
				));
		}
	}

	void SingleCastState::onUserStartsMoving()
	{
		// Interrupt spell cast if moving
		if (!m_hasFinished)
		{
			stopCast();
		}
	}

	void SingleCastState::sendEndCast(bool success)
	{
		auto &executer = m_cast.getExecuter();
		auto *worldInstance = executer.getWorldInstance();
		if (!worldInstance || m_spell.attributes & 0x40)
		{
			return;
		}
		
		if (success)
		{
			sendPacketFromCaster(executer,
				std::bind(game::server_write::spellGo, std::placeholders::_1,
				executer.getGuid(),
				executer.getGuid(),
				std::cref(m_spell),
				std::cref(m_target),
				game::spell_cast_flags::Unknown3));
		}
		else
		{
			sendPacketFromCaster(executer,
				std::bind(game::server_write::spellFailure, std::placeholders::_1,
				executer.getGuid(),
				m_spell.id,
				game::spell_cast_result::FailedAffectingCombat));

			sendPacketFromCaster(executer,
				std::bind(game::server_write::spellFailedOther, std::placeholders::_1,
				executer.getGuid(),
				m_spell.id));
		}
	}

	void SingleCastState::onCastFinished()
	{
		GameCharacter *character = nullptr;
		if (isPlayerGUID(m_cast.getExecuter().getGuid()))
		{
			character = dynamic_cast<GameCharacter*>(&m_cast.getExecuter());
		}

		// TODO: We need to check some conditions now

		// Consume power
		if (m_spell.cost > 0)
		{
			if (m_spell.powerType == power_type::Health)
			{
				// Special case
				DLOG("TODO: Spell cost power type Health");
			}
			else
			{
				UInt32 currentPower = m_cast.getExecuter().getUInt32Value(unit_fields::Power1 + m_spell.powerType);
				if (currentPower < m_spell.cost)
				{
					WLOG("Not enough power to cast spell!");

					sendEndCast(false);
					m_hasFinished = true;
					return;
				}

				currentPower -= m_spell.cost;
				m_cast.getExecuter().setUInt32Value(unit_fields::Power1 + m_spell.powerType, currentPower);

				if (m_spell.powerType == power_type::Mana)
					m_cast.getExecuter().notifyManaUse();
			}
		}


		// TODO: Apply cooldown

		sendEndCast(true);
		m_hasFinished = true;

		const std::weak_ptr<char> isAlive = m_isAlive;

		// Apply spell effects on all targets
		namespace se = game::spell_effects;
		for (auto &effect : m_spell.effects)
		{
			switch (effect.type)
			{
				case se::SchoolDamage:
					spellEffectSchoolDamage(effect);
					break;
				case se::PowerDrain:
					spellEffectDrainPower(effect);
					break;
				case se::Heal:
					spellEffectHeal(effect);
					break;
				case se::Proficiency:
					spellEffectProficiency(effect);
					break;
				case se::AddComboPoints:
					spellEffectAddComboPoints(effect);
					break;
				case se::WeaponDamageNoSchool:
					spellEffectWeaponDamageNoSchool(effect);
					break;
				case se::ApplyAura:
					spellEffectApplyAura(effect);
					break;
				case se::WeaponDamage:
					spellEffectWeaponDamage(effect);
					break;
				case se::NormalizedWeaponDmg:
					spellEffectNormalizedWeaponDamage(effect);
					break;
				case se::Weapon:
				case se::Language:
					// Nothing to do here, since the skills for these spells will be applied as soon as the player
					// learns this spell.
					break;
				default:
					WLOG("Spell effect " << game::constant_literal::spellEffectNames.getName(effect.type) << " (" << effect.type << ") not yet implemented");
					break;
			}
		}

		// Consume combo points if required
		if ((m_spell.attributesEx[0] & 0x00100000) && character)
		{
			// 0 will reset combo points
			character->addComboPoints(0, 0);
		}

		// Start auto attack if required
		if (m_spell.attributesEx[0] & 0x00000200)
		{
			GameUnit *attackTarget = nullptr;
			if (m_target.hasUnitTarget())
			{
				attackTarget = dynamic_cast<GameUnit*>(m_cast.getExecuter().getWorldInstance()->findObjectByGUID(m_target.getUnitTarget()));
			}

			if (attackTarget)
			{
				m_cast.getExecuter().startAttack(*attackTarget);
			}
			else
			{
				WLOG("Unable to find target for auto attack after spell cast!");
			}
		}
		
		if (isAlive.lock())
		{
			//may destroy this, too
			m_casting.ended(true);
		}
	}

	void SingleCastState::onTargetRemovedOrDead()
	{
		stopCast();
	}

	void SingleCastState::onUserDamaged()
	{
		// This is only triggerd if the spell has the attribute
		stopCast();
	}

	void SingleCastState::spellEffectSchoolDamage(const SpellEntry::Effect &effect)
	{	
		// Calculate damage based on base points
		UInt32 damage = calculateEffectBasePoints(effect);

		// TODO: Apply combo point damage

		// TODO: Apply spell mods

		// Resolve GUIDs
		GameObject *target = nullptr;
		GameUnit *unitTarget = nullptr;
		GameUnit &caster = m_cast.getExecuter();
		auto *world = caster.getWorldInstance();

		if (m_target.getTargetMap() == game::spell_cast_target_flags::Self)
			target = &caster;
		else if (world)
		{
			UInt64 targetGuid = 0;
			if (m_target.hasUnitTarget())
				targetGuid = m_target.getUnitTarget();
			else if (m_target.hasGOTarget())
				targetGuid = m_target.getGOTarget();
			else if (m_target.hasItemTarget())
				targetGuid = m_target.getItemTarget();
			
			if (targetGuid != 0)
				target = world->findObjectByGUID(targetGuid);

			if (m_target.hasUnitTarget() && isUnitGUID(targetGuid))
				unitTarget = reinterpret_cast<GameUnit*>(target);
		}

		// Check target
		if (!target)
		{
			WLOG("EFFECT_SCHOOL_DAMAGE: No valid target found!");
			return;
		}

		// Send spell damage packet
		sendPacketFromCaster(caster,
			std::bind(game::server_write::spellNonMeleeDamageLog, std::placeholders::_1,
			target->getGuid(),
			caster.getGuid(),
			m_spell.id,
			damage,
			m_spell.schoolMask,
			0,
			0,
			false,
			0,
			false));

		// Update health value
		UInt32 health = target->getUInt32Value(unit_fields::Health);
		if (health > damage)
			health -= damage;
		else
			health = 0;

		target->setUInt32Value(unit_fields::Health, health);
		if (health == 0 && unitTarget)
		{
			unitTarget->killed(&caster);
		}
	}

	void SingleCastState::spellEffectNormalizedWeaponDamage(const SpellEntry::Effect &effect)
	{
		// Calculate damage based on base points
		UInt32 damage = calculateEffectBasePoints(effect);

		// Add weapon damage
		const float weaponMin = m_cast.getExecuter().getFloatValue(unit_fields::MinDamage);
		const float weaponMax = m_cast.getExecuter().getFloatValue(unit_fields::MaxDamage);

		// Randomize weapon damage
		std::uniform_real_distribution<float> distribution(weaponMin, weaponMax);
		damage += UInt32(distribution(randomGenerator));

		// Resolve GUIDs
		GameObject *target = nullptr;
		GameUnit *unitTarget = nullptr;
		GameUnit &caster = m_cast.getExecuter();
		auto *world = caster.getWorldInstance();

		if (m_target.getTargetMap() == game::spell_cast_target_flags::Self)
			target = &caster;
		else if (world)
		{
			UInt64 targetGuid = 0;
			if (m_target.hasUnitTarget())
				targetGuid = m_target.getUnitTarget();
			else if (m_target.hasGOTarget())
				targetGuid = m_target.getGOTarget();
			else if (m_target.hasItemTarget())
				targetGuid = m_target.getItemTarget();

			if (targetGuid != 0)
				target = world->findObjectByGUID(targetGuid);

			if (m_target.hasUnitTarget() && isUnitGUID(targetGuid))
				unitTarget = reinterpret_cast<GameUnit*>(target);
		}

		// Check target
		if (!target)
		{
			WLOG("EFFECT_NORMALIZED_WEAPON_DMG: No valid target found!");
			return;
		}

		// Armor reduction
		damage = calculateArmorReducedDamage(m_cast.getExecuter().getLevel(), *unitTarget, damage);

		// Send spell damage packet
		sendPacketFromCaster(caster,
			std::bind(game::server_write::spellNonMeleeDamageLog, std::placeholders::_1,
			target->getGuid(),
			caster.getGuid(),
			m_spell.id,
			damage,
			m_spell.schoolMask,
			0,
			0,
			false,
			0,
			false));

		// Update health value
		UInt32 health = target->getUInt32Value(unit_fields::Health);
		if (health > damage)
			health -= damage;
		else
			health = 0;

		target->setUInt32Value(unit_fields::Health, health);
		if (health == 0 && unitTarget)
		{
			unitTarget->killed(&caster);
		}
	}

	void SingleCastState::spellEffectDrainPower(const SpellEntry::Effect &effect)
	{
		// Calculate the power to drain
		UInt32 powerToDrain = calculateEffectBasePoints(effect);
		Int32 powerType = effect.miscValueA;

		// Resolve GUIDs
		GameObject *target = nullptr;
		GameUnit *unitTarget = nullptr;
		GameUnit &caster = m_cast.getExecuter();
		auto *world = caster.getWorldInstance();

		if (m_target.getTargetMap() == game::spell_cast_target_flags::Self)
			target = &caster;
		else if (world)
		{
			UInt64 targetGuid = 0;
			if (m_target.hasUnitTarget())
				targetGuid = m_target.getUnitTarget();
			else if (m_target.hasGOTarget())
				targetGuid = m_target.getGOTarget();
			else if (m_target.hasItemTarget())
				targetGuid = m_target.getItemTarget();

			if (targetGuid != 0)
				target = world->findObjectByGUID(targetGuid);

			if (m_target.hasUnitTarget() && isUnitGUID(targetGuid))
				unitTarget = reinterpret_cast<GameUnit*>(target);
		}

		// Check target
		if (!unitTarget)
		{
			WLOG("EFFECT_POWER_DRAIN: No valid target found!");
			return;
		}

		// Does this have any effect on the target?
		if (unitTarget->getByteValue(unit_fields::Bytes0, 3) != powerType)
			return;	// Target does not use this kind of power
		if (powerToDrain == 0)
			return;	// Nothing to drain...

		UInt32 currentPower = unitTarget->getUInt32Value(unit_fields::Power1 + powerType);
		if (currentPower == 0)
			return;	// Target doesn't have any power left

		// Now drain the power
		if (powerToDrain > currentPower)
			powerToDrain = currentPower;

		// Remove power
		unitTarget->setUInt32Value(unit_fields::Power1 + powerType, currentPower - powerToDrain);

		// If mana was drain, give the same amount of mana to the caster (or energy, if the caster does
		// not use mana)
		if (powerType == power_type::Mana)
		{
			// Give the same amount of power to the caster, but convert it to energy if needed
			UInt8 casterPowerType = caster.getByteValue(unit_fields::Bytes0, 3);
			if (casterPowerType != powerType)
			{
				// Only mana will be given
				return;
			}

			// Send spell damage packet
			sendPacketFromCaster(caster,
				std::bind(game::server_write::spellEnergizeLog, std::placeholders::_1,
					caster.getGuid(),
					caster.getGuid(),
					m_spell.id,
					casterPowerType, 
					powerToDrain));

			// Modify casters power values
			UInt32 casterPower = caster.getUInt32Value(unit_fields::Power1 + casterPowerType);
			UInt32 maxCasterPower = caster.getUInt32Value(unit_fields::MaxPower1 + casterPowerType);
			if (casterPower + powerToDrain > maxCasterPower)
				powerToDrain = maxCasterPower - casterPower;
			caster.setUInt32Value(unit_fields::Power1 + casterPowerType, casterPower + powerToDrain);
		}
	}

	void SingleCastState::spellEffectProficiency(const SpellEntry::Effect &effect)
	{
		// Self target
		GameCharacter *character = nullptr;
		if (isPlayerGUID(m_cast.getExecuter().getGuid()))
		{
			character = dynamic_cast<GameCharacter*>(&m_cast.getExecuter());
		}

		if (!character)
		{
			WLOG("SPELL_EFFECT_PROFICIENCY: Requires character unit target!");
			return;
		}

		const UInt32 mask = m_spell.itemSubClassMask;
		if (m_spell.itemClass == 2 && !(character->getWeaponProficiency() & mask))
		{
			character->addWeaponProficiency(mask);
		}
		else if (m_spell.itemClass == 4 && !(character->getArmorProficiency() & mask))
		{
			character->addArmorProficiency(mask);
		}
	}

	Int32 SingleCastState::calculateEffectBasePoints(const SpellEntry::Effect &effect)
	{
		GameCharacter *character = nullptr;
		if (isPlayerGUID(m_cast.getExecuter().getGuid()))
		{
			character = dynamic_cast<GameCharacter*>(&m_cast.getExecuter());
		}

		const Int32 comboPoints = character ? character->getComboPoints() : 0;

		// Calculate the damage done
		const float basePointsPerLevel = effect.pointsPerLevel;
		const Int32 basePoints = effect.basePoints;
		const Int32 randomPoints = effect.dieSides;
		const Int32 comboDamage = effect.pointsPerComboPoint * comboPoints;

		std::uniform_int_distribution<int> distribution(effect.baseDice, randomPoints);
		const Int32 randomValue = (effect.baseDice >= randomPoints ? effect.baseDice : distribution(randomGenerator));

		return basePoints + randomValue + comboDamage;
	}

	void SingleCastState::spellEffectAddComboPoints(const SpellEntry::Effect &effect)
	{
		GameCharacter *character = nullptr;
		if (isPlayerGUID(m_cast.getExecuter().getGuid()))
		{
			character = dynamic_cast<GameCharacter*>(&m_cast.getExecuter());
		}

		if (!character)
		{
			ELOG("Invalid character");
			return;
		}

		UInt64 comboTarget = m_target.getUnitTarget();
		character->addComboPoints(comboTarget, UInt8(calculateEffectBasePoints(effect)));
	}

	void SingleCastState::spellEffectWeaponDamageNoSchool(const SpellEntry::Effect &effect)
	{
		//TODO: Implement
		spellEffectNormalizedWeaponDamage(effect);
	}

	void SingleCastState::spellEffectWeaponDamage(const SpellEntry::Effect &effect)
	{
		//TODO: Implement
		spellEffectNormalizedWeaponDamage(effect);
	}

	void SingleCastState::spellEffectApplyAura(const SpellEntry::Effect &effect)
	{
		// We need a unit target
		GameUnit *unitTarget = nullptr;
		if (m_target.getTargetMap() == game::spell_cast_target_flags::Self)
		{
			unitTarget = &m_cast.getExecuter();
		}
		else if (m_target.hasUnitTarget())
		{
			auto *world = m_cast.getExecuter().getWorldInstance();
			if (world)
			{
				unitTarget = dynamic_cast<GameUnit*>(world->findObjectByGUID(m_target.getUnitTarget()));
			}
		}

		if (!unitTarget)
		{
			WLOG("EFFECT_APPLY_AURA needs a valid unit target!");
			return;
		}

		// Casting unit
		GameUnit &caster = m_cast.getExecuter();

		// Determine aura type
		game::AuraType auraType = static_cast<game::AuraType>(effect.auraName);
		const String auraTypeName = game::constant_literal::auraTypeNames.getName(auraType);
		DLOG("Spell: Aura is: " << auraTypeName);

		// Create a new aura instance
		const Int32 basePoints = calculateEffectBasePoints(effect);
		std::unique_ptr<Aura> aura = make_unique<Aura>(m_spell, effect, basePoints, caster, *unitTarget);

		// TODO: Dimishing return and custom durations

		// TODO: Apply spell haste

		// TODO: Check if aura already expired

		// TODO: Add aura to unit target
		const bool success = unitTarget->addAura(std::move(aura));
		if (!success)
		{
			// TODO: What should we do here? Just ignore?
			WLOG("Aura could not be added to unit target!");
		}
	}

	void SingleCastState::spellEffectHeal(const SpellEntry::Effect &effect)
	{
		UInt32 healAmount = calculateEffectBasePoints(effect);

		// Resolve GUIDs
		GameObject *target = nullptr;
		GameUnit *unitTarget = nullptr;
		GameUnit &caster = m_cast.getExecuter();
		auto *world = caster.getWorldInstance();

		if (m_target.getTargetMap() == game::spell_cast_target_flags::Self)
			target = &caster;
		else if (world)
		{
			UInt64 targetGuid = 0;
			if (m_target.hasUnitTarget())
				targetGuid = m_target.getUnitTarget();
			else if (m_target.hasGOTarget())
				targetGuid = m_target.getGOTarget();
			else if (m_target.hasItemTarget())
				targetGuid = m_target.getItemTarget();

			if (targetGuid != 0)
				target = world->findObjectByGUID(targetGuid);

			if (m_target.hasUnitTarget() && isUnitGUID(targetGuid))
				unitTarget = reinterpret_cast<GameUnit*>(target);
		}

		// Check target
		if (!unitTarget)
		{
			WLOG("EFFECT_POWER_DRAIN: No valid target found!");
			return;
		}

		UInt32 health = target->getUInt32Value(unit_fields::Health);
		UInt32 maxHealth = target->getUInt32Value(unit_fields::MaxHealth);
		if (health == 0)
		{
			WLOG("Can't heal dead target!");
			return;
		}

		// Send spell heal packet
		sendPacketFromCaster(caster,
			std::bind(game::server_write::spellHealLog, std::placeholders::_1,
			unitTarget->getGuid(),
			caster.getGuid(),
			m_spell.id,
			healAmount,
			false));

		// Update health value
		if (health + healAmount < maxHealth)
			health += healAmount;
		else
			health = maxHealth;

		target->setUInt32Value(unit_fields::Health, health);
	}

}
