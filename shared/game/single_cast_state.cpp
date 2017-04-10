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
#include "game_dyn_object.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	SingleCastState::SingleCastState(SpellCast &cast, const proto::SpellEntry &spell, SpellTargetMap target, const game::SpellPointsArray &basePoints, GameTime castTime, bool isProc/* = false*/, UInt64 itemGuid/* = 0*/)
		: m_cast(cast)
		, m_spell(spell)
		, m_target(std::move(target))
		, m_hasFinished(false)
		, m_countdown(cast.getTimers())
		, m_impactCountdown(cast.getTimers())
		, m_castTime(castTime)
		, m_castEnd(0)
		, m_basePoints(std::move(basePoints))
		, m_isProc(isProc)
		, m_attackTable()
		, m_itemGuid(itemGuid)
		, m_projectileStart(0)
		, m_projectileEnd(0)
		, m_connectedMeleeSignal(false)
		, m_delayCounter(0)
		, m_tookCastItem(false)
		, m_tookReagents(false)
		, m_attackerProc(0)
		, m_victimProc(0)
		, m_canTrigger(false)
		, m_attackType(game::weapon_attack::BaseAttack)
		, m_instantsCast(false)
		, m_delayedCast(false)
	{
		// Check if the executer is in the world
		auto &executer = m_cast.getExecuter();
		auto *worldInstance = executer.getWorldInstance();

		if (!m_itemGuid)
		{
			m_castTime *= executer.getFloatValue(unit_fields::ModCastSpeed);
		}

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
			              static_cast<Int32>(m_castTime),			// Cast time in ms
			              0)										// Cast count (unknown)
			);
		}

		if (worldInstance && isChanneled())
		{
			sendPacketFromCaster(
				executer,
				std::bind(game::server_write::channelStart,
						  std::placeholders::_1,
						  casterId,
						  m_spell.id(),
						  m_spell.duration())
			);
			
			executer.setUInt64Value(unit_fields::ChannelObject, m_target.getUnitTarget());
			executer.setUInt32Value(unit_fields::ChannelSpell, m_spell.id());
		}

		const math::Vector3 &location = m_cast.getExecuter().getLocation();
		m_x = location.x, m_y = location.y, m_z = location.z;

		m_countdown.ended.connect([this]()
		{
			if (isChanneled())
			{
				this->finishChanneling();
			}
			else
			{
				this->onCastFinished();
			}
		});
	}

	void SingleCastState::activate()
	{
		if (m_castTime > 0)
		{
			m_castEnd = getCurrentTime() + m_castTime;
			m_countdown.setEnd(m_castEnd);
			m_damaged = m_cast.getExecuter().takenDamage.connect([this](GameUnit *attacker, UInt32 damage, game::DamageType type) {
				if (!m_hasFinished && m_countdown.running && type == game::DamageType::Direct)
				{
					if (m_spell.interruptflags() & game::spell_interrupt_flags::PushBack &&
						m_delayCounter < 2 &&
						m_cast.getExecuter().isGameCharacter() &&
						damage > 0)	// Pushback only works on characters
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
					if (m_spell.interruptflags() & game::spell_interrupt_flags::Damage &&
						m_cast.getExecuter().isGameCharacter())
					{
						// This interrupt flag seems to only be used on players as there is at least one spell (5514), which
						// definetly has this flag set but is NOT interrupt on any damage, and never was (NPC Dark Sprite)
						stopCast(game::spell_interrupt_flags::Damage);
					}
				}
			});

			WorldInstance *world = m_cast.getExecuter().getWorldInstance();
			ASSERT(world);

			GameUnit *unitTarget = nullptr;
			m_target.resolvePointers(*world, &unitTarget, nullptr, nullptr, nullptr);
			if (unitTarget)
			{
				m_onTargetDied = unitTarget->killed.connect(this, &SingleCastState::onTargetKilled);
				m_onTargetRemoved = unitTarget->despawned.connect(this, &SingleCastState::onTargetDespawned);
			}

			if (m_spell.attributes(0) & game::spell_attributes::NotInCombat)
			{
				m_onThreatened = m_cast.getExecuter().threatened.connect(std::bind(&SingleCastState::stopCast, this, game::spell_interrupt_flags::None, 0));
			}
		}
		else
		{
			if (isChanneled())
			{
				m_castEnd = getCurrentTime() + m_spell.duration();
				m_countdown.setEnd(m_castEnd);
				m_damaged = m_cast.getExecuter().takenDamage.connect([this](GameUnit *attacker, UInt32 damage, game::DamageType type) {
					if (m_countdown.running)
					{
						if (m_spell.channelinterruptflags() & game::spell_channel_interrupt_flags::Delay &&
							m_delayCounter < 2 &&
							m_cast.getExecuter().isGameCharacter() &&
							damage > 0)	// Pushback only works on characters
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

							const float delay = m_spell.duration() * 0.25f;
							m_castEnd -= m_castEnd - getCurrentTime() >= delay ? delay : m_castEnd - getCurrentTime();
							m_countdown.setEnd(m_castEnd);

							// Notify about spell delay
							sendPacketFromCaster(m_cast.getExecuter(),
								std::bind(game::server_write::channelUpdate, std::placeholders::_1,
									m_cast.getExecuter().getGuid(),
									m_castEnd - getCurrentTime()));
							m_delayCounter++;
						}
						if (m_spell.interruptflags() & game::spell_interrupt_flags::Damage)
						{
							stopCast(game::spell_interrupt_flags::Damage);
						}
					}
				});

				WorldInstance *world = m_cast.getExecuter().getWorldInstance();
				ASSERT(world);

				GameUnit *unitTarget = nullptr;
				m_target.resolvePointers(*world, &unitTarget, nullptr, nullptr, nullptr);
				if (unitTarget)
				{
					m_onTargetDied = unitTarget->killed.connect(this, &SingleCastState::onTargetKilled);
					m_onTargetRemoved = unitTarget->despawned.connect(this, &SingleCastState::onTargetDespawned);
				}
			}

			onCastFinished();
		}
	}

	std::pair<game::SpellCastResult, SpellCasting *> SingleCastState::startCast(SpellCast &cast, const proto::SpellEntry &spell, SpellTargetMap target, const game::SpellPointsArray &basePoints, GameTime castTime, bool doReplacePreviousCast, UInt64 itemGuid)
	{
		if (!m_hasFinished &&
		        !doReplacePreviousCast)
		{
			return std::make_pair(game::spell_cast_result::FailedSpellInProgress, &m_casting);
		}
		
		finishChanneling();
		SpellCasting &casting = castSpell(
		                            cast,
		                            spell,
		                            std::move(target),
		                            std::move(basePoints),
		                            castTime,
		                            itemGuid);

		return std::make_pair(game::spell_cast_result::CastOkay, &casting);
	}

	void SingleCastState::stopCast(game::SpellInterruptFlags reason, UInt64 interruptCooldown/* = 0*/)
	{
		finishChanneling();

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

	void SingleCastState::finishChanneling()
	{
		if (isChanneled())
		{
			// Caster could have left the world
			auto *world = m_cast.getExecuter().getWorldInstance();
			if (!world)
			{
				return;
			}

			m_cast.getExecuter().getAuras().removeAllAurasDueToSpell(m_spell.id());
			GameUnit *target = reinterpret_cast<GameUnit *>(world->findObjectByGUID(m_target.getUnitTarget()));
			if (target)
			{
				if (target->isAlive())
				{
					target->getAuras().removeAllAurasDueToSpell(m_spell.id());
				}
			}

			sendPacketFromCaster(m_cast.getExecuter(),
				std::bind(game::server_write::channelUpdate, std::placeholders::_1,
					m_cast.getExecuter().getGuid(),
					0));

			m_cast.getExecuter().setUInt64Value(unit_fields::ChannelObject, 0);
			m_cast.getExecuter().setUInt32Value(unit_fields::ChannelSpell, 0);

			m_casting.ended(true);

			// Destroy dynamic objects
			for (auto &obj : m_dynObjectsToDespawn)
			{
				m_cast.getExecuter().removeDynamicObject(obj);
			}

			m_dynObjectsToDespawn.clear();
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

		// Raise event
		getCasting().ended(success);

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

			executer.spellCastError(m_spell, game::spell_cast_result::FailedInterrupted);
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

		// Check some effect-dependant errors
		for (const auto &effect : m_spell.effects())
		{
			switch (effect.type())
			{
				case game::spell_effects::OpenLock:
				{
					// Check if object is currently in use by another player
					GameObject *obj = nullptr;
					m_target.resolvePointers(*m_cast.getExecuter().getWorldInstance(), nullptr, nullptr, &obj, nullptr);
					if (obj && obj->isWorldObject())
					{
						if (obj->getUInt32Value(world_object_fields::State) != 1)
						{
							m_cast.getExecuter().spellCastError(m_spell, game::spell_cast_result::FailedChestInUse);
							strongThis->sendEndCast(false);
							return;
						}
					}
					break;
				}
			}
		}

		m_hasFinished = true;

		const std::weak_ptr<SingleCastState> weakThis = strongThis;
		const UInt32 spellAttributes = m_spell.attributes(0);
		if (spellAttributes & game::spell_attributes::OnNextSwing)
		{
			m_onAttackError = m_cast.getExecuter().autoAttackError.connect([this](AttackSwingError error) {
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

				simple::current_connection().disconnect();
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

				if (!strongThis->consumeReagents())
				{
					m_cast.getExecuter().spellCastError(m_spell, game::spell_cast_result::FailedReagents);
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
				strongThis->applyAllEffects(true, true);

				return true;
			});
		}
		else
		{
			if (!consumePower()) {
				return;
			}

			if (!consumeReagents()) {
				return;
			}

			if (!consumeItem()) {
				return;
			}

			sendEndCast(true);

			if (m_spell.speed() > 0.0f)
			{
				// Apply all instant effects
				applyAllEffects(true, false);

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
							// Calculate spell impact delay
							auto strongTarget = std::static_pointer_cast<GameUnit>(unitTarget->shared_from_this());

							// This will be executed on the impact
							m_impactCountdown.ended.connect(
							    [this, strongThis, strongTarget]() mutable
							{
								const auto currentTime = getCurrentTime();
								const auto &targetLoc = strongTarget->getLocation();

								float percentage = static_cast<float>(currentTime - m_projectileStart) / static_cast<float>(m_projectileEnd - m_projectileStart);
								math::Vector3 projectilePos = m_projectileOrigin.lerp(m_projectileDest, percentage);
								const float dist = (targetLoc - projectilePos).length();
								const GameTime timeMS = (dist / m_spell.speed()) * 1000;

								m_projectileOrigin = projectilePos;
								m_projectileDest = targetLoc;
								m_projectileStart = currentTime;
								m_projectileEnd = currentTime + timeMS;

								if (timeMS >= 50)
								{
									m_impactCountdown.setEnd(currentTime + std::min<GameTime>(timeMS, 400));
								}
								else
								{
									strongThis->applyAllEffects(false, true);
									strongTarget.reset();
									strongThis.reset();
								}
							});

							m_projectileStart = getCurrentTime();
							m_projectileEnd = m_projectileStart + timeMS;
							m_projectileOrigin = m_cast.getExecuter().getLocation();
							m_projectileDest = unitTarget->getLocation();
							m_impactCountdown.setEnd(m_projectileStart + std::min<GameTime>(timeMS, 400));
						}
						else
						{
							applyAllEffects(false, true);
						}
					}
				}
			}
			else
			{
				applyAllEffects(true, true);
			}


			if (m_cast.getExecuter().isCreature())
			{
				reinterpret_cast<GameCreature&>(m_cast.getExecuter()).raiseTrigger(trigger_event::OnSpellCast, { m_spell.id() });
			}
		}

		const UInt32 spellAttributesA = m_spell.attributes(1);
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

		if (weakThis.lock() && !isChanneled())
		{
			//may destroy this, too
			m_casting.ended(true);
		}
	}

	void SingleCastState::onTargetKilled(GameUnit * /*killer*/)
	{
		stopCast(game::spell_interrupt_flags::None);
	}

	void SingleCastState::onTargetDespawned(GameObject & /*target*/)
	{
		stopCast(game::spell_interrupt_flags::None);
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
			m_affectedTargets.insert(targetUnit->shared_from_this());

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

			// Apply damage bonus
			m_cast.getExecuter().applyDamageDoneBonus(school, 1, totalDamage);
			targetUnit->applyDamageTakenBonus(school, 1, totalDamage);

			// Update health value
			const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
			float threat = noThreat ? 0.0f : totalDamage - resisted - absorbed;
			if (targetUnit->dealDamage(totalDamage - resisted - absorbed, school, game::DamageType::Direct, &attacker, threat))
			{
				std::map<UInt64, game::SpellMissInfo> missedTargets;
				if (state == game::victim_state::Evades)
				{
					missedTargets[targetUnit->getGuid()] = game::spell_miss_info::Evade;
				}
				else if (state == game::victim_state::IsImmune)
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
					sendPacketFromCaster(attacker,
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

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i], totalDamage, absorbed, true);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i], totalDamage, absorbed, true);
			}
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
		const Int32 basePoints = (m_basePoints[effect.index()] == 0 ? effect.basepoints() : m_basePoints[effect.index()]) + level * basePointsPerLevel;
		const Int32 randomPoints = effect.diesides() + level * randomPointsPerLevel;
		const Int32 comboDamage = effect.pointspercombopoint() * comboPoints;

		std::uniform_int_distribution<int> distribution(effect.basedice(), randomPoints);
		const Int32 randomValue = (effect.basedice() >= randomPoints ? effect.basedice() : distribution(randomGenerator));

		Int32 outBasePoints = m_basePoints[effect.index()] == 0 ? basePoints + randomValue + comboDamage : basePoints;
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
		float castTime = m_spell.casttime();
		if (castTime < 1500) {
			castTime = 1500;
		}
		float spellAddCoefficient = (castTime / 3500.0);
		float bonusModificator = 1 + (bonusPct / 100);
		return (basePoints + (spellAddCoefficient * spellPower)) *  bonusModificator;
	}

	void SingleCastState::applyAllEffects(bool executeInstants, bool executeDelayed)
	{
		// Add spell cooldown if any
		if (!m_instantsCast && !m_delayedCast)
		{
			UInt64 spellCatCD = m_spell.categorycooldown();
			UInt64 spellCD = m_spell.cooldown();

			// If cast by an item, the item cooldown is used instead of the spell cooldown
			auto item = getItem();
			if (item)
			{
				for (auto &spell : item->getEntry().spells())
				{
					if (spell.spell() == m_spell.id() &&
						(spell.trigger() == game::item_spell_trigger::OnUse || spell.trigger() == game::item_spell_trigger::OnUseNoDelay))
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

		if (!m_isProc && !m_itemGuid &&
			!(m_spell.attributes(3) & game::spell_attributes_ex_c::DisableProc) &&
			!(m_spell.attributes(0) & game::spell_attributes::Passive))
		{
			m_canTrigger = true;
		}
		else if (m_spell.attributes(2) & game::spell_attributes_ex_b::TriggeredCanProc ||
			m_spell.attributes(3) & game::spell_attributes_ex_c::TriggeredCanProc2)
		{
			m_canTrigger = true;
		}

		switch (m_spell.dmgclass())
		{
		case game::spell_dmg_class::Melee:
			m_attackerProc = game::spell_proc_flags::DoneSpellMeleeDmgClass;
			m_victimProc = game::spell_proc_flags::TakenSpellMeleeDmgClass;
			m_attackType = game::weapon_attack::BaseAttack;

			if (m_spell.attributes(3) & game::spell_attributes_ex_c::ReqOffhand)
			{
				m_attackerProc |= game::spell_proc_flags::DoneOffhandAttack;
				m_attackType = game::weapon_attack::OffhandAttack;
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

			m_attackType = game::weapon_attack::RangedAttack;
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
				m_attackType = game::weapon_attack::RangedAttack;
			}
			else
			{
				m_attackerProc = game::spell_proc_flags::DoneSpellMagicDmgClassNeg;
				m_victimProc = game::spell_proc_flags::TakenSpellMagicDmgClassNeg;
			}

			break;
		}

		// Needed for combat ratings
		m_cast.getExecuter().setWeaponAttack(m_attackType);

		// Execute spell immediatly
		namespace se = game::spell_effects;
		std::vector<std::pair<UInt32, EffectHandler>> instantMap{
			{ se::Charge,				std::bind(&SingleCastState::spellEffectCharge, this, std::placeholders::_1) },
		};

		std::vector<std::pair<UInt32, EffectHandler>> delayedMap {
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
			{se::KnockBack,				std::bind(&SingleCastState::spellEffectKnockBack, this, std::placeholders::_1) },
			// Add all effects above here
			{se::ApplyAura,				std::bind(&SingleCastState::spellEffectApplyAura, this, std::placeholders::_1)},
			{se::PersistentAreaAura,	std::bind(&SingleCastState::spellEffectPersistentAreaAura, this, std::placeholders::_1) },
			{se::ApplyAreaAuraParty,	std::bind(&SingleCastState::spellEffectApplyAura, this, std::placeholders::_1)},
			{se::SchoolDamage,			std::bind(&SingleCastState::spellEffectSchoolDamage, this, std::placeholders::_1)}
		};

		// Make sure that the executer exists after all effects have been executed
		auto strongCaster = std::static_pointer_cast<GameUnit>(m_cast.getExecuter().shared_from_this());

		if (executeInstants && !m_instantsCast)
		{
			for (auto &effect : instantMap)
			{
				for (int k = 0; k < effects.size(); ++k)
				{
					if (effect.first == effects[k])
					{
						ASSERT(effect.second);
						effect.second(m_spell.effects(k));
					}
				}
			}

			m_instantsCast = true;
		}

		if (executeDelayed && !m_delayedCast)
		{
			for (auto &effect : delayedMap)
			{
				for (int k = 0; k < effects.size(); ++k)
				{
					if (effect.first == effects[k])
					{
						ASSERT(effect.second);
						effect.second(m_spell.effects(k));
					}
				}
			}

			m_delayedCast = true;
		}

		if (!m_instantsCast || !m_delayedCast)
		{
			return;
		}

		completedEffects();

		// Consume combo points if required
		if ((m_spell.attributes(1) & (game::spell_attributes_ex_a::ReqComboPoints_1 | game::spell_attributes_ex_a::ReqComboPoints_2)))
		{
			// 0 will reset combo points
			reinterpret_cast<GameCharacter&>(*strongCaster).addComboPoints(0, 0);
		}

		if (m_canTrigger)
		{
			if (!m_hitResults.empty())
			{
				auto *world = m_cast.getExecuter().getWorldInstance();
				
				if (world)
				{
					for (auto itr = m_hitResults.begin(); itr != m_hitResults.end(); ++itr)
					{
						if (isUnitGUID(itr->first))
						{
							GameObject *targetObj = world->findObjectByGUID(itr->first);
							auto *target = reinterpret_cast<GameUnit *>(targetObj);
							bool canRemove = false;

							if (itr == m_hitResults.begin())
							{
								canRemove = true;
							}

							if (m_isProc)
							{
								canRemove = false;
							}

							strongCaster->procEvent(target, itr->second.procAttacker, itr->second.procVictim, itr->second.procEx, itr->second.amount, m_attackType, &m_spell, canRemove);
						}
					}
				}
			}
			else
			{
				strongCaster->procEvent(nullptr, m_attackerProc, m_victimProc, 0, 0, m_attackType, &m_spell, m_isProc ? false : true);
			}
		}

		// Cast all additional spells if available
		for (const auto &spell : m_spell.additionalspells())
		{
			strongCaster->castSpell(m_target, spell, { 0, 0, 0 }, 0, true);
		}

		if (strongCaster->isGameCharacter())
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

					reinterpret_cast<GameCharacter&>(*strongCaster).onQuestSpellCastCredit(m_spell.id(), *strongTarget);
				}
			}
		}
	}

	bool SingleCastState::hasChargeEffect() const
	{
		for (const auto &eff : m_spell.effects())
		{
			if (eff.type() == game::spell_effects::Charge) 
				return true;
		}

		return false;
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
					(spell.trigger() == game::item_spell_trigger::OnUse || spell.trigger() == game::item_spell_trigger::OnUseNoDelay))
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

	bool SingleCastState::consumeReagents(bool delayed/* = true*/)
	{
		if (m_tookReagents && delayed)
			return true;

		if (m_cast.getExecuter().isGameCharacter())
		{
			// First check if all items are available (TODO: Create transactions to avoid two loops)
			auto &character = reinterpret_cast<GameCharacter&>(m_cast.getExecuter());
			for (const auto &reagent : m_spell.reagents())
			{
				if (character.getInventory().getItemCount(reagent.item()) < reagent.count())
				{
					WLOG("Not enough items in inventory!");
					return false;
				}
			}

			// Now consume all reagents
			for (const auto &reagent : m_spell.reagents())
			{
				const auto *item = character.getProject().items.getById(reagent.item());
				if (!item)
					return false;

				auto result = character.getInventory().removeItems(*item, reagent.count());
				if (result != game::inventory_change_failure::Okay)
				{
					ELOG("Could not consume reagents: " << result);
					return false;
				}
			}
		}

		m_tookReagents = true;
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
			m_affectedTargets.insert(targetUnit->shared_from_this());

			UInt32 totalDamage;
			const game::VictimState &state = victimStates[i];
			if (state == game::victim_state::IsImmune ||
				state == game::victim_state::Evades ||
				state == game::victim_state::Dodge ||
				state == game::victim_state::Parry)
			{
				totalDamage = 0;
			}
			else if (hitInfos[i] == game::hit_info::Miss)
			{
				totalDamage = 0;
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

				if (attacker.isGameCharacter())
				{
					reinterpret_cast<GameCharacter&>(attacker).applySpellMod(
						spell_mod_op::Damage, m_spell.id(), totalDamage);
				}

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
				m_completedEffectsExecution[targetUnit->getGuid()] = completedEffects.connect(this, &SingleCastState::executeMeleeAttack);
			}

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i], totalDamage, 0, true);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i], totalDamage, 0, true);
			}
		}
	}

	std::shared_ptr<GameItem> wowpp::SingleCastState::getItem() const
	{
		// If cast by an item, the item cooldown is used instead of the spell cooldown
		if (m_itemGuid && m_cast.getExecuter().isGameCharacter())
		{
			auto *character = reinterpret_cast<GameCharacter *>(&m_cast.getExecuter());
			auto &inv = character->getInventory();

			UInt16 itemSlot = 0;
			if (inv.findItemByGUID(m_itemGuid, itemSlot))
			{
				return inv.getItemAtSlot(itemSlot);
			}
		}

		return std::shared_ptr<GameItem>();
	}
}
