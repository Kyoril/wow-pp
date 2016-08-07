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
#include "common/make_unique.h"
#include "game_creature.h"
#include "universe.h"
#include "aura.h"
#include "unit_mover.h"

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
			    [&buffer, &packet](ITileSubscriber & subscriber)
			{
				subscriber.sendPacket(
				    packet,
				    buffer
				);
			});
		}
	}

	SingleCastState::SingleCastState(SpellCast &cast, const proto::SpellEntry &spell, SpellTargetMap target, Int32 basePoints, GameTime castTime, bool isProc/* = false*/, UInt64 itemGuid/* = 0*/)
		: m_cast(cast)
		, m_spell(spell)
		, m_target(std::move(target))
		, m_hasFinished(false)
		, m_countdown(cast.getTimers())
		, m_impactCountdown(cast.getTimers())
		, m_castTime(castTime)
		, m_castEnd(0)
		, m_basePoints(basePoints)
		, m_isProc(isProc)
		, m_attackTable()
		, m_itemGuid(itemGuid)
		, m_projectileStart(0)
		, m_projectileEnd(0)
		, m_connectedMeleeSignal(false)
		, m_delayCounter(0)
		, m_tookCastItem(false)
		, m_attackerProc(0)
		, m_victimProc(0)
	{
		// Check if the executer is in the world
		auto &executer = m_cast.getExecuter();
		auto *worldInstance = executer.getWorldInstance();

		auto const casterId = executer.getGuid();

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
			m_castEnd = getCurrentTime() + m_castTime;
			m_countdown.setEnd(m_castEnd);
			m_damaged = m_cast.getExecuter().takenDamage.connect([this](GameUnit *attacker, UInt32 damage) {
				if (!m_hasFinished && m_countdown.running)
				{
					if (m_spell.interruptflags() & game::spell_interrupt_flags::PushBack &&
						m_delayCounter < 2 &&
						m_cast.getExecuter().isGameCharacter())	// Pushback only works on characters
					{
						Int32 resistChance = 100;

						// GameCharacter type already checked above, as the pushback mechanic should only work on
						// player characters and not on creatures.
						reinterpret_cast<GameCharacter&>(m_cast.getExecuter()).applySpellMod(
							spell_mod_op::PreventSpellDelay, m_spell.id(), resistChance);
						resistChance += m_cast.getExecuter().getAuras().getTotalBasePoints(game::aura_type::ResistPushback) - 100;
						if (resistChance >= 100)
							return;

						std::uniform_int_distribution<Int32> resistRoll(0, 99);
						if (resistChance > resistRoll(randomGenerator))
						{
							return;
						}

						m_castEnd += 500;
						m_countdown.setEnd(m_castEnd);

						// Notify about spell delay
						sendPacketFromCaster(m_cast.getExecuter(),
							std::bind(game::server_write::spellDelayed, std::placeholders::_1, m_cast.getExecuter().getGuid(), 500));
						m_delayCounter++;
					}
					if (m_spell.interruptflags() & game::spell_interrupt_flags::Damage)
					{
						stopCast(game::spell_interrupt_flags::Damage);
					}
				}
			});

			WorldInstance *world = m_cast.getExecuter().getWorldInstance();
			assert(world);

			GameUnit *unitTarget = nullptr;
			m_target.resolvePointers(*world, &unitTarget, nullptr, nullptr, nullptr);
			if (unitTarget)
			{
				m_onTargetDied = unitTarget->killed.connect(std::bind(&SingleCastState::onTargetRemovedOrDead, this));
				m_onTargetRemoved = unitTarget->despawned.connect(std::bind(&SingleCastState::onTargetRemovedOrDead, this));
			}

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

	std::pair<game::SpellCastResult, SpellCasting *> SingleCastState::startCast(SpellCast &cast, const proto::SpellEntry &spell, SpellTargetMap target, Int32 basePoints, GameTime castTime, bool doReplacePreviousCast, UInt64 itemGuid)
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
		                            castTime,
		                            itemGuid);

		return std::make_pair(game::spell_cast_result::CastOkay, &casting);
	}

	void SingleCastState::stopCast(game::SpellInterruptFlags reason, UInt64 interruptCooldown/* = 0*/)
	{
		// Nothing to cancel
		if (m_hasFinished)
			return;

		// Flags do not match
		if (reason != game::spell_interrupt_flags::None &&
			(m_spell.interruptflags() & reason) == 0)
		{
			return;
		}

		m_countdown.cancel();
		sendEndCast(false);
		m_hasFinished = true;

		if (interruptCooldown)
		{
			applyCooldown(interruptCooldown, interruptCooldown);
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
				stopCast(game::spell_interrupt_flags::Movement);
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
			                               m_itemGuid ? m_itemGuid : executer.getGuid(),
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
			                               game::spell_cast_result::FailedBadTargets));

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
			character = dynamic_cast<GameCharacter *>(&m_cast.getExecuter());
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
					float maxrange = m_spell.maxrange();
					if (m_cast.getExecuter().isGameCharacter())
					{
						reinterpret_cast<GameCharacter&>(m_cast.getExecuter()).applySpellMod(spell_mod_op::Range, m_spell.id(), maxrange);
					}

					if (maxrange < m_spell.minrange()) maxrange = m_spell.minrange();

					const float combatReach = unitTarget->getFloatValue(unit_fields::CombatReach) + m_cast.getExecuter().getFloatValue(unit_fields::CombatReach);
					const float distance = m_cast.getExecuter().getDistanceTo(*unitTarget);
					if (m_spell.minrange() > 0.0f && distance < m_spell.minrange())
					{
						m_cast.getExecuter().spellCastError(m_spell, game::spell_cast_result::FailedTooClose);

						sendEndCast(false);
						m_hasFinished = true;

						return;
					}
					else if (maxrange > 0.0f && distance > maxrange + combatReach)
					{
						m_cast.getExecuter().spellCastError(m_spell, game::spell_cast_result::FailedOutOfRange);

						sendEndCast(false);
						m_hasFinished = true;

						return;
					}

					// Line of sight check
					if (!m_cast.getExecuter().isInLineOfSight(*unitTarget))
					{
						m_cast.getExecuter().spellCastError(m_spell, game::spell_cast_result::FailedLineOfSight);

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

		m_hasFinished = true;

		const std::weak_ptr<SingleCastState> weakThis = strongThis;
		const UInt32 spellAttributes = m_spell.attributes(0);
		if (spellAttributes & game::spell_attributes::OnNextSwing)
		{
			m_onAttackError = m_cast.getExecuter().autoAttackError.connect_extended([this](boost::signals2::connection c, AttackSwingError error) {
				if (error != attack_swing_error::Success)
				{
					game::SpellCastResult result = game::spell_cast_result::FailedError;
					switch (error)
					{
						case attack_swing_error::CantAttack:
							result = game::spell_cast_result::FailedBadTargets;
							break;
						case attack_swing_error::OutOfRange:
							result = game::spell_cast_result::FailedOutOfRange;
							break;
						case attack_swing_error::TargetDead:
							result = game::spell_cast_result::FailedTargetsDead;
							break;
						case attack_swing_error::WrongFacing:
							result = game::spell_cast_result::FailedError;
							break;
						default:
							result = game::spell_cast_result::FailedError;
					}

					m_cast.getExecuter().spellCastError(m_spell, result);
					sendEndCast(false);
				}

				c.disconnect();
			});

			// Execute on next weapon swing
			m_cast.getExecuter().setAttackSwingCallback([strongThis, this]() -> bool
			{
				m_onAttackError.disconnect();

				if (!strongThis->consumePower())
				{
					m_cast.getExecuter().spellCastError(m_spell, game::spell_cast_result::FailedNoPower);
					strongThis->sendEndCast(false);
					return false;
				}

				if (!strongThis->consumeItem())
				{
					m_cast.getExecuter().spellCastError(m_spell, game::spell_cast_result::FailedItemNotFound);
					strongThis->sendEndCast(false);
					return false;
				}

				strongThis->sendEndCast(true);
				strongThis->applyAllEffects();
				return true;
			});
		}
		else
		{
			if (!consumePower()) {
				return;
			}

			if (!consumeItem()) {
				return;
			}

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
						const GameTime timeMS = (dist / m_spell.speed()) * 1000;
						if (timeMS >= 50)
						{
							// This will be executed on the impact
							m_impactCountdown.ended.connect(
							    [strongThis]() mutable
							{
								strongThis->applyAllEffects();
								strongThis.reset();
							});

							m_projectileStart = getCurrentTime();
							m_projectileEnd = m_projectileStart + timeMS;
							m_projectileOrigin = m_cast.getExecuter().getLocation();

							m_onTargetMoved = unitTarget->moved.connect([this](GameObject & target, const math::Vector3 & oldPosition, float oldO) {
								if (m_impactCountdown.running)
								{
									const auto currentTime = getCurrentTime();
									auto targetLoc = target.getLocation();

									float percentage = static_cast<float>(currentTime - m_projectileStart) / static_cast<float>(m_projectileEnd - m_projectileStart);
									math::Vector3 projectilePos = m_projectileOrigin.lerp(oldPosition, percentage);
									const float dist = (targetLoc - projectilePos).length();
									const GameTime timeMS = (dist / m_spell.speed()) * 1000;

									m_projectileOrigin = projectilePos;
									m_projectileStart = currentTime;
									m_projectileEnd = currentTime + timeMS;

									if (timeMS >= 50)
									{
										m_impactCountdown.setEnd(currentTime + timeMS);
									}
									else
									{
										m_impactCountdown.cancel();
										applyAllEffects();
									}
								}
							});

							m_impactCountdown.setEnd(m_projectileEnd);
						}
						else
						{
							applyAllEffects();
						}
					}
				}
			}
			else
			{
				applyAllEffects();
			}
		}

		const UInt32 spellAttributesA = m_spell.attributes(1);
		// Consume combo points if required
		if ((spellAttributesA & game::spell_attributes_ex_a::ReqComboPoints_1) && character)
		{
			// 0 will reset combo points
			character->addComboPoints(0, 0);
		}

		// Start auto attack if required
		if (spellAttributesA & game::spell_attributes_ex_a::MeleeCombatStart)
		{
			GameUnit *attackTarget = nullptr;
			if (m_target.hasUnitTarget())
			{
				attackTarget = dynamic_cast<GameUnit *>(m_cast.getExecuter().getWorldInstance()->findObjectByGUID(m_target.getUnitTarget()));
			}

			if (attackTarget)
			{
				m_cast.getExecuter().setVictim(attackTarget);
				m_cast.getExecuter().startAttack();
			}
			else
			{
				WLOG("Unable to find target for auto attack after spell cast!");
			}
		}

		// Stop auto attack if required
		if (m_spell.attributes(0) & game::spell_attributes::StopAttackTarget)
		{
			m_cast.getExecuter().stopAttack();
		}

		if (weakThis.lock())
		{
			//may destroy this, too
			m_casting.ended(true);
		}
	}

	void SingleCastState::onTargetRemovedOrDead()
	{
		stopCast(game::spell_interrupt_flags::None);

		m_onTargetMoved.disconnect();
	}

	void SingleCastState::onUserDamaged()
	{
		// This is only triggered if the spell has the attribute
		stopCast(game::spell_interrupt_flags::Damage);
	}

	void SingleCastState::executeMeleeAttack()
	{
		GameUnit &attacker = m_cast.getExecuter();
		UInt8 school = m_spell.schoolmask();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpecialMeleeAttack(&attacker, m_spell, m_target, school, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			UInt32 totalDamage = m_meleeDamage[i];
			UInt32 blocked = 0;
			bool crit = false;
			UInt32 resisted = 0;
			UInt32 absorbed = 0;

			const game::VictimState &state = victimStates[i];
			if (state == game::victim_state::Blocks)
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

			// Apply damage bonus
			m_cast.getExecuter().applyDamageDoneBonus(school, 1, totalDamage);
			targetUnit->applyDamageTakenBonus(school, 1, totalDamage);

			// Update health value
			const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
			float threat = noThreat ? 0.0f : totalDamage - resisted - absorbed;
			if (targetUnit->dealDamage(totalDamage - resisted - absorbed, school, &attacker, threat))
			{
				std::map<UInt64, game::SpellMissInfo> missedTargets;
				if (state == game::victim_state::IsImmune)
				{
					missedTargets[targetUnit->getGuid()] = game::spell_miss_info::Immune;
				}
				else if (state == game::victim_state::Dodge)
				{
					missedTargets[targetUnit->getGuid()] = game::spell_miss_info::Dodge;
				}
				else if (hitInfos[i] == game::hit_info::Miss)
				{
					missedTargets[targetUnit->getGuid()] = game::spell_miss_info::Miss;
				}
				else if (state == game::victim_state::Parry)
				{
					missedTargets[targetUnit->getGuid()] = game::spell_miss_info::Parry;
				}

				if (missedTargets.empty())
				{
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
				}
				else
				{
					wowpp::sendPacketFromCaster(attacker,
						std::bind(game::server_write::spellLogMiss, std::placeholders::_1,
							m_spell.id(),
							attacker.getGuid(),
							0,
							std::cref(missedTargets)
							));
				}

				// TODO: Is this really needed? Since this signal is already fired in the dealDamage method
				//targetUnit->takenDamage(&attacker, totalDamage - resisted - absorbed);
			}
		}
	}

	void SingleCastState::spellEffectInstantKill(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			targetUnit->dealDamage(targetUnit->getUInt32Value(unit_fields::Health), m_spell.schoolmask(), &caster, 0.0f);
		}
	}

	void SingleCastState::spellEffectDummy(const proto::SpellEffect & effect)
	{
		// Get unit target by target map
		GameUnit *unitTarget = nullptr;
		if (!m_target.resolvePointers(*m_cast.getExecuter().getWorldInstance(), &unitTarget, nullptr, nullptr, nullptr))
		{
			return;
		}

		if (unitTarget)
		{
			m_affectedTargets.insert(unitTarget->shared_from_this());
		}

		if (m_spell.family() == 4)	// Warrior
		{
			if (m_spell.familyflags() == 0x20000000)		// Execute
			{
				// Rage has already been reduced by executing this spell, though the remaining value is the rest
				m_cast.getExecuter().castSpell(
					m_target, 20647, m_basePoints + m_cast.getExecuter().getUInt32Value(unit_fields::Power2) * effect.dmgmultiplier());
				m_cast.getExecuter().setUInt32Value(unit_fields::Power2, 0);
			}
		}
	}

	void SingleCastState::spellEffectTeleportUnits(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		UInt32 targetMap = 0;
		math::Vector3 targetPos(0.0f, 0.0f, 0.0f);
		float targetO = 0.0f;
		switch (effect.targetb())
		{
		case game::targets::DstHome:
			{
				if (caster.isGameCharacter())
				{
					GameCharacter *character = dynamic_cast<GameCharacter *>(&caster);
					character->getHome(targetMap, targetPos, targetO);
				}
				else
				{
					WLOG("Only characters do have a home point");
					return;
				}
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
		case game::targets::DstCaster:
			targetMap = caster.getMapId();
			targetPos = caster.getLocation();
			targetO = caster.getOrientation();
			break;
		default:
			WLOG("Unhandled destination type " << effect.targetb() << " - not teleporting!");
			return;
		}

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			if (targetUnit->isGameCharacter())
			{
				targetUnit->teleport(targetMap, targetPos, targetO);
			}
			else if (targetUnit->getMapId() == targetMap)
			{
				// Simply relocate creatures and other stuff
				targetUnit->relocate(targetPos, targetO);
			}
		}
	}

	void SingleCastState::spellEffectSchoolDamage(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		UInt8 school = m_spell.schoolmask();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			const game::VictimState &state = victimStates[i];
			UInt32 totalDamage;
			bool crit = false;
			UInt32 resisted = 0;
			UInt32 absorbed = 0;
			if (state == game::victim_state::IsImmune)
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
				
				// Apply damage bonus
				caster.applyDamageDoneBonus(school, 1, totalDamage);
				targetUnit->applyDamageTakenBonus(school, 1, totalDamage);

				if (caster.isGameCharacter())
				{
					auto added = reinterpret_cast<GameCharacter&>(caster).applySpellMod(
						spell_mod_op::Damage, m_spell.id(), totalDamage);
				}

				if (hitInfos[i] == game::hit_info::CriticalHit)
				{
					crit = true;
					totalDamage *= 2.0f;

					if (caster.isGameCharacter())
					{
						reinterpret_cast<GameCharacter&>(caster).applySpellMod(
							spell_mod_op::CritDamageBonus, m_spell.id(), totalDamage);
					}
				}
				resisted = totalDamage * (resists[i] / 100.0f);
				absorbed = targetUnit->consumeAbsorb(totalDamage - resisted, school);
			}

			// Update health value
			const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
			float threat = noThreat ? 0.0f : totalDamage - resisted - absorbed;
			if (!noThreat && m_cast.getExecuter().isGameCharacter())
			{
				reinterpret_cast<GameCharacter&>(m_cast.getExecuter()).applySpellMod(spell_mod_op::Threat, m_spell.id(), threat);
			}
			if (targetUnit->dealDamage(totalDamage - resisted - absorbed, school, &caster, threat))
			{
				if (totalDamage == 0 && resisted == 0) {
					totalDamage = resisted = 1;
				}

				// Send spell damage packet
				m_completedEffectsExecution[targetUnit->getGuid()] = completedEffects.connect([this, &caster, targetUnit, totalDamage, school, absorbed, resisted, crit, state]()
				{
					std::map<UInt64, game::SpellMissInfo> missedTargets;
					if (state == game::victim_state::IsImmune)
					{
						missedTargets[targetUnit->getGuid()] = game::spell_miss_info::Immune;
					}
					else if (state == game::victim_state::Dodge)
					{
						missedTargets[targetUnit->getGuid()] = game::spell_miss_info::Dodge;
					}
					
					if (missedTargets.empty())
					{
						wowpp::sendPacketFromCaster(caster,
							std::bind(game::server_write::spellNonMeleeDamageLog, std::placeholders::_1,
								targetUnit->getGuid(),
								caster.getGuid(),
								m_spell.id(),
								totalDamage,
								school,
								absorbed,
								resisted,
								false,
								0,
								crit));
					}
					else
					{
						wowpp::sendPacketFromCaster(caster,
							std::bind(game::server_write::spellLogMiss, std::placeholders::_1,
								m_spell.id(),
								caster.getGuid(),
								0,
								std::cref(missedTargets)
								));
					}
				});	// End connect

				//caster.spellProcEvent(m_attackerProc);
				//targetUnit->spellProcEvent(m_victimProc);
				//caster.doneSpellMagicDmgClassNeg(targetUnit, school);
				// TODO: Really needed? Because this signal is already fired in the dealDamage method
				//targetUnit->takenDamage(&caster);
			}
		}
	}

	void SingleCastState::spellEffectNormalizedWeaponDamage(const proto::SpellEffect &effect)
	{
		meleeSpecialAttack(effect, false);
	}

	void SingleCastState::spellEffectStealBeneficialBuff(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		UInt8 school = m_spell.schoolmask();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			UInt32 totalPoints = 0;
			bool spellFailed = false;

			if (hitInfos[i] == game::hit_info::Miss)
			{
				spellFailed = true;
			}
			else if (victimStates[i] == game::victim_state::IsImmune)
			{
				spellFailed = true;
			}
			else if (victimStates[i] == game::victim_state::Normal)
			{
				if (resists[i] == 100.0f)
				{
					spellFailed = true;
				}
				else
				{
					totalPoints = calculateEffectBasePoints(effect);
				}
			}

			if (spellFailed)
			{
				// TODO send fail packet
				sendPacketFromCaster(caster,
				                     std::bind(game::server_write::spellNonMeleeDamageLog, std::placeholders::_1,
				                               targetUnit->getGuid(),
				                               caster.getGuid(),
				                               m_spell.id(),
				                               1,
				                               school,
				                               0,
				                               1,	//resisted
				                               false,
				                               0,
				                               false));
			}
			else if (targetUnit->isAlive())
			{
				UInt32 auraDispelType = effect.miscvaluea();
				for (UInt32 i = 0; i < totalPoints; i++)
				{
					Aura *stolenAura = targetUnit->getAuras().popBack(auraDispelType, true);
					if (stolenAura)
					{
						proto::SpellEntry spell(stolenAura->getSpell());
						proto::SpellEffect effect(stolenAura->getEffect());
						UInt32 basepoints(stolenAura->getBasePoints());
						//stolenAura->misapplyAura();

						auto *world = caster.getWorldInstance();
						auto &universe = world->getUniverse();
						std::shared_ptr<Aura> aura = std::make_shared<Aura>(spell, effect, basepoints, caster, caster, m_itemGuid, [&universe](std::function<void()> work)
						{
							universe.post(work);
						}, [](Aura & self)
						{
							// Prevent aura from being deleted before being removed from the list
							auto strong = self.shared_from_this();

							// Remove aura from the list
							self.getTarget().getAuras().removeAura(self);
						});
						caster.getAuras().addAura(std::move(aura));
					}
					else
					{
						break;
					}
				}
			}
		}
	}

	void SingleCastState::spellEffectInterruptCast(const proto::SpellEffect & effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			targetUnit->cancelCast(game::spell_interrupt_flags::Interrupt, m_spell.duration());
		}
	}

	void SingleCastState::spellEffectLearnSpell(const proto::SpellEffect & effect)
	{
		if (!effect.triggerspell())
		{
			ELOG("SPELL_EFFECT_LEARN_SPELL: Missing trigger spell entry for effect #" << effect.index());
			return;
		}

		// Look for spell
		const auto *spell = m_cast.getExecuter().getProject().spells.getById(effect.triggerspell());
		if (!spell)
		{
			ELOG("SPELL_EFFECT_LEARN_SPELL: Could not find spell " << effect.triggerspell());
			return;
		}

		GameUnit *unitTarget = nullptr;
		if (!m_target.resolvePointers(
			*m_cast.getExecuter().getWorldInstance(),
			&unitTarget,
			nullptr,
			nullptr,
			nullptr))
		{
			return;
		}

		if (!unitTarget)
		{
			return;
		}

		m_affectedTargets.insert(unitTarget->shared_from_this());
		if (unitTarget->isGameCharacter())
		{
			auto *character = reinterpret_cast<GameCharacter*>(unitTarget);
			if (character->addSpell(*spell))
			{
				// Activate passive spell if it is one
				if (spell->attributes(0) & game::spell_attributes::Passive)
				{
					SpellTargetMap targetMap;
					targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
					targetMap.m_unitTarget = character->getGuid();
					character->castSpell(std::move(targetMap), effect.triggerspell(), -1, 0, true);
				}

				// TODO: Send packets
			}
		}
	}

	void SingleCastState::spellEffectScriptEffect(const proto::SpellEffect & effect)
	{
		// TODO: Do something here...
		UInt32 spellId = 0;
		switch (m_spell.id())
		{
			case 25140:
				spellId = 32571;
				break;
			case 25143:
				spellId = 32572;
				break;
			case 25650:
				spellId = 30140;
				break;
			case 25652:
				spellId = 30141;
				break;
			case 29128:
				spellId = 32568;
				break;
			case 29129:
				spellId = 32569;
				break;
			case 35376:
				spellId = 25649;
				break;
			case 35727:
				spellId = 35730;
				break;
			default:
				WLOG("Unhandled SPELL_EFFECT_SCRIPT_EFFECT for spell " << m_spell.id());
				return;
		}

		if (spellId != 0)
		{
			// Look for the spell
			const auto *entry = m_cast.getExecuter().getProject().spells.getById(spellId);
			if (!entry)
			{
				return;
			}

			// Get the cast time of this spell
			Int64 castTime = entry->casttime();
			if (m_cast.getExecuter().isGameCharacter())
			{
				reinterpret_cast<GameCharacter&>(m_cast.getExecuter()).applySpellMod(spell_mod_op::CastTime, spellId, castTime);
			}
			if (castTime < 0) castTime = 0;

			m_cast.getExecuter().castSpell(m_target, spellId, -1, castTime, false);
		}
	}

	void SingleCastState::spellEffectDispelMechanic(const proto::SpellEffect & effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			targetUnit->getAuras().removeAllAurasDueToMechanic(1 << effect.miscvaluea());
		}
	}

	void SingleCastState::spellEffectResurrect(const proto::SpellEffect & effect)
	{
		if (!isPlayerGUID(m_target.getUnitTarget()))
		{
			return;
		}

		auto *world = m_cast.getExecuter().getWorldInstance();

		if (!world)
		{
			return;
		}

		GameUnit &caster = m_cast.getExecuter();
		GameUnit *targetUnit = nullptr;
		m_target.resolvePointers(*world, &targetUnit, nullptr, nullptr, nullptr);

		if (!targetUnit || targetUnit->isAlive())
		{
			return;
		} 

		auto *target = reinterpret_cast<GameCharacter *>(targetUnit);

		if (target->isResurrectRequested())
		{
			return;
		}

		UInt32 health = target->getUInt32Value(unit_fields::MaxHealth) * calculateEffectBasePoints(effect) / 100;
		UInt32 mana = target->getUInt32Value(unit_fields::Power1) * calculateEffectBasePoints(effect) / 100;

		target->setResurrectRequestData(caster.getGuid(), caster.getMapId(), caster.getLocation(), health, mana);
		target->resurrectRequested(caster.getGuid(), caster.getName(), caster.isGameCharacter() ? object_type::Character : object_type::Unit);
	}

	void SingleCastState::spellEffectResurrectNew(const proto::SpellEffect & effect)
	{
		if (!isPlayerGUID(m_target.getUnitTarget()))
		{
			return;
		}

		auto *world = m_cast.getExecuter().getWorldInstance();

		if (!world)
		{
			return;
		}

		GameUnit &caster = m_cast.getExecuter();
		GameUnit *targetUnit = nullptr;
		m_target.resolvePointers(*world, &targetUnit, nullptr, nullptr, nullptr);

		if (!targetUnit || targetUnit->isAlive())
		{
			return;
		}

		auto *target = reinterpret_cast<GameCharacter *>(targetUnit);

		if (target->isResurrectRequested())
		{
			return;
		}

		UInt32 health = calculateEffectBasePoints(effect);
		UInt32 mana = effect.miscvaluea();

		target->setResurrectRequestData(caster.getGuid(), caster.getMapId(), caster.getLocation(), health, mana);
		target->resurrectRequested(caster.getGuid(), caster.getName(), caster.isGameCharacter() ? object_type::Character : object_type::Unit);
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

		if (m_target.getTargetMap() == game::spell_cast_target_flags::Self) {
			target = &caster;
		}
		else if (world)
		{
			UInt64 targetGuid = 0;
			if (m_target.hasUnitTarget()) {
				targetGuid = m_target.getUnitTarget();
			}
			else if (m_target.hasGOTarget()) {
				targetGuid = m_target.getGOTarget();
			}
			else if (m_target.hasItemTarget()) {
				targetGuid = m_target.getItemTarget();
			}

			if (targetGuid != 0) {
				target = world->findObjectByGUID(targetGuid);
			}

			if (m_target.hasUnitTarget() && isUnitGUID(targetGuid)) {
				unitTarget = reinterpret_cast<GameUnit *>(target);
			}
		}

		// Check target
		if (!unitTarget)
		{
			WLOG("EFFECT_POWER_DRAIN: No valid target found!");
			return;
		}

		m_affectedTargets.insert(unitTarget->shared_from_this());
		unitTarget->threaten(caster, 0.0f);

		// Does this have any effect on the target?
		if (unitTarget->getByteValue(unit_fields::Bytes0, 3) != powerType) {
			return;    // Target does not use this kind of power
		}
		if (powerToDrain == 0) {
			return;    // Nothing to drain...
		}

		UInt32 currentPower = unitTarget->getUInt32Value(unit_fields::Power1 + powerType);
		if (currentPower == 0) {
			return;    // Target doesn't have any power left
		}

		// Now drain the power
		if (powerToDrain > currentPower) {
			powerToDrain = currentPower;
		}

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
			if (casterPower + powerToDrain > maxCasterPower) {
				powerToDrain = maxCasterPower - casterPower;
			}
			caster.setUInt32Value(unit_fields::Power1 + casterPowerType, casterPower + powerToDrain);
		}
	}

	void SingleCastState::spellEffectProficiency(const proto::SpellEffect &effect)
	{
		// Self target
		GameCharacter *character = nullptr;
		if (isPlayerGUID(m_cast.getExecuter().getGuid()))
		{
			character = dynamic_cast<GameCharacter *>(&m_cast.getExecuter());
		}

		if (!character)
		{
			WLOG("SPELL_EFFECT_PROFICIENCY: Requires character unit target!");
			return;
		}

		m_affectedTargets.insert(character->shared_from_this());

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
			character = dynamic_cast<GameCharacter *>(&m_cast.getExecuter());
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

		Int32 outBasePoints = basePoints + randomValue + comboDamage;
		if (m_cast.getExecuter().isGameCharacter())
		{
			switch (effect.index())
			{
				case 0:
					reinterpret_cast<GameCharacter&>(m_cast.getExecuter()).applySpellMod(
						spell_mod_op::Effect1, m_spell.id(), outBasePoints);
					break;
				case 1:
					reinterpret_cast<GameCharacter&>(m_cast.getExecuter()).applySpellMod(
						spell_mod_op::Effect2, m_spell.id(), outBasePoints);
					break;
				case 2:
					reinterpret_cast<GameCharacter&>(m_cast.getExecuter()).applySpellMod(
						spell_mod_op::Effect3, m_spell.id(), outBasePoints);
					break;
			}
			
			// Apply AllEffects mod
			reinterpret_cast<GameCharacter&>(m_cast.getExecuter()).applySpellMod(
				spell_mod_op::AllEffects, m_spell.id(), outBasePoints);
		}

		if (basePointsPerLevel == 0.0f &&
			m_spell.attributes(0) & game::spell_attributes::LevelDamageCalc)
		{
			bool applyLevelDamageCalc = false;

			const auto sl = m_spell.spelllevel();
			switch (effect.type())
			{
				case game::spell_effects::SchoolDamage:
				case game::spell_effects::WeaponDamage:
				case game::spell_effects::WeaponDamageNoSchool:
				case game::spell_effects::EnvironmentalDamage:
					applyLevelDamageCalc = true;
					break;
				case game::spell_effects::ApplyAura:
					switch (effect.aura())
					{
						case game::aura_type::PeriodicDamage:
							applyLevelDamageCalc = true;
							break;
					}
					break;
				default:
					applyLevelDamageCalc = false;
					break;
			}

			if (applyLevelDamageCalc)
			{
				outBasePoints = Int32(outBasePoints * m_cast.getExecuter().getLevel() / (sl ? sl : 1));
			}
		}

		return outBasePoints;
	}

	UInt32 SingleCastState::getSpellPointsTotal(const proto::SpellEffect &effect, UInt32 spellPower, UInt32 bonusPct)
	{
		Int32 basePoints = calculateEffectBasePoints(effect);
		float castTime = m_castTime;
		if (castTime < 1500) {
			castTime = 1500;
		}
		float spellAddCoefficient = (castTime / 3500.0);
		float bonusModificator = 1 + (bonusPct / 100);
		return (basePoints + (spellAddCoefficient * spellPower)) *  bonusModificator;
	}

	void SingleCastState::spellEffectAddComboPoints(const proto::SpellEffect &effect)
	{
		GameCharacter *character = nullptr;
		if (isPlayerGUID(m_cast.getExecuter().getGuid()))
		{
			character = dynamic_cast<GameCharacter *>(&m_cast.getExecuter());
		}

		if (!character)
		{
			ELOG("Invalid character");
			return;
		}

		m_affectedTargets.insert(character->shared_from_this());

		UInt64 comboTarget = m_target.getUnitTarget();
		character->addComboPoints(comboTarget, UInt8(calculateEffectBasePoints(effect)));
	}

	void SingleCastState::spellEffectDuel(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		if (!caster.isGameCharacter())
		{
			// This spell effect is only usable by player characters right now
			ELOG("Caster is not a game character!");
			return;
		}

		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		// Did we find at least one valid target?
		if (targets.empty())
		{
			WLOG("No targets found");
			return;
		}

		// Get the first target
		GameUnit *targetUnit = targets.front();
		assert(targetUnit);

		// Check if target is a character
		if (!targetUnit->isGameCharacter())
		{
			// Skip this target
			WLOG("Target is not a character");
			return;
		}

		// Check if target is already dueling
		if (targetUnit->getUInt64Value(character_fields::DuelArbiter))
		{
			// Target is already dueling
			WLOG("Target is already dueling");
			return;
		}

		// Target is dead
		if (!targetUnit->isAlive())
		{
			WLOG("Target is dead");
			return;
		}

		// We affect this target
		m_affectedTargets.insert(targetUnit->shared_from_this());

		// Find flag object entry
		auto &project = targetUnit->getProject();
		const auto *entry = project.objects.getById(effect.miscvaluea());
		if (!entry)
		{
			ELOG("Could not find duel arbiter object: " << effect.miscvaluea());
			return;
		}

		// Spawn new duel arbiter flag
		if (auto *world = caster.getWorldInstance())
		{
			auto flagObject = world->spawnWorldObject(
				*entry,
				caster.getLocation(),
				0.0f,
				0.0f
				);
			flagObject->setUInt32Value(world_object_fields::AnimProgress, 0);
			flagObject->setUInt32Value(world_object_fields::Level, caster.getLevel());
			flagObject->setUInt32Value(world_object_fields::Faction, caster.getFactionTemplate().id());
			world->addGameObject(*flagObject);

			// Save this object to prevent it from being deleted immediatly
			caster.addWorldObject(flagObject);

			// Save them
			caster.setUInt64Value(character_fields::DuelArbiter, flagObject->getGuid());
			targetUnit->setUInt64Value(character_fields::DuelArbiter, flagObject->getGuid());
			DLOG("Duel arbiter spawned: " << flagObject->getGuid());
		}
	}

	void SingleCastState::spellEffectWeaponDamageNoSchool(const proto::SpellEffect &effect)
	{
		//TODO: Implement
		meleeSpecialAttack(effect, false);
	}

	void SingleCastState::spellEffectCreateItem(const proto::SpellEffect &effect)
	{
		// Get item entry
		const auto *item = m_cast.getExecuter().getProject().items.getById(effect.itemtype());
		if (!item)
		{
			ELOG("SPELL_EFFECT_CREATE_ITEM: Could not find item by id " << effect.itemtype());
			return;
		}

		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;

		m_attackTable.checkPositiveSpellNoCrit(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);
		const auto itemCount = calculateEffectBasePoints(effect);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			if (targetUnit->isGameCharacter())
			{
				auto *charUnit = reinterpret_cast<GameCharacter *>(targetUnit);
				auto &inv = charUnit->getInventory();

				std::map<UInt16, UInt16> addedBySlot;
				auto result = inv.createItems(*item, itemCount, &addedBySlot);
				if (result != game::inventory_change_failure::Okay)
				{
					charUnit->inventoryChangeFailure(result, nullptr, nullptr);
					continue;
				}

				// Send item notification
				for (auto &slot : addedBySlot)
				{
					auto inst = inv.getItemAtSlot(slot.first);
					if (inst)
					{
						UInt8 bag = 0, subslot = 0;
						Inventory::getRelativeSlots(slot.first, bag, subslot);
						const auto totalCount = inv.getItemCount(item->id());

						TileIndex2D tile;
						if (charUnit->getTileIndex(tile))
						{
							std::vector<char> buffer;
							io::VectorSink sink(buffer);
							game::Protocol::OutgoingPacket itemPacket(sink);
							game::server_write::itemPushResult(itemPacket, charUnit->getGuid(), std::cref(*inst), false, true, bag, subslot, slot.second, totalCount);
							forEachSubscriberInSight(
							    charUnit->getWorldInstance()->getGrid(),
							    tile,
							    [&](ITileSubscriber & subscriber)
							{
								auto subscriberGroup = subscriber.getControlledObject()->getGroupId();
								if ((charUnit->getGroupId() == 0 && subscriber.getControlledObject()->getGuid() == charUnit->getGuid()) ||
								        (charUnit->getGroupId() != 0 && subscriberGroup == charUnit->getGroupId())
								   )
								{
									subscriber.sendPacket(itemPacket, buffer);
								}
							});
						}
					}
				}
			}
		}
	}

	void SingleCastState::spellEffectWeaponDamage(const proto::SpellEffect &effect)
	{
		//TODO: Implement
		meleeSpecialAttack(effect, false);
	}

	void SingleCastState::spellEffectApplyAura(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		bool isPositive = Aura::isPositive(m_spell, effect);
		UInt8 school = m_spell.schoolmask();

		if (isPositive)
		{
			m_attackTable.checkPositiveSpellNoCrit(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);	//Buff
		}
		else
		{
			m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);	//Debuff
		}

		UInt32 aura = effect.aura();
		bool modifiedByBonus;
		switch (aura)
		{
		case game::aura_type::PeriodicDamage:
		case game::aura_type::PeriodicHeal:
		case game::aura_type::PeriodicLeech:
			modifiedByBonus = true;
		default:
			modifiedByBonus = false;
		}

		// World was already checked. If world would be nullptr, unitTarget would be null as well
		auto *world = m_cast.getExecuter().getWorldInstance();
		auto &universe = world->getUniverse();
		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			UInt32 totalPoints = 0;
			game::SpellMissInfo missInfo = game::spell_miss_info::None;
			bool spellFailed = false;

			if (hitInfos[i] == game::hit_info::Miss)
			{
				missInfo = game::spell_miss_info::Miss;
			}
			else if (victimStates[i] == game::victim_state::IsImmune)
			{
				missInfo = game::spell_miss_info::Immune;
			}
			else if (victimStates[i] == game::victim_state::Normal)
			{
				if (resists[i] == 100.0f)
				{
					missInfo = game::spell_miss_info::Resist;
				}
				else
				{
					if (modifiedByBonus)
					{
						UInt32 spellPower = caster.getBonus(school);
						UInt32 spellBonusPct = caster.getBonusPct(school);
						totalPoints = getSpellPointsTotal(effect, spellPower, spellBonusPct);
						totalPoints -= totalPoints * (resists[i] / 100.0f);
					}
					else
					{
						totalPoints = calculateEffectBasePoints(effect);
					}
				}
			}

			if (missInfo != game::spell_miss_info::None)
			{
				m_completedEffectsExecution[targetUnit->getGuid()] = completedEffects.connect([this, &caster, targetUnit, school, missInfo]()
				{
					std::map<UInt64, game::SpellMissInfo> missedTargets;
					missedTargets[targetUnit->getGuid()] = missInfo;

					wowpp::sendPacketFromCaster(caster,
						std::bind(game::server_write::spellLogMiss, std::placeholders::_1,
							m_spell.id(),
							caster.getGuid(),
							0,
							std::cref(missedTargets)));
				});	// End connect
			}
			else if (targetUnit->isAlive())
			{
				std::shared_ptr<Aura> aura = std::make_shared<Aura>(m_spell, effect, totalPoints, caster, *targetUnit, m_itemGuid, [&universe](std::function<void()> work)
				{
					universe.post(work);
				}, [&universe](Aura & self)
				{
					// Prevent aura from being deleted before being removed from the list
					auto strong = self.shared_from_this();
					universe.post([strong]()
					{
						strong->getTarget().getAuras().removeAura(*strong);
					});
				});

				// TODO: Dimishing return and custom durations

				// TODO: Apply spell haste

				// TODO: Check if aura already expired

				const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
				if (!noThreat)
				{
					targetUnit->threaten(caster, 0.0f);
				}

				// TODO: Add aura to unit target
				const bool success = targetUnit->getAuras().addAura(std::move(aura));
				if (!success)
				{
					// TODO: What should we do here? Just ignore?
					WLOG("Aura could not be added to unit target!");
				}

				// We need to be sitting for this aura to work
				if (m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::NotSeated)
				{
					// Sit down
					caster.setStandState(unit_stand_state::Sit);
				}
			}
		}

		// If auras should be removed on immunity, do so!
		if (aura == game::aura_type::MechanicImmunity &&
			(m_spell.attributes(1) & game::spell_attributes_ex_a::DispelAurasOnImmunity) != 0)
		{
			if (!m_removeAurasOnImmunity.connected())
			{
				m_removeAurasOnImmunity = completedEffects.connect([this] {
					UInt32 immunityMask = 0;
					for (Int32 i = 0; i < m_spell.effects_size(); ++i)
					{
						if (m_spell.effects(i).type() == game::spell_effects::ApplyAura &&
							m_spell.effects(i).aura() == game::aura_type::MechanicImmunity)
						{
							immunityMask |= (1 << m_spell.effects(i).miscvaluea());
						}
					}

					for (auto &target : m_affectedTargets)
					{
						auto strong = target.lock();
						if (strong)
						{
							auto unit = std::static_pointer_cast<GameUnit>(strong);
							unit->getAuras().removeAllAurasDueToMechanic(immunityMask);
						}
					}
				});
			}
		}
	}

	void SingleCastState::spellEffectHeal(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);		//Buff, HoT

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

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

				caster.applyHealingDoneBonus(m_spell.spelllevel(), caster.getLevel(), 1, totalPoints);
				targetUnit->applyHealingTakenBonus(1, totalPoints);

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

			//caster.spellProcEvent(m_attackerProc);
			//targetUnit->spellProcEvent(m_victimProc);
		}
	}

	void SingleCastState::spellEffectBind(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;

		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			if (targetUnit->isGameCharacter())
			{
				GameCharacter *character = dynamic_cast<GameCharacter *>(targetUnit);
				character->setHome(caster.getMapId(), caster.getLocation(), caster.getOrientation());
			}
		}
	}

	void SingleCastState::spellEffectQuestComplete(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;

		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			if (targetUnit->isGameCharacter())
			{
				reinterpret_cast<GameCharacter*>(targetUnit)->completeQuest(effect.miscvaluea());
			}
		}
	}

	void SingleCastState::applyAllEffects()
	{
		// Add spell cooldown if any
		{
			UInt64 spellCatCD = m_spell.categorycooldown();
			UInt64 spellCD = m_spell.cooldown();

			// If cast by an item, the item cooldown is used instead of the spell cooldown
			if (m_itemGuid && m_cast.getExecuter().isGameCharacter())
			{
				auto *character = reinterpret_cast<GameCharacter *>(&m_cast.getExecuter());
				auto &inv = character->getInventory();

				UInt16 itemSlot = 0;
				if (inv.findItemByGUID(m_itemGuid, itemSlot))
				{
					auto item = inv.getItemAtSlot(itemSlot);
					if (item)
					{
						for (auto &spell : item->getEntry().spells())
						{
							if (spell.spell() == m_spell.id() &&
								(spell.trigger() == 0 || spell.trigger() == 5))
							{
								if (spell.categorycooldown() > 0 ||
									spell.cooldown() > 0)
								{
									// Use item cooldown instead of spell cooldown
									spellCatCD = spell.categorycooldown();
									spellCD = spell.cooldown();
								}
								break;
							}
						}
					}
				}
			}

			UInt64 finalCD = spellCD;
			if (!finalCD) {
				finalCD = spellCatCD;
			}

			if (m_cast.getExecuter().isGameCharacter())
			{
				reinterpret_cast<GameCharacter&>(m_cast.getExecuter()).applySpellMod(
					spell_mod_op::Cooldown, m_spell.id(), finalCD);
			}

			if (finalCD)
			{
				applyCooldown(finalCD, spellCatCD);
			}
		}

		// Make sure that this isn't destroyed during the effects
		auto strong = shared_from_this();

		std::vector<UInt32> effects;
		for (int i = 0; i < m_spell.effects_size(); ++i)
		{
			effects.push_back(m_spell.effects(i).type());
		}

		if (!(m_spell.attributes(0) & game::spell_attributes::Passive))
		{
			switch (m_spell.dmgclass())
			{
				case game::spell_dmg_class::Melee:
					m_attackerProc = game::spell_proc_flags::DoneSpellMeleeDmgClass;
					m_victimProc = game::spell_proc_flags::TakenSpellMeleeDmgClass;

					if (m_spell.attributes(3) & game::spell_attributes_ex_c::ReqOffhand)
					{
						m_attackerProc |= game::spell_proc_flags::DoneOffhandAttack;
					}
					break;
				case game::spell_dmg_class::Ranged:
					if (m_spell.attributes(2) & game::spell_attributes_ex_b::AuroRepeat)
					{
						m_attackerProc = game::spell_proc_flags::DoneRangedAutoAttack;
						m_victimProc = game::spell_proc_flags::TakenRangedAutoAttack;
					}
					else
					{
						m_attackerProc = game::spell_proc_flags::DoneSpellRangedDmgClass;
						m_victimProc = game::spell_proc_flags::TakenSpellRangedDmgClass;
					}
					break;
				default:
					bool isPositive = false;

					for (const auto &effect : m_spell.effects())
					{
						if (Aura::isPositive(m_spell, effect))
						{
							isPositive = true;
						}
						else
						{
							isPositive = false;
							break;
						}
					}

					if (isPositive)
					{
						m_attackerProc = game::spell_proc_flags::DoneSpellMagicDmgClassPos;
						m_victimProc = game::spell_proc_flags::TakenSpellMagicDmgClassPos;
					}
					else if (m_spell.attributes(2) & game::spell_attributes_ex_b::AuroRepeat)
					{
						m_attackerProc = game::spell_proc_flags::DoneRangedAutoAttack;
						m_victimProc = game::spell_proc_flags::TakenRangedAutoAttack;
					}
					else
					{
						m_attackerProc = game::spell_proc_flags::DoneSpellMagicDmgClassNeg;
						m_victimProc = game::spell_proc_flags::TakenSpellMagicDmgClassNeg;
					}

					break;
			}

			GameUnit *target = nullptr;

			if (m_target.hasUnitTarget())
			{
				auto *world = m_cast.getExecuter().getWorldInstance();
				if (!world)
				{
					return;
				}

				GameObject *obj = world->findObjectByGUID(m_target.getUnitTarget());
				target = reinterpret_cast<GameUnit*>(obj);
			}

			m_cast.getExecuter().spellProcEvent(m_attackerProc, target, &m_spell);
			
		}

		// Execute spell immediatly
		namespace se = game::spell_effects;
		std::vector<std::pair<UInt32, EffectHandler>> effectMap {
			//ordered pairs to avoid 25% resists for binary spells like frostnova
			{se::Dummy,					std::bind(&SingleCastState::spellEffectDummy, this, std::placeholders::_1) },
			{se::InstantKill,			std::bind(&SingleCastState::spellEffectInstantKill, this, std::placeholders::_1)},
			{se::PowerDrain,			std::bind(&SingleCastState::spellEffectDrainPower, this, std::placeholders::_1)},
			{se::Heal,					std::bind(&SingleCastState::spellEffectHeal, this, std::placeholders::_1)},
			{se::Bind,					std::bind(&SingleCastState::spellEffectBind, this, std::placeholders::_1)},
			{se::QuestComplete,			std::bind(&SingleCastState::spellEffectQuestComplete, this, std::placeholders::_1)},
			{se::Proficiency,			std::bind(&SingleCastState::spellEffectProficiency, this, std::placeholders::_1)},
			{se::AddComboPoints,		std::bind(&SingleCastState::spellEffectAddComboPoints, this, std::placeholders::_1)},
			{se::Duel,					std::bind(&SingleCastState::spellEffectDuel, this, std::placeholders::_1)},
			{se::WeaponDamageNoSchool,	std::bind(&SingleCastState::spellEffectWeaponDamageNoSchool, this, std::placeholders::_1)},
			{se::CreateItem,			std::bind(&SingleCastState::spellEffectCreateItem, this, std::placeholders::_1)},
			{se::WeaponDamage,			std::bind(&SingleCastState::spellEffectWeaponDamage, this, std::placeholders::_1)},
			{se::TeleportUnits,			std::bind(&SingleCastState::spellEffectTeleportUnits, this, std::placeholders::_1)},
			{se::TriggerSpell,			std::bind(&SingleCastState::spellEffectTriggerSpell, this, std::placeholders::_1)},
			{se::Energize,				std::bind(&SingleCastState::spellEffectEnergize, this, std::placeholders::_1)},
			{se::WeaponPercentDamage,	std::bind(&SingleCastState::spellEffectWeaponPercentDamage, this, std::placeholders::_1)},
			{se::PowerBurn,				std::bind(&SingleCastState::spellEffectPowerBurn, this, std::placeholders::_1)},
			{se::Charge,				std::bind(&SingleCastState::spellEffectCharge, this, std::placeholders::_1)},
			{se::OpenLock,				std::bind(&SingleCastState::spellEffectOpenLock, this, std::placeholders::_1)},
			{se::OpenLockItem,			std::bind(&SingleCastState::spellEffectOpenLock, this, std::placeholders::_1) },
			{se::ApplyAreaAuraParty,	std::bind(&SingleCastState::spellEffectApplyAreaAuraParty, this, std::placeholders::_1)},
			{se::Dispel,				std::bind(&SingleCastState::spellEffectDispel, this, std::placeholders::_1)},
			{se::Summon,				std::bind(&SingleCastState::spellEffectSummon, this, std::placeholders::_1)},
			{se::SummonPet,				std::bind(&SingleCastState::spellEffectSummonPet, this, std::placeholders::_1) },
			{se::ScriptEffect,			std::bind(&SingleCastState::spellEffectScript, this, std::placeholders::_1)},
			{se::AttackMe,				std::bind(&SingleCastState::spellEffectAttackMe, this, std::placeholders::_1)},
			{se::NormalizedWeaponDmg,	std::bind(&SingleCastState::spellEffectNormalizedWeaponDamage, this, std::placeholders::_1)},
			{se::StealBeneficialBuff,	std::bind(&SingleCastState::spellEffectStealBeneficialBuff, this, std::placeholders::_1)},
			{se::InterruptCast,			std::bind(&SingleCastState::spellEffectInterruptCast, this, std::placeholders::_1) },
			{se::LearnSpell,			std::bind(&SingleCastState::spellEffectLearnSpell, this, std::placeholders::_1) },
			{se::ScriptEffect,			std::bind(&SingleCastState::spellEffectScriptEffect, this, std::placeholders::_1) },
			{se::DispelMechanic,		std::bind(&SingleCastState::spellEffectDispelMechanic, this, std::placeholders::_1) },
			{se::Resurrect,				std::bind(&SingleCastState::spellEffectResurrect, this, std::placeholders::_1) },
			{se::ResurrectNew,			std::bind(&SingleCastState::spellEffectResurrectNew, this, std::placeholders::_1) },
			// Add all effects above here
			{se::ApplyAura,				std::bind(&SingleCastState::spellEffectApplyAura, this, std::placeholders::_1)},
			{se::ApplyAreaAuraParty,	std::bind(&SingleCastState::spellEffectApplyAura, this, std::placeholders::_1)},
			{se::SchoolDamage,			std::bind(&SingleCastState::spellEffectSchoolDamage, this, std::placeholders::_1)}
		};

		// Make sure that the executer exists after all effects have been executed
		std::weak_ptr<GameObject> weakExecuter(m_cast.getExecuter().shared_from_this());
		for (std::vector<std::pair<UInt32, EffectHandler>>::iterator it = effectMap.begin(); it != effectMap.end(); ++it)
		{
			for (int k = 0; k < effects.size(); ++k)
			{
				if (it->first == effects[k])
				{
					assert(it->second);
					it->second(m_spell.effects(k));
				}
			}
		}

		completedEffects();

		if (auto strong = weakExecuter.lock())
		{
			// Cast all additional spells if available
			auto strongUnit = std::dynamic_pointer_cast<GameUnit>(strong);
			assert(strongUnit);

			for (const auto &spell : m_spell.additionalspells())
			{
				strongUnit->castSpell(m_target, spell, -1, 0, true);
			}
		}

		if (auto strong = weakExecuter.lock())
		{
			auto strongUnit = std::dynamic_pointer_cast<GameUnit>(strong);
			assert(strongUnit);

			if (strongUnit->isGameCharacter())
			{
				for (const auto &target : m_affectedTargets)
				{
					auto strongTarget = target.lock();
					if (strongTarget)
					{
						if (strongTarget->isCreature())
						{
							std::static_pointer_cast<GameCreature>(strongTarget)->raiseTrigger(
								trigger_event::OnSpellHit, { m_spell.id() });
						}

						reinterpret_cast<GameCharacter&>(*strongUnit).onQuestSpellCastCredit(m_spell.id(), *strongTarget);
					}
				}
			}
		}
	}

	bool SingleCastState::consumeItem(bool delayed/* = true*/)
	{
		if (m_tookCastItem && delayed)
			return true;
		
		if (m_itemGuid != 0 && m_cast.getExecuter().isGameCharacter())
		{
			auto *character = reinterpret_cast<GameCharacter *>(&m_cast.getExecuter());
			auto &inv = character->getInventory();

			UInt16 itemSlot = 0;
			if (!inv.findItemByGUID(m_itemGuid, itemSlot))
			{
				return false;
			}

			auto item = inv.getItemAtSlot(itemSlot);
			if (!item)
			{
				return false;
			}

			auto removeItem = [this, item, itemSlot, &inv]() {
				if (m_tookCastItem)
				{
					return;
				}

				auto result = inv.removeItem(itemSlot, 1);
				if (result != game::inventory_change_failure::Okay)
				{
					inv.getOwner().inventoryChangeFailure(result, item.get(), nullptr);
				}
				else
				{
					m_tookCastItem = true;
				}
			};

			for (auto &spell : item->getEntry().spells())
			{
				// OnUse spell cast
				if (spell.spell() == m_spell.id() &&
					(spell.trigger() == 0 || spell.trigger() == 5))
				{
					// Item is removed on use
					if (spell.charges() == UInt32(-1))
					{
						if (delayed)
						{
							m_completedEffectsExecution[m_cast.getExecuter().getGuid()] = completedEffects.connect(removeItem);
						}
						else
						{
							removeItem();
						}
					}
					break;
				}
			}
		}

		return true;
	}

	bool SingleCastState::consumePower()
	{
		const Int32 totalCost = m_itemGuid ? 0 : m_cast.calculatePowerCost(m_spell);
		if (totalCost > 0)
		{
			if (m_spell.powertype() == game::power_type::Health)
			{
				// Special case
				UInt32 currentHealth = m_cast.getExecuter().getUInt32Value(unit_fields::Health);
				if (totalCost > 0 &&
					currentHealth < UInt32(totalCost))
				{
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
				if (totalCost > 0 &&
					currentPower < UInt32(totalCost))
				{
					sendEndCast(false);
					m_hasFinished = true;
					return false;
				}

				currentPower -= totalCost;
				m_cast.getExecuter().setUInt32Value(unit_fields::Power1 + m_spell.powertype(), currentPower);

				if (m_spell.powertype() == game::power_type::Mana) {
					m_cast.getExecuter().notifyManaUse();
				}
			}
		}

		return true;
	}

	void SingleCastState::applyCooldown(UInt64 cooldownTimeMS, UInt64 catCooldownTimeMS)
	{
		m_cast.getExecuter().setCooldown(m_spell.id(), static_cast<UInt32>(cooldownTimeMS));
		if (m_spell.category() && catCooldownTimeMS)
		{
			auto *cat = m_cast.getExecuter().getProject().spellCategories.getById(m_spell.category());
			if (cat)
			{
				for (const auto &spellId : cat->spells())
				{
					if (spellId != m_spell.id()) {
						m_cast.getExecuter().setCooldown(spellId, static_cast<UInt32>(catCooldownTimeMS));
					}
				}
			}
		}
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
		if (powerType < 0 || powerType > 5) {
			return;
		}

		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);		//Buff, HoT

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];

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
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
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
				burn = 0 - targetUnit->addPower(game::power_type::Mana, 0 - burn);
				damage = burn * effect.multiplevalue();
				absorbed = targetUnit->consumeAbsorb(damage, school);
			}

			// Update health value
			const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
			float threat = noThreat ? 0.0f : damage - absorbed;
			if (!noThreat && m_cast.getExecuter().isGameCharacter())
			{
				reinterpret_cast<GameCharacter&>(m_cast.getExecuter()).applySpellMod(spell_mod_op::Threat, m_spell.id(), threat);
			}
			if (targetUnit->dealDamage(damage - absorbed, school, &caster, threat))
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
					//spellProcEvent(m_attackerProc);
					//targetUnit->spellProcEvent(m_victimProc);
					//caster.doneSpellMagicDmgClassNeg(targetUnit, school);
					// TODO: Really needed? Because this method is already fired in the dealDamage method
					// targetUnit->takenDamage(&caster);
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

		meleeSpecialAttack(effect, true);
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

		GameObject *raw = world->findObjectByGUID(m_target.getGOTarget());
		if (!raw)
		{
			WLOG("SPELL_EFFECT_OPEN_LOCK: Could not find target object");
			return;
		}

		if (!raw->isWorldObject())
		{
			WLOG("SPELL_EFFECT_OPEN_LOCK: Target object is not a world object");
			return;
		}

		obj = reinterpret_cast<WorldObject*>(raw);
		m_affectedTargets.insert(obj->shared_from_this());

		UInt32 currentState = obj->getUInt32Value(world_object_fields::State);

		const auto &entry = obj->getEntry();
		UInt32 lockId = getLockId(entry);
		DLOG("Lock id: " << lockId);

		// TODO: Get lock info

		// We need to consume eventual cast items NOW before we send the loot packet to the client
		// If we remove the item afterwards, the client will reject the loot dialog and request a 
		// close immediatly. Don't worry, consumeItem may be called multiple times: It will only 
		// remove the item once
		if (!consumeItem(false))
		{
			return;
		}

		// If it is a door, try to open it
		switch (entry.type())
		{
		case world_object_type::Door:
		case world_object_type::Button:
			obj->setUInt32Value(world_object_fields::State, (currentState == 1 ? 0 : 1));
			break;
		case world_object_type::Chest:
			{
				obj->setUInt32Value(world_object_fields::State, (currentState == 1 ? 0 : 1));

				// Open chest loot window
				auto *loot = obj->getObjectLoot();
				if (loot &&
				        !loot->isEmpty())
				{
					// Start inspecting the loot
					if (m_cast.getExecuter().isGameCharacter())
					{
						GameCharacter *character = reinterpret_cast<GameCharacter *>(&m_cast.getExecuter());
						character->lootinspect(*loot);
					}
				}
				break;
			}
		default:	// Make the compiler happy
			break;
		}

		if (m_cast.getExecuter().isGameCharacter())
		{
			GameCharacter *character = reinterpret_cast<GameCharacter *>(&m_cast.getExecuter());
			character->onQuestObjectCredit(m_spell.id(), *obj);
			character->objectInteraction(*obj);
		}

		// Raise interaction triggers
		obj->raiseTrigger(trigger_event::OnInteraction);
	}

	void SingleCastState::spellEffectApplyAreaAuraParty(const proto::SpellEffect &effect)
	{

	}

	void SingleCastState::spellEffectDispel(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		UInt8 school = m_spell.schoolmask();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			UInt32 totalPoints = 0;
			bool spellFailed = false;

			if (hitInfos[i] == game::hit_info::Miss)
			{
				spellFailed = true;
			}
			else if (victimStates[i] == game::victim_state::IsImmune)
			{
				spellFailed = true;
			}
			else if (victimStates[i] == game::victim_state::Normal)
			{
				if (resists[i] == 100.0f)
				{
					spellFailed = true;
				}
				else
				{
					totalPoints = calculateEffectBasePoints(effect);
				}
			}

			if (spellFailed)
			{
				// TODO send fail packet
				sendPacketFromCaster(caster,
				                     std::bind(game::server_write::spellNonMeleeDamageLog, std::placeholders::_1,
				                               targetUnit->getGuid(),
				                               caster.getGuid(),
				                               m_spell.id(),
				                               1,
				                               school,
				                               0,
				                               1,	//resisted
				                               false,
				                               0,
				                               false));
			}
			else if (targetUnit->isAlive())
			{
				UInt32 auraDispelType = effect.miscvaluea();
				for (UInt32 i = 0; i < totalPoints; i++)
				{
					bool positive = caster.isHostileTo(*targetUnit);
					Aura *aura = targetUnit->getAuras().popBack(auraDispelType, positive);
					if (aura)
					{
						aura->misapplyAura();
					}
					else
					{
						break;
					}
				}
			}
		}
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
			spawned->threaten(*executer.getVictim(), 0.0001f);
		}
	}

	void SingleCastState::spellEffectSummonPet(const proto::SpellEffect & effect)
	{
		GameUnit &executer = m_cast.getExecuter();

		// Check if caster already have a pet
		UInt64 petGUID = executer.getUInt64Value(unit_fields::Summon);
		if (petGUID != 0)
		{
			// Already have a pet!
			return;
		}

		// Get the pet unit entry
		const auto *entry = executer.getProject().units.getById(effect.miscvaluea());
		if (!entry)
		{
			WLOG("Can't summon pet - missing entry");
			return;
		}

		auto *world = executer.getWorldInstance();
		if (!world)
		{
			return;
		}

		float o = executer.getOrientation();
		math::Vector3 location(executer.getLocation());

		auto spawned = world->spawnSummonedCreature(*entry, location, o);
		if (!spawned)
		{
			ELOG("Could not spawn creature!");
			return;
		}

		spawned->setUInt64Value(unit_fields::SummonedBy, executer.getGuid());
		spawned->setFactionTemplate(executer.getFactionTemplate());
		spawned->setLevel(executer.getLevel());		// TODO
		executer.setUInt64Value(unit_fields::Summon, spawned->getGuid());
		spawned->setUInt32Value(unit_fields::CreatedBySpell, m_spell.id());
		spawned->setUInt64Value(unit_fields::CreatedBy, executer.getGuid());
		spawned->setUInt32Value(unit_fields::NpcFlags, 0);
		spawned->setUInt32Value(unit_fields::Bytes1, 0);
		spawned->setUInt32Value(unit_fields::PetNumber, guidLowerPart(spawned->getGuid()));
		world->addGameObject(*spawned);
	}

	void SingleCastState::spellEffectCharge(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		if (!targets.empty())
		{
			GameUnit &firstTarget = *targets[0];

			// TODO: Error checks and limit max path length
			auto &mover = caster.getMover();
			mover.moveTo(firstTarget.getLocation(), 25.0f);
		}
	}

	void SingleCastState::spellEffectAttackMe(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			GameUnit *topThreatener = targetUnit->getTopThreatener().get();
			if (topThreatener)
			{
				float addThread = targetUnit->getThreat(*topThreatener).get();
				addThread -= targetUnit->getThreat(caster).get();
				if (addThread > 0.0f) {
					targetUnit->threaten(caster, addThread);
				}
			}
		}
	}

	void SingleCastState::spellEffectScript(const proto::SpellEffect &effect)
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

	void SingleCastState::meleeSpecialAttack(const proto::SpellEffect &effect, bool basepointsArePct)
	{
		GameUnit &attacker = m_cast.getExecuter();
		UInt8 school = m_spell.schoolmask();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpecialMeleeAttack(&attacker, m_spell, m_target, school, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			UInt32 totalDamage;
			const game::VictimState &state = victimStates[i];
			if (state == game::victim_state::IsImmune)
			{
				totalDamage = 0;
			}
			else if (hitInfos[i] == game::hit_info::Miss)
			{
				totalDamage = 0;
			}
			else if (state == game::victim_state::Dodge)
			{
				totalDamage = 0;
			}
			else if (state == game::victim_state::Parry)
			{
				totalDamage = 0;
				//TODO accelerate next m_victim autohit
			}
			else
			{
				if (basepointsArePct)
				{
					totalDamage = 0;
				}
				else
				{
					totalDamage = calculateEffectBasePoints(effect);
				}

				// Add weapon damage
				const bool isRanged = (m_spell.attributes(0) & game::spell_attributes::Ranged) != 0;
				const float weaponMin = 
					isRanged  ? attacker.getFloatValue(unit_fields::MinRangedDamage) : attacker.getFloatValue(unit_fields::MinDamage);
				const float weaponMax = 
					isRanged ? attacker.getFloatValue(unit_fields::MaxRangedDamage) : attacker.getFloatValue(unit_fields::MaxDamage);

				// Randomize weapon damage
				std::uniform_real_distribution<float> distribution(weaponMin, weaponMax);
				totalDamage += UInt32(distribution(randomGenerator));

				// Armor reduction
				totalDamage = targetUnit->calculateArmorReducedDamage(attacker.getLevel(), totalDamage);
				if (totalDamage < 0) {	//avoid negative damage when blockValue is high
					totalDamage = 0;
				}

				if (basepointsArePct)
				{
					totalDamage *= (calculateEffectBasePoints(effect) / 100.0);
				}
			}
			if (i < m_meleeDamage.size())
			{
				m_meleeDamage[i] = m_meleeDamage[i] + totalDamage;
			}
			else
			{
				m_meleeDamage.push_back(totalDamage);
			}
			if (!m_connectedMeleeSignal) 
			{
				m_connectedMeleeSignal = true;
				m_completedEffectsExecution[targetUnit->getGuid()] = completedEffects.connect(std::bind(&SingleCastState::executeMeleeAttack, this));
			}
		}
	}
}
