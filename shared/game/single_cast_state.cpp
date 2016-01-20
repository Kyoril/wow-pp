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

#include "single_cast_state.h"
#include "game_unit.h"
#include "game_character.h"
#include "no_cast_state.h"
#include "common/clock.h"
#include "log/default_log_levels.h"
#include "world_instance.h"
#include "unit_finder.h"
#include "common/utilities.h"
#include "game/defines.h"
#include "game/game_world_object.h"
#include "visibility_grid.h"
#include "visibility_tile.h"
#include "each_tile_in_sight.h"
#include "binary_io/vector_sink.h"
#include "game_protocol/game_protocol.h"
#include <boost/iterator/indirect_iterator.hpp>
#include "common/make_unique.h"
#include "game_creature.h"
#include "universe.h"
#include "aura.h"
#include "proto_data/faction_helper.h"
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

			math::Vector3 location(caster.getLocation());

			TileIndex2D tileIndex;
			worldInstance->getGrid().getTilePosition(location, tileIndex[0], tileIndex[1]);

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
	
	SingleCastState::SingleCastState(SpellCast &cast, const proto::SpellEntry &spell, SpellTargetMap target, Int32 basePoints, GameTime castTime, bool isProc/* = false*/)
		: m_cast(cast)
		, m_spell(spell)
		, m_target(std::move(target))
		, m_hasFinished(false)
		, m_countdown(cast.getTimers())
		, m_impactCountdown(cast.getTimers())
		, m_castTime(castTime)
		, m_basePoints(basePoints)
		, m_isProc(isProc)
		, m_attackTable()
	{
		// Check if the executer is in the world
		auto &executer = m_cast.getExecuter();
		auto *worldInstance = executer.getWorldInstance();

		auto const casterId = executer.getGuid();
		auto const targetId = target.getUnitTarget();
		auto const spellId = spell.id();

		if (worldInstance && !(m_spell.attributes(0) & game::spell_attributes::Passive) && !m_isProc)
		{
			sendPacketFromCaster(
				executer,
				std::bind(game::server_write::spellStart, 
					std::placeholders::_1,					// Packet
					casterId,								// Caster
					casterId,								// Target
					std::cref(m_spell),						// Spell cref
					std::cref(m_target),					// Target map cref
					game::spell_cast_flags::Unknown1,		// Cast flags
					static_cast<Int32>(castTime),			// Cast time in ms
					0)										// Cast count (unknown)
				);
		}

		math::Vector3 location(m_cast.getExecuter().getLocation());
		m_x = location.x, m_y = location.y, m_z = location.z;

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

	std::pair<game::SpellCastResult, SpellCasting *> SingleCastState::startCast(SpellCast &cast, const proto::SpellEntry &spell, SpellTargetMap target, Int32 basePoints, GameTime castTime, bool doReplacePreviousCast)
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
			basePoints,
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
			math::Vector3 location(m_cast.getExecuter().getLocation());
			if (location.x != m_x || location.y != m_y || location.z != m_z)
			{
				stopCast();
			}
		}
	}

	void SingleCastState::sendEndCast(bool success)
	{
		auto &executer = m_cast.getExecuter();
		auto *worldInstance = executer.getWorldInstance();
		if (!worldInstance || m_spell.attributes(0) & game::spell_attributes::Passive)
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

			UInt32 flags = game::spell_cast_flags::Unknown3;
			if (m_isProc)
			{
				flags |= game::spell_cast_flags::Pending;
			}

			sendPacketFromCaster(executer,
				std::bind(game::server_write::spellGo, std::placeholders::_1,
				executer.getGuid(),
				executer.getGuid(),
				std::cref(m_spell),
				std::cref(targetMap),
				static_cast<game::SpellCastFlags>(flags)));
		}
		else
		{
			sendPacketFromCaster(executer,
				std::bind(game::server_write::spellFailure, std::placeholders::_1,
				executer.getGuid(),
				m_spell.id(),
				game::spell_cast_result::FailedNoPower));

			sendPacketFromCaster(executer,
				std::bind(game::server_write::spellFailedOther, std::placeholders::_1,
				executer.getGuid(),
				m_spell.id()));
		}
	}

	void SingleCastState::onCastFinished()
	{
		auto strongThis = shared_from_this();

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
			if (m_spell.minrange() != 0.0f || m_spell.maxrange() != 0.0f)
			{
				if (unitTarget)
				{
					const float combatReach = unitTarget->getFloatValue(unit_fields::CombatReach) + m_cast.getExecuter().getFloatValue(unit_fields::CombatReach);
					const float distance = m_cast.getExecuter().getDistanceTo(*unitTarget);
					if (m_spell.minrange() > 0.0f && distance < m_spell.minrange())
					{
						m_cast.getExecuter().spellCastError(m_spell, game::spell_cast_result::FailedTooClose);

						sendEndCast(false);
						m_hasFinished = true;

						return;
					}
					else if (m_spell.maxrange() > 0.0f && distance > m_spell.maxrange() + combatReach)
					{
						m_cast.getExecuter().spellCastError(m_spell, game::spell_cast_result::FailedOutOfRange);

						sendEndCast(false);
						m_hasFinished = true;

						return;
					}
				}
			}

			// Check facing again (we could have moved during the spell cast)
			if (m_spell.facing() & 0x01)
			{
				const auto *world = m_cast.getExecuter().getWorldInstance();
				if (world)
				{
					GameUnit *unitTarget = nullptr;
					m_target.resolvePointers(*m_cast.getExecuter().getWorldInstance(), &unitTarget, nullptr, nullptr, nullptr);

					if (unitTarget)
					{
						math::Vector3 location(unitTarget->getLocation());

						// 120 degree field of view
						if (!m_cast.getExecuter().isInArc(2.0f * 3.1415927f / 3.0f, location.x, location.y))
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

		const std::weak_ptr<SingleCastState> weakThis = strongThis;
		if (m_spell.attributes(0) & game::spell_attributes::OnNextSwing)
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
					return false;
				}
				
				return true;
			});
		}
		else
		{
			if (!consumePower())
				return;

			sendEndCast(true);

			if (m_spell.speed() > 0.0f)
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
						const float timeMS = (dist / m_spell.speed()) * 1000.0f;
						
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
		if ((m_spell.attributes(1) & game::spell_attributes_ex_a::ReqComboPoints_1) && character)
		{
			// 0 will reset combo points
			character->addComboPoints(0, 0);
		}

		// Start auto attack if required
		if (m_spell.attributes(1) & game::spell_attributes_ex_a::MeleeCombatStart)
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
	
	void SingleCastState::spellEffectInstantKill(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit*> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);
		
		for (int i=0; i<targets.size(); i++)
		{
			GameUnit* targetUnit = targets[i];
			targetUnit->dealDamage(targetUnit->getUInt32Value(unit_fields::Health), m_spell.schoolmask(), &caster, true);
		}
	}

	void SingleCastState::spellEffectTeleportUnits(const proto::SpellEffect &effect)
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

		UInt32 targetMap = 0;
		math::Vector3 targetPos(0.0f, 0.0f, 0.0f);
		float targetO = 0.0f;
		switch (effect.targetb())
		{
			case game::targets::DstHome:
			{
				GameCharacter *character = dynamic_cast<GameCharacter*>(&caster);
				if (!character)
				{
					WLOG("Only characters do have a home point");
					return;
				}
				character->getHome(targetMap, targetPos, targetO);
				break;
			}
			case game::targets::DstDB:
			{
				targetMap = m_spell.targetmap();
				targetPos.x = m_spell.targetx();
				targetPos.y = m_spell.targety();
				targetPos.z = m_spell.targetz();
				targetO = m_spell.targeto();
				break;
			}
			default:
				WLOG("Unhandled destination type " << effect.targetb() << " - not teleporting!");
				return;
		}

		// Check whether it is the same map
		if (unitTarget->getMapId() != targetMap)
		{
			// Only players can change maps
			if (unitTarget->getTypeId() != object_type::Character)
			{
				WLOG("SPELL_EFFECT_TELEPORT_UNITS: Only players can be teleported to another map!");
				return;
			}

			// Log destination
			unitTarget->teleport(targetMap, targetPos, targetO);
		}
		else
		{
			// Same map, just move the unit
			if (unitTarget->getTypeId() == object_type::Character)
			{
				// Send teleport signal for player characters
				unitTarget->teleport(targetMap, targetPos, targetO);
			}
			else
			{
				// Simply relocate creatures and other stuff
				unitTarget->relocate(targetPos, targetO);
			}
		}
	}

	void SingleCastState::spellEffectSchoolDamage(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		UInt8 school = m_spell.schoolmask();
		std::vector<GameUnit*> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);
		
		for (int i=0; i<targets.size(); i++)
		{
			GameUnit* targetUnit = targets[i];
			UInt32 totalDamage;
			bool crit = false;
			UInt32 resisted = 0;
			UInt32 absorbed = 0;
			if (victimStates[i] == game::victim_state::IsImmune)
			{
				totalDamage = 0;
			}
			else if (hitInfos[i] == game::hit_info::Miss)
			{
				totalDamage = 0;
			}
			else
			{
				UInt32 spellPower = caster.getBonus(school);
				UInt32 spellBonusPct = caster.getBonusPct(school);
				totalDamage = getSpellPointsTotal(effect, spellPower, spellBonusPct);
				if (hitInfos[i] == game::hit_info::CriticalHit)
				{
					crit = true;
					totalDamage *= 2.0f;
				}
				resisted = totalDamage * (resists[i] / 100.0f);
				absorbed = targetUnit->consumeAbsorb(totalDamage - resisted, school);
			}
			
			// Update health value
			const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
			if (targetUnit->dealDamage(totalDamage - resisted - absorbed, school, &caster, noThreat))
			{
				// Send spell damage packet
				sendPacketFromCaster(caster,
					std::bind(game::server_write::spellNonMeleeDamageLog, std::placeholders::_1,
					targetUnit->getGuid(),
					caster.getGuid(),
					m_spell.id(),
					totalDamage,
					school,
					absorbed,
					resisted,	//resisted
					false,
					0,
					crit));
				
				caster.doneSpellMagicDmgClassNeg(targetUnit, school);
				targetUnit->takenDamage(&caster);
			}
		}
	}

	void SingleCastState::spellEffectNormalizedWeaponDamage(const proto::SpellEffect &effect)
	{
		GameUnit &attacker = m_cast.getExecuter();
		UInt8 school = m_spell.schoolmask();
		std::vector<GameUnit*> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpecialMeleeAttack(&attacker, m_target, school, targets, victimStates, hitInfos, resists);
		
		for (int i=0; i<targets.size(); i++)
		{
			GameUnit* targetUnit = targets[i];
			UInt32 totalDamage;
			UInt32 blocked = 0;
			bool crit = false;
			UInt32 resisted = 0;
			UInt32 absorbed = 0;
			if (victimStates[i] == game::victim_state::IsImmune)
			{
				totalDamage = 0;
			}
			else if (hitInfos[i] == game::hit_info::Miss)
			{
				totalDamage = 0;
			}
			else if (victimStates[i] == game::victim_state::Dodge)
			{
				totalDamage = 0;
			}
			else if (victimStates[i] == game::victim_state::Parry)
			{
				totalDamage = 0;
				//TODO accelerate next m_victim autohit
			}
			else 
			{
				// Calculate damage based on base points
				totalDamage = calculateEffectBasePoints(effect);

				// Add weapon damage
				const float weaponMin = attacker.getFloatValue(unit_fields::MinDamage);
				const float weaponMax = attacker.getFloatValue(unit_fields::MaxDamage);

				// Randomize weapon damage
				std::uniform_real_distribution<float> distribution(weaponMin, weaponMax);
				totalDamage += UInt32(distribution(randomGenerator));
				
				// Armor reduction
				totalDamage = targetUnit->calculateArmorReducedDamage(attacker.getLevel(), totalDamage);
				if (totalDamage < 0)	//avoid negative damage when blockValue is high
					totalDamage = 0;

				if (hitInfos[i] == game::hit_info::Glancing)
				{
					totalDamage *= 0.75f;	//TODO more detail
				}
				else if (victimStates[i] == game::victim_state::Blocks)
				{
					UInt32 blockValue = 50;	//TODO get from m_victim
					if (blockValue >= totalDamage)	//avoid negative damage when blockValue is high
					{
						totalDamage = 0;
						blocked = totalDamage;
					}
					else
					{
						totalDamage -= blockValue;
						blocked = blockValue;
					}
				}
				else if (hitInfos[i] == game::hit_info::CriticalHit)
				{
					crit = true;
					totalDamage *= 2.0f;
				}
				else if (hitInfos[i] == game::hit_info::Crushing)
				{
					totalDamage *= 1.5f;
				}
				resisted = totalDamage * (resists[i] / 100.0f);
				absorbed = targetUnit->consumeAbsorb(totalDamage - resisted, school);
				if (absorbed > 0 && absorbed == totalDamage)
				{
					hitInfos[i] = static_cast<game::HitInfo>(hitInfos[i] | game::hit_info::Absorb);
				}
			}
			
			// Update health value
			const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
			if (targetUnit->dealDamage(totalDamage - resisted - absorbed, school, &attacker, noThreat))
			{
				// Send spell damage packet
				sendPacketFromCaster(attacker,
					std::bind(game::server_write::spellNonMeleeDamageLog, std::placeholders::_1,
					targetUnit->getGuid(),
					attacker.getGuid(),
					m_spell.id(),
					totalDamage,
					school,
					absorbed,
					0,
					false,
					0,
					crit));
				
				targetUnit->takenDamage(&attacker);
			}
		}
	}

	void SingleCastState::spellEffectDrainPower(const proto::SpellEffect &effect)
	{
		// Calculate the power to drain
		UInt32 powerToDrain = calculateEffectBasePoints(effect);
		Int32 powerType = effect.miscvaluea();

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
		if (powerType == game::power_type::Mana)
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
					m_spell.id(),
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

	void SingleCastState::spellEffectProficiency(const proto::SpellEffect &effect)
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

		const UInt32 mask = m_spell.itemsubclassmask();
		if (m_spell.itemclass() == 2 && !(character->getWeaponProficiency() & mask))
		{
			character->addWeaponProficiency(mask);
		}
		else if (m_spell.itemclass() == 4 && !(character->getArmorProficiency() & mask))
		{
			character->addArmorProficiency(mask);
		}
	}

	Int32 SingleCastState::calculateEffectBasePoints(const proto::SpellEffect &effect)
	{
		GameCharacter *character = nullptr;
		if (isPlayerGUID(m_cast.getExecuter().getGuid()))
		{
			character = dynamic_cast<GameCharacter*>(&m_cast.getExecuter());
		}

		const Int32 comboPoints = character ? character->getComboPoints() : 0;

		Int32 level = static_cast<Int32>(m_cast.getExecuter().getLevel());
		if (level > m_spell.maxlevel() && m_spell.maxlevel() > 0)
		{
			level = m_spell.maxlevel();
		}
		else if (level < m_spell.baselevel())
		{
			level = m_spell.baselevel();
		}
		level -= m_spell.spelllevel();

		// Calculate the damage done
		const float basePointsPerLevel = effect.pointsperlevel();
		const float randomPointsPerLevel = effect.diceperlevel();
		const Int32 basePoints = (m_basePoints == -1 ? effect.basepoints() : m_basePoints) + level * basePointsPerLevel;
		const Int32 randomPoints = effect.diesides() + level * randomPointsPerLevel;
		const Int32 comboDamage = effect.pointspercombopoint() * comboPoints;

		std::uniform_int_distribution<int> distribution(effect.basedice(), randomPoints);
		const Int32 randomValue = (effect.basedice() >= randomPoints ? effect.basedice() : distribution(randomGenerator));

		return basePoints + randomValue + comboDamage;
	}
	
	UInt32 SingleCastState::getSpellPointsTotal(const proto::SpellEffect &effect, UInt32 spellPower, UInt32 bonusPct)
	{
		Int32 basePoints = calculateEffectBasePoints(effect);
		float castTime = m_castTime;
		if (castTime < 1500)
			castTime = 1500;
		float spellAddCoefficient = (castTime / 3500.0);
		float bonusModificator = 1 + (bonusPct / 100);
		return (basePoints + (spellAddCoefficient * spellPower)) *  bonusModificator;
	}
	
	void SingleCastState::spellEffectAddComboPoints(const proto::SpellEffect &effect)
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
	
	void SingleCastState::spellEffectDuel(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit*> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);
		
		for (int i=0; i<targets.size(); i++)
		{
			GameUnit* targetUnit = targets[i];
			
			SpellTargetMap targetMap;
			targetMap.m_targetMap = game::spell_cast_target_flags::Self;
			targetUnit->castSpell(targetMap, 7267, -1, 0, true);	// cast beg at loser
		}
	}

	void SingleCastState::spellEffectWeaponDamageNoSchool(const proto::SpellEffect &effect)
	{
		//TODO: Implement
		spellEffectNormalizedWeaponDamage(effect);
	}
	
	void SingleCastState::spellEffectCreateItem(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit*> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		
		m_attackTable.checkPositiveSpellNoCrit(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);
		
		for (int i=0; i<targets.size(); i++)
		{
			GameUnit* targetUnit = targets[i];
			
		}
	}

	void SingleCastState::spellEffectWeaponDamage(const proto::SpellEffect &effect)
	{
		//TODO: Implement
		spellEffectNormalizedWeaponDamage(effect);
	}

	void SingleCastState::spellEffectApplyAura(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit*> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		bool isNegative = false;	//TODO check it
		
		if (isNegative) {
			m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);	//DoT
		}
		else
		{
			m_attackTable.checkPositiveSpellNoCrit(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);		//Buff, HoT
		}
		
		// Determine aura type
		game::AuraType auraType = static_cast<game::AuraType>(effect.aura());
