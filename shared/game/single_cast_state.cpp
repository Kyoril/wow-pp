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
#include "universe.h"
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
		, m_impactCountdown(cast.getTimers())
		, m_castTime(castTime)
	{
		// Check if the executer is in the world
		auto &executer = m_cast.getExecuter();
		auto *worldInstance = executer.getWorldInstance();

		auto const casterId = executer.getGuid();
		auto const targetId = target.getUnitTarget();
		auto const spellId = spell.id;

		if (worldInstance && !(m_spell.attributes & spell_attributes::Passive))
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

		float o = 0.0f;
		m_cast.getExecuter().getLocation(m_x, m_y, m_z, o);

		m_countdown.ended.connect([this]()
		{
			this->onCastFinished();
		});
	}

	void SingleCastState::activate()
	{
		if (m_castTime > 0)
		{
			m_countdown.setEnd(getCurrentTime() + m_castTime);

			// Subscribe to damage events if the spell is cancelled on damage
			m_onUserMoved = m_cast.getExecuter().moved.connect(
				std::bind(&SingleCastState::onUserStartsMoving, this));

			// TODO: Subscribe to target removed and died events (in both cases, the cast may be interrupted)
		}
		else
		{
			onCastFinished();
		}
	}

	std::pair<game::SpellCastResult, SpellCasting *> SingleCastState::startCast(SpellCast &cast, const SpellEntry &spell, SpellTargetMap target, GameTime castTime, bool doReplacePreviousCast)
	{
		if (!m_hasFinished &&
			!doReplacePreviousCast)
		{
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

		const std::weak_ptr<SingleCastState> weakThis = shared_from_this();
		m_casting.ended(false);

		if (weakThis.lock())
		{
			m_cast.setState(std::unique_ptr<SpellCast::CastState>(
				new NoCastState
				));
		}
	}

	void SingleCastState::onUserStartsMoving()
	{
		if (!m_hasFinished)
		{
			// Interrupt spell cast if moving
			float x, y, z, o = 0.0f;
			m_cast.getExecuter().getLocation(x, y, z, o);
			if (x != m_x || y != m_y || z != m_z)
			{
				stopCast();
			}
		}
	}

	void SingleCastState::sendEndCast(bool success)
	{
		auto &executer = m_cast.getExecuter();
		auto *worldInstance = executer.getWorldInstance();
		if (!worldInstance || m_spell.attributes & spell_attributes::Passive)
		{
			return;
		}
		
		if (success)
		{
			// Instead of self-targeting, use unit target
			SpellTargetMap targetMap = m_target;
			if (targetMap.getTargetMap() == game::spell_cast_target_flags::Self)
			{
				targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
				targetMap.m_unitTarget = executer.getGuid();
			}

			sendPacketFromCaster(executer,
				std::bind(game::server_write::spellGo, std::placeholders::_1,
				executer.getGuid(),
				executer.getGuid(),
				std::cref(m_spell),
				std::cref(targetMap),
				game::spell_cast_flags::Unknown3));
		}
		else
		{
			sendPacketFromCaster(executer,
				std::bind(game::server_write::spellFailure, std::placeholders::_1,
				executer.getGuid(),
				m_spell.id,
				game::spell_cast_result::FailedNoPower));

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

		if (m_castTime > 0)
		{
			if (!m_cast.getExecuter().getWorldInstance())
			{
				ELOG("Caster is no longer in world instance");
				m_hasFinished = true;
				return;
			}

			GameUnit *unitTarget = nullptr;
			m_target.resolvePointers(*m_cast.getExecuter().getWorldInstance(), &unitTarget, nullptr, nullptr, nullptr);

			// Range check
			if (m_spell.minRange != 0.0f || m_spell.maxRange != 0.0f)
			{
				if (unitTarget)
				{
					const float distance = m_cast.getExecuter().getDistanceTo(*unitTarget);
					if (m_spell.minRange > 0.0f && distance < m_spell.minRange)
					{
						m_cast.getExecuter().spellCastError(m_spell, game::spell_cast_result::FailedTooClose);

						sendEndCast(false);
						m_hasFinished = true;

						return;
					}
					else if (m_spell.maxRange > 0.0f && distance > m_spell.maxRange)
					{
						m_cast.getExecuter().spellCastError(m_spell, game::spell_cast_result::FailedOutOfRange);

						sendEndCast(false);
						m_hasFinished = true;

						return;
					}
				}
			}

			// Check facing again (we could have moved during the spell cast)
			if (m_spell.facing & 0x01)
			{
				const auto *world = m_cast.getExecuter().getWorldInstance();
				if (world)
				{
					GameUnit *unitTarget = nullptr;
					m_target.resolvePointers(*m_cast.getExecuter().getWorldInstance(), &unitTarget, nullptr, nullptr, nullptr);

					if (unitTarget)
					{
						float x, y, z, o;
						unitTarget->getLocation(x, y, z, o);

						// 120 degree field of view
						if (!m_cast.getExecuter().isInArc(2.0f * 3.1415927f / 3.0f, x, y))
						{
							m_cast.getExecuter().spellCastError(m_spell, game::spell_cast_result::FailedUnitNotInfront);

							sendEndCast(false);
							m_hasFinished = true;

							return;
						}
					}
				}
			}
		}
		
		// TODO: Apply cooldown
		
		m_hasFinished = true;

		auto strongThis = shared_from_this();
		const std::weak_ptr<SingleCastState> weakThis = strongThis;
		if (m_spell.attributes & spell_attributes::OnNextSwing)
		{
			// Execute on next weapon swing
			m_cast.getExecuter().setAttackSwingCallback([strongThis, this]() -> bool
			{
				if (strongThis->consumePower())
				{
					strongThis->sendEndCast(true);
					strongThis->applyAllEffects();
				}
				else
				{
					m_cast.getExecuter().spellCastError(m_spell, game::spell_cast_result::FailedNoPower);
				}
				
				return true;
			});
		}
		else
		{
			if (!consumePower())
				return;

			sendEndCast(true);

			if (m_spell.speed > 0.0f)
			{
				// Calculate distance to target
				GameUnit *unitTarget = nullptr;
				auto *world = m_cast.getExecuter().getWorldInstance();
				if (world)
				{
					m_target.resolvePointers(*world, &unitTarget, nullptr, nullptr, nullptr);
					if (unitTarget)
					{
						// Calculate distance to the target
						const float dist = m_cast.getExecuter().getDistanceTo(*unitTarget);
						const float timeMS = (dist / m_spell.speed) * 1000.0f;
						
						// This will be executed on the impact
						m_impactCountdown.ended.connect(
							[strongThis]() mutable {
							strongThis->applyAllEffects();
							strongThis.reset();
						});

						m_impactCountdown.setEnd(getCurrentTime() + timeMS);
					}
				}
			}
			else
			{
				applyAllEffects();
			}
		}

		// Consume combo points if required
		if ((m_spell.attributesEx[0] & spell_attributes_ex_a::ReqComboPoints_1) && character)
		{
			// 0 will reset combo points
			character->addComboPoints(0, 0);
		}

		// Start auto attack if required
		if (m_spell.attributesEx[0] & spell_attributes_ex_a::MeleeCombatStart)
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
		
		if (weakThis.lock())
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

	void SingleCastState::spellEffectTeleportUnits(const SpellEntry::Effect &effect)
	{
		// Resolve GUIDs
		GameUnit *unitTarget = nullptr;
		GameUnit &caster = m_cast.getExecuter();
		auto *world = caster.getWorldInstance();

		if (m_target.getTargetMap() == game::spell_cast_target_flags::Self)
			unitTarget = &caster;
		else if (world && m_target.hasUnitTarget())
		{
			unitTarget = dynamic_cast<GameUnit*>(world->findObjectByGUID(m_target.getUnitTarget()));
		}

		if (!unitTarget)
		{
			WLOG("SPELL_EFFECT_TELEPORT_UNITS: No unit target to teleport!");
			return;
		}

		// Check whether it is the same map
		if (unitTarget->getMapId() != m_spell.targetMap)
		{
			// Only players can change maps
			if (unitTarget->getTypeId() != object_type::Character)
			{
				WLOG("SPELL_EFFECT_TELEPORT_UNITS: Only players can be teleported to another map!");
				return;
			}

			// Log destination
			DLOG("Teleporting player to map " << m_spell.targetMap << ": " << m_spell.targetX << " / " << m_spell.targetY << " / " << m_spell.targetZ);
			unitTarget->teleport(m_spell.targetMap, m_spell.targetX, m_spell.targetY, m_spell.targetZ, m_spell.targetO);
		}
		else
		{
			// Same map, just move the unit
			if (unitTarget->getTypeId() == object_type::Character)
			{
				// Send teleport signal for player characters
				unitTarget->teleport(m_spell.targetMap, m_spell.targetX, m_spell.targetY, m_spell.targetZ, m_spell.targetO);
			}
			else
			{
				// Simply relocate creatures and other stuff
				unitTarget->relocate(m_spell.targetX, m_spell.targetY, m_spell.targetZ, m_spell.targetO);
			}
		}
	}

	void SingleCastState::spellEffectSchoolDamage(const SpellEntry::Effect &effect)
	{	
		// Calculate damage based on base points
		UInt32 damage = calculateEffectBasePoints(effect);

		// TODO: Apply combo point damage

		// TODO: Apply spell mods

		// Resolve GUIDs
		GameUnit *unitTarget = nullptr;
		GameUnit &caster = m_cast.getExecuter();
		auto *world = caster.getWorldInstance();

		if (m_target.getTargetMap() == game::spell_cast_target_flags::Self)
			unitTarget = &caster;
		else if (world && m_target.hasUnitTarget())
		{
			unitTarget = dynamic_cast<GameUnit*>(world->findObjectByGUID(m_target.getUnitTarget()));
		}

		// Check target
		if (!unitTarget)
		{
			WLOG("EFFECT_SCHOOL_DAMAGE: No valid target found!");
			return;
		}

		// Send spell damage packet
		sendPacketFromCaster(caster,
			std::bind(game::server_write::spellNonMeleeDamageLog, std::placeholders::_1,
			unitTarget->getGuid(),
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
		unitTarget->dealDamage(damage, m_spell.schoolMask, &caster);
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
		GameUnit *unitTarget = nullptr;
		GameUnit &caster = m_cast.getExecuter();
		auto *world = caster.getWorldInstance();

		if (m_target.getTargetMap() == game::spell_cast_target_flags::Self)
			unitTarget = &caster;
		else if (world && m_target.hasUnitTarget())
			unitTarget = dynamic_cast<GameUnit*>(world->findObjectByGUID(m_target.getUnitTarget()));

		// Check target
		if (!unitTarget)
		{
			WLOG("EFFECT_NORMALIZED_WEAPON_DMG: No valid target found!");
			return;
		}

		// Armor reduction
		damage = calculateArmorReducedDamage(m_cast.getExecuter().getLevel(), *unitTarget, damage);

		// Send spell damage packet
		sendPacketFromCaster(caster,
			std::bind(game::server_write::spellNonMeleeDamageLog, std::placeholders::_1,
			unitTarget->getGuid(),
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
		unitTarget->dealDamage(damage, m_spell.schoolMask, &caster);
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

		// Check if target is dead
		UInt32 health = unitTarget->getUInt32Value(unit_fields::Health);
		if (health == 0 && 
			!(m_spell.attributesEx[2] & 0x00100000))
		{
			// Spell aura is not death persistant and thus can not be added
			DLOG("Target is dead - can't apply aura");
			return;
		}

		// Casting unit
		GameUnit &caster = m_cast.getExecuter();

		// Determine aura type
		game::AuraType auraType = static_cast<game::AuraType>(effect.auraName);
		const String auraTypeName = game::constant_literal::auraTypeNames.getName(auraType);
		DLOG("Spell: Aura is: " << auraTypeName);

		// World was already checked. If world would ne nullptr, unitTarget would be null as well
		auto *world = m_cast.getExecuter().getWorldInstance();
		auto &universe = world->getUniverse();

		// Create a new aura instance
		const Int32 basePoints = calculateEffectBasePoints(effect);
		std::shared_ptr<Aura> aura = std::make_shared<Aura>(m_spell, effect, basePoints, caster, *unitTarget, [&universe](std::function<void()> work)
		{
			universe.post(work);
		}, [](Aura &self)
		{
			auto &auras = self.getTarget().getAuras();

			const auto position = findAuraInstanceIndex(auras, self);
			//assert(position.is_initialized());
			if (position.is_initialized())
			{
				auras.removeAura(*position);
			}
		});

		// TODO: Dimishing return and custom durations

		// TODO: Apply spell haste

		// TODO: Check if aura already expired

		// TODO: Add aura to unit target
		const bool success = unitTarget->getAuras().addAura(std::move(aura));
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

	void SingleCastState::applyAllEffects()
	{
		// Make sure that this isn't destroyed during the effects
		auto strong = shared_from_this();

		// Execute spell immediatly
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
			case se::TeleportUnits:
				spellEffectTeleportUnits(effect);
				break;
			case se::Weapon:
			case se::Language:
				// Nothing to do here, since the skills for these spells will be applied as soon as the player
				// learns this spell.
				break;
			case se::TriggerSpell:
				spellEffectTriggerSpell(effect);
				break;
			case se::Energize:
				spellEffectEnergize(effect);
				break;
			default:
				WLOG("Spell effect " << game::constant_literal::spellEffectNames.getName(effect.type) << " (" << effect.type << ") not yet implemented");
				break;
			}
		}
	}

	bool SingleCastState::consumePower()
	{
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
					return false;
				}

				currentPower -= m_spell.cost;
				m_cast.getExecuter().setUInt32Value(unit_fields::Power1 + m_spell.powerType, currentPower);

				if (m_spell.powerType == power_type::Mana)
					m_cast.getExecuter().notifyManaUse();
			}
		}

		return true;
	}

	void SingleCastState::spellEffectTriggerSpell(const SpellEntry::Effect &effect)
	{
		if (!effect.triggerSpell)
		{
			WLOG("Spell " << m_spell.id << ": No spell to trigger found! Trigger effect will be ignored.");
			return;
		}

		// Get the spell to trigger
		startCast(m_cast, *effect.triggerSpell, m_target, 0, true);
	}

	void SingleCastState::spellEffectEnergize(const SpellEntry::Effect &effect)
	{
		Int32 powerType = effect.miscValueA;
		if (powerType < 0 || powerType > 5)
			return;

		UInt32 power = calculateEffectBasePoints(effect);

		// TODO: Get unit target
		GameUnit *unitTarget = nullptr;
		auto *world = m_cast.getExecuter().getWorldInstance();
		if (!world)
		{
			WLOG("Caster not in any world instance right now.");
			return;
		}

		if (m_target.getTargetMap() == game::spell_cast_target_flags::Self)
			unitTarget = &m_cast.getExecuter();
		else
			m_target.resolvePointers(*world, &unitTarget, nullptr, nullptr, nullptr);

		if (!unitTarget)
		{
			WLOG("No valid unit target found!");
			return;
		}

		UInt32 curPower = unitTarget->getUInt32Value(unit_fields::Power1 + powerType);
		UInt32 maxPower = unitTarget->getUInt32Value(unit_fields::MaxPower1 + powerType);
		if (curPower + power > maxPower)
		{
			curPower = maxPower;
		}
		else
		{
			curPower += power;
		}
		unitTarget->setUInt32Value(unit_fields::Power1 + powerType, curPower);
		sendPacketFromCaster(m_cast.getExecuter(),
			std::bind(game::server_write::spellEnergizeLog, std::placeholders::_1, m_cast.getExecuter().getGuid(), unitTarget->getGuid(), m_spell.id, static_cast<UInt8>(powerType), power));
	}
}