//		const String auraTypeName = game::constant_literal::auraTypeNames.getName(auraType);
//		DLOG("Spell: Aura is: " << auraTypeName);

		// World was already checked. If world would be nullptr, unitTarget would be null as well
		auto *world = m_cast.getExecuter().getWorldInstance();
		auto &universe = world->getUniverse();
		for (int i=0; i<targets.size(); i++)
		{
			GameUnit* targetUnit = targets[i];
			UInt8 school = m_spell.schoolmask();
			UInt32 totalPoints;
			bool resisted = false;
			
			if (hitInfos[i] == game::hit_info::Miss)
			{
				totalPoints = 0;
			}
			else if (victimStates[i] == game::victim_state::IsImmune)
			{
				totalPoints = 0;
			}
			else if (victimStates[i] == game::victim_state::Normal)
			{
				UInt32 spellPower = caster.getBonus(school);
				UInt32 spellBonusPct = caster.getBonusPct(school);
				totalPoints = getSpellPointsTotal(effect, spellPower, spellBonusPct);
				if (resists[i] == 100.0f)
				{
					resisted = true;
					totalPoints = 0;
				}
			}
			else
			{
				totalPoints = calculateEffectBasePoints(effect);
			}
			
			if (targetUnit->isAlive() && (isNegative == false || totalPoints > 0))
			{
				std::shared_ptr<Aura> aura = std::make_shared<Aura>(m_spell, effect, totalPoints, caster, *targetUnit, [&universe](std::function<void()> work)
				{
					universe.post(work);
				}, [](Aura &self)
				{
					// Prevent aura from being deleted before being removed from the list
					auto strong = self.shared_from_this();
					
					// Remove aura from the list
					auto &auras = self.getTarget().getAuras();
					const auto position = findAuraInstanceIndex(auras, self);
					if (position.is_initialized())
					{
						auras.removeAura(*position);
					}
				});

				// TODO: Dimishing return and custom durations

				// TODO: Apply spell haste

				// TODO: Check if aura already expired

				const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
				if (!noThreat)
				{
					targetUnit->threatened(caster, 0.0f);
				}

				// TODO: Add aura to unit target
				const bool success = targetUnit->getAuras().addAura(std::move(aura));
				if (!success)
				{
					// TODO: What should we do here? Just ignore?
					WLOG("Aura could not be added to unit target!");
				}
			}
		}
	}

	void SingleCastState::spellEffectHeal(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit*> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);		//Buff, HoT
		
		for (int i=0; i<targets.size(); i++)
		{
			GameUnit* targetUnit = targets[i];
			UInt8 school = m_spell.schoolmask();
			UInt32 totalPoints;
			bool crit = false;
			if (victimStates[i] == game::victim_state::IsImmune)
			{
				totalPoints = 0;
			}
			else
			{
				UInt32 spellPower = caster.getBonus(school);
				UInt32 spellBonusPct = caster.getBonusPct(school);
				totalPoints = getSpellPointsTotal(effect, spellPower, spellBonusPct);
				if (hitInfos[i] == game::hit_info::CriticalHit)
				{
					crit = true;
					totalPoints *= 2.0f;
				}
			}
			
			// Update health value
			const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
			if (targetUnit->heal(totalPoints, &caster, noThreat))
			{
				// Send spell heal packet
				sendPacketFromCaster(caster,
					std::bind(game::server_write::spellHealLog, std::placeholders::_1,
					targetUnit->getGuid(),
					caster.getGuid(),
					m_spell.id(),
					totalPoints,
					crit));
			}
		}
	}

	void SingleCastState::spellEffectBind(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit*> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);
		
		for (int i=0; i<targets.size(); i++)
		{
			GameUnit* targetUnit = targets[i];
			if (targetUnit->getTypeId() == object_type::Character)
			{
				GameCharacter *character = dynamic_cast<GameCharacter*>(targetUnit);
				character->setHome(caster.getMapId(), caster.getLocation(), caster.getOrientation());
			}
		}
	}

	void SingleCastState::applyAllEffects()
	{
		// Make sure that this isn't destroyed during the effects
		auto strong = shared_from_this();

		// Execute spell immediatly
		namespace se = game::spell_effects;
		for (int i = 0; i < m_spell.effects_size(); ++i)
		{
			const auto &effect = m_spell.effects(i);
			switch (effect.type())
			{
			case se::InstantKill:
				spellEffectInstantKill(effect);
				break;
			case se::SchoolDamage:
				spellEffectSchoolDamage(effect);
				break;
			case se::PowerDrain:
				spellEffectDrainPower(effect);
				break;
			case se::Heal:
				spellEffectHeal(effect);
				break;
			case se::Bind:
				spellEffectBind(effect);
				break;
			case se::Proficiency:
				spellEffectProficiency(effect);
				break;
			case se::AddComboPoints:
				spellEffectAddComboPoints(effect);
				break;
			case se::Duel:
				spellEffectDuel(effect);
				break;
			case se::WeaponDamageNoSchool:
				spellEffectWeaponDamageNoSchool(effect);
			case se::CreateItem:
				spellEffectCreateItem(effect);
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
			case se::PowerBurn:
				spellEffectPowerBurn(effect);
				break;
			case se::Charge:
				spellEffectCharge(effect);
				break;
			case se::OpenLock:
				spellEffectOpenLock(effect);
				break;
			case se::ApplyAreaAuraParty:
				spellEffectApplyAreaAuraParty(effect);
				break;
			case se::Summon:
				spellEffectSummon(effect);
				break;
			case se::ScriptEffect:
				spellEffectScript(effect);
				break;
			default:
				WLOG("Spell effect " << game::constant_literal::spellEffectNames.getName(static_cast<game::SpellEffect>(effect.type())) << " (" << effect.type() << ") not yet implemented");
				break;
			}
		}

		// Cast all additional spells if available
		for (const auto &spell : m_spell.additionalspells())
		{
			m_cast.getExecuter().castSpell(m_target, spell, -1, 0, true);
		}
	}

	bool SingleCastState::consumePower()
	{
		UInt32 totalCost = 0;
		if (m_spell.cost() > 0)
		{
			totalCost = m_spell.cost();
		}
		else if (m_spell.costpct() > 0)
		{
            auto powerType = static_cast<game::PowerType>(m_spell.powertype());
			switch (powerType)
			{
				case game::power_type::Health:
					totalCost = m_cast.getExecuter().getUInt32Value(unit_fields::BaseHealth) * m_spell.costpct() / 100;
					break;
				case game::power_type::Mana:
					totalCost = m_cast.getExecuter().getUInt32Value(unit_fields::BaseMana) * m_spell.costpct() / 100;
					break;
				default:
					break;
			}
		}

		if (totalCost > 0)
		{
			if (m_spell.powertype() == game::power_type::Health)
			{
				// Special case
				UInt32 currentHealth = m_cast.getExecuter().getUInt32Value(unit_fields::Health);
				if (currentHealth < totalCost)
				{
					WLOG("Not enough health to cast spell!");

					sendEndCast(false);
					m_hasFinished = true;
					return false;
				}

				currentHealth -= totalCost;
				m_cast.getExecuter().setUInt32Value(unit_fields::Health, currentHealth);
			}
			else
			{
				UInt32 currentPower = m_cast.getExecuter().getUInt32Value(unit_fields::Power1 + m_spell.powertype());
				if (currentPower < totalCost)
				{
					WLOG("Not enough power to cast spell!");

					sendEndCast(false);
					m_hasFinished = true;
					return false;
				}

				currentPower -= totalCost;
				m_cast.getExecuter().setUInt32Value(unit_fields::Power1 + m_spell.powertype(), currentPower);

				if (m_spell.powertype() == game::power_type::Mana)
					m_cast.getExecuter().notifyManaUse();
			}
		}

		return true;
	}

	void SingleCastState::spellEffectTriggerSpell(const proto::SpellEffect &effect)
	{
		if (!effect.triggerspell())
		{
			WLOG("Spell " << m_spell.id() << ": No spell to trigger found! Trigger effect will be ignored.");
			return;
		}

		GameUnit &caster = m_cast.getExecuter();
		caster.castSpell(m_target, effect.triggerspell(), -1, 0, true);
	}

	void SingleCastState::spellEffectEnergize(const proto::SpellEffect &effect)
	{
		Int32 powerType = effect.miscvaluea();
		if (powerType < 0 || powerType > 5)
			return;
		
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit*> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);		//Buff, HoT

		for (int i=0; i<targets.size(); i++)
		{
			GameUnit* targetUnit = targets[i];

			UInt32 power = calculateEffectBasePoints(effect);
			if (victimStates[i] == game::victim_state::IsImmune)
			{
				power = 0;
			}

			UInt32 curPower = targetUnit->getUInt32Value(unit_fields::Power1 + powerType);
			UInt32 maxPower = targetUnit->getUInt32Value(unit_fields::MaxPower1 + powerType);
			if (curPower + power > maxPower)
			{
				curPower = maxPower;
			}
			else
			{
				curPower += power;
			}
			targetUnit->setUInt32Value(unit_fields::Power1 + powerType, curPower);
			sendPacketFromCaster(m_cast.getExecuter(),
				std::bind(game::server_write::spellEnergizeLog, std::placeholders::_1, 
					m_cast.getExecuter().getGuid(), targetUnit->getGuid(), m_spell.id(), static_cast<UInt8>(powerType), power));
		}
	}
	
	void SingleCastState::spellEffectPowerBurn(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit*> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);
		
		for (int i=0; i<targets.size(); i++)
		{
			GameUnit* targetUnit = targets[i];
			UInt8 school = m_spell.schoolmask();
			UInt32 burn;
			UInt32 damage = 0;
			UInt32 resisted = 0;
			UInt32 absorbed = 0;
			if (victimStates[i] == game::victim_state::IsImmune)
			{
				burn = 0;
			}
			if (hitInfos[i] == game::hit_info::Miss)
			{
				burn = 0;
			}
			else
			{
				burn = calculateEffectBasePoints(effect);
				resisted = burn * (resists[i] / 100.0f);
				burn -= resisted;
				burn = targetUnit->removeMana(burn);
				damage = burn * effect.multiplevalue();
				absorbed = targetUnit->consumeAbsorb(damage, school);
			}
			
			// Update health value
			const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
			if (targetUnit->dealDamage(damage - absorbed, school, &caster, noThreat))
			{
				// Send spell damage packet
				sendPacketFromCaster(caster,
					std::bind(game::server_write::spellNonMeleeDamageLog, std::placeholders::_1,
					targetUnit->getGuid(),
					caster.getGuid(),
					m_spell.id(),
					damage,
					school,
					absorbed,
					resisted,
					false,
					0,
					false));	//crit
				
				if (targetUnit->isAlive())
				{
					caster.doneSpellMagicDmgClassNeg(targetUnit, school);
					targetUnit->takenDamage(&caster);
				}
			}
		}
	}

	namespace
	{
		// TODO
		static UInt32 getLockId(const proto::ObjectEntry &entry)
		{
			UInt32 lockId = 0;

			switch (entry.type())
			{
				case 0:
				case 1:
					lockId = entry.data(1);
					break;
				case 2:
				case 3:
				case 6:
				case 10:
				case 12:
				case 13:
				case 24:
				case 26:
					lockId = entry.data(0);
					break;
				case 25:
					lockId = entry.data(4);
					break;
			}

			return lockId;
		}
	}
	
	void SingleCastState::spellEffectWeaponPercentDamage(const proto::SpellEffect &effect)
	{
		
	}

	void SingleCastState::spellEffectOpenLock(const proto::SpellEffect &effect)
	{
		// Try to get the target
		WorldObject *obj = nullptr;
		if (!m_target.hasGOTarget())
		{
			DLOG("TODO: SPELL_EFFECT_OPEN_LOCK without GO target");
			return;
		}

		auto *world = m_cast.getExecuter().getWorldInstance();
		if (!world)
		{
			return;
		}

		obj = dynamic_cast<WorldObject*>(world->findObjectByGUID(m_target.getGOTarget()));
		if (!obj)
		{
			WLOG("SPELL_EFFECT_OPEN_LOCK: Could not find target object");
			return;
		}

		UInt32 currentState = obj->getUInt32Value(world_object_fields::State);

		const auto &entry = obj->getEntry();
		UInt32 lockId = getLockId(entry);
		DLOG("Lock id: " << lockId);

		// TODO: Get lock info

		// If it is a door, try to open it
		if (entry.type() == 0)
		{
			obj->setUInt32Value(world_object_fields::State, (currentState == 1 ? 0 : 1));
			return;
		}
	}
	
	void SingleCastState::spellEffectApplyAreaAuraParty(const proto::SpellEffect &effect)
	{
		
	}

	void SingleCastState::spellEffectSummon(const proto::SpellEffect &effect)
	{
		const auto *entry = m_cast.getExecuter().getProject().units.getById(effect.summonunit());
		if (!entry)
		{
			WLOG("Can't summon anything - missing entry");
			return;
		}

		GameUnit &executer = m_cast.getExecuter();
		auto *world = executer.getWorldInstance();
		if (!world)
		{
			WLOG("Could not find world instance!");
			return;
		}

		float o = executer.getOrientation();
		math::Vector3 location(executer.getLocation());

		// TODO: We need to have access to unit entries

		auto spawned = world->spawnSummonedCreature(*entry, location, o);
		if (!spawned)
		{
			ELOG("Could not spawn creature!");
			return;
		}

		spawned->setUInt64Value(unit_fields::SummonedBy, executer.getGuid());
		world->addGameObject(*spawned);

		if (executer.getVictim())
		{
			spawned->threatened(*executer.getVictim(), 0.0001f);
		}
	}

	void wowpp::SingleCastState::spellEffectCharge(const proto::SpellEffect & effect)
	{
		Int32 basePoints = calculateEffectBasePoints(effect);
		DLOG("CHARGE (POINTS: " << basePoints << ")");
	}

	void wowpp::SingleCastState::spellEffectScript(const proto::SpellEffect & effect)
	{
		switch (m_spell.id())
		{
			case 20271:	// Judgment
				// aura = get active seal from aura_container (Dummy-effect)
				// m_cast.getExecuter().castSpell(target, aura.getBasePoints(), -1, 0, false);
				break;
			default:
				break;
		}
	}
}
