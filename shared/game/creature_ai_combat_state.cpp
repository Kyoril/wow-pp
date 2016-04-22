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
#include "creature_ai_combat_state.h"
#include "creature_ai.h"
#include "defines.h"
#include "universe.h"
#include "game_creature.h"
#include "game_character.h"
#include "world_instance.h"
#include "proto_data/trigger_helper.h"
#include "binary_io/vector_sink.h"
#include "game_protocol/game_protocol.h"
#include "each_tile_in_sight.h"
#include "common/constants.h"
#include "log/default_log_levels.h"
#include "unit_mover.h"

namespace wowpp
{
	CreatureAICombatState::CreatureAICombatState(CreatureAI &ai, GameUnit &victim)
		: CreatureAIState(ai)
		, m_lastThreatTime(0)
		, m_nextActionCountdown(ai.getControlled().getTimers())
		, m_isCasting(false)
		, m_entered(false)
		, m_isRanged(false)
	{
		// Setup
		m_lastSpellEntry = nullptr;
		m_lastSpell = nullptr;
		m_customCooldown = 0;
		m_lastCastTime = 0;
		m_lastCastResult = game::spell_cast_result::CastOkay;

		// Add initial threat
		addThreat(victim, 0.0f);
		m_nextActionCountdown.ended.connect(
		    std::bind(&CreatureAICombatState::chooseNextAction, this));
	}

	CreatureAICombatState::~CreatureAICombatState()
	{
		// Disconnect all connected slots
		m_nextActionCountdown.ended.disconnect_all_slots();
	}

	void CreatureAICombatState::onEnter()
	{
		auto &controlled = getControlled();

		// Flag controlled unit for combat
		controlled.addFlag(unit_fields::UnitFlags, game::unit_flags::InCombat);

		// Watch for threat events
		m_onThreatened = controlled.threatened.connect([this](GameUnit & threatener, float amount)
		{
			addThreat(threatener, amount);
		});
		m_getThreat = controlled.getThreat.connect([this](GameUnit & threatener)
		{
			return getThreat(threatener);
		});
		m_setThreat = controlled.setThreat.connect([this](GameUnit & threatener, float amount)
		{
			setThreat(threatener, amount);
		});
		m_getTopThreatener = controlled.getTopThreatener.connect([this]()
		{
			return getTopThreatener();
		});
		m_onControlledMoved = controlled.moved.connect([this](GameObject &, const math::Vector3 & position, float rotation)
		{
			if (!getControlled().isCombatMovementEnabled())
			{
				return;
			}

			if (m_lastSpellEntry != nullptr && m_lastSpell != nullptr)
			{
				// Check if we are able to cast that spell now, and if so: Do it!
				GameUnit *victim = getControlled().getVictim();
				if (!victim) {
					return;
				}

				// Check power requirement

				// Check distance and line of sight
				if (m_lastSpell->minrange() != 0.0f || m_lastSpell->maxrange() != 0.0f)
				{
					const float combatReach = victim->getFloatValue(unit_fields::CombatReach) + getControlled().getFloatValue(unit_fields::CombatReach);
					const float distance = getControlled().getDistanceTo(*victim);
					if ((m_lastSpell->minrange() > 0.0f && distance < m_lastSpell->minrange()) ||
					        (m_lastSpell->maxrange() > 0.0f && distance > m_lastSpell->maxrange() + combatReach))
					{
						// Still out of range, do nothing
						return;
					}
				}

				// Check for line of sight
				if (!getControlled().isInLineOfSight(*victim))
				{
					// Still not in line of sight
					return;
				}

				// Reset variables and choose next action (should automatically be old action in most cases)
				m_customCooldown = 0;
				m_lastSpellEntry = nullptr;
				m_lastSpell = nullptr;
				m_lastCastTime = 0;
				chooseNextAction();
			}
			else
			{
				GameUnit *victim = getControlled().getVictim();
				if (victim)
				{
					const float distance =
						(victim->getLocation() - getControlled().getLocation()).squared_length();
					if (distance <= (getControlled().getMeleeReach() * getControlled().getMeleeReach()))
					{
						m_onVictimMoved.disconnect();
						getControlled().getMover().stopMovement();

						m_nextActionCountdown.setEnd(getCurrentTime() + 500);
					}
				}
			}
		});
		
		// Reset AI eventually
		m_onMoveTargetChanged = getControlled().getMover().targetChanged.connect([this]
		{
			auto &homePos = getAI().getHome().position;
			if (getCurrentTime() >= (m_lastThreatTime + constants::OneSecond * 10) &&
			getControlled().getDistanceTo(homePos, false) >= 60.0f)
			{
				getAI().reset();
			}
		});
		m_onStunChanged = getControlled().stunStateChanged.connect([this](bool stunned)
		{
			// If we are no longer stunned, update victim again
			if (!stunned)
			{
				if (!getControlled().isCombatMovementEnabled())
				{
					// Try to continue last movement if we aren't there already
					auto &mover = getControlled().getMover();
					mover.moveTo(mover.getTarget());

					return;
				}

				chooseNextAction();
			}
			else
			{
				// Do not watch for unit motion
				m_onVictimMoved.disconnect();

				// No longer attack unit if stunned
				m_isCasting = false;
				getControlled().cancelCast(game::spell_interrupt_flags::None);
				getControlled().stopAttack();
				getControlled().setVictim(nullptr);

				m_customCooldown = 0;
				m_lastSpellEntry = nullptr;
				m_lastSpell = nullptr;
				m_lastCastTime = 0;
			}
		});
		m_onRootChanged = getControlled().rootStateChanged.connect([this](bool rooted)
		{
			if (!rooted)
			{
				if (!getControlled().isCombatMovementEnabled())
				{
					// Try to continue last movement if we aren't there already
					auto &mover = getControlled().getMover();
					mover.moveTo(mover.getTarget());

					return;
				}
			}

			chooseNextAction();
		});

		// Process aggro event
		TileIndex2D tile;
		if (controlled.getTileIndex(tile))
		{
			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);
			game::server_write::aiReaction(packet, controlled.getGuid(), 2);

			forEachSubscriberInSight(
			    controlled.getWorldInstance()->getGrid(),
			    tile,
			    [&packet, &buffer](ITileSubscriber & subscriber)
			{
				subscriber.sendPacket(packet, buffer);
			});
		}

		// No more loot recipients
		controlled.removeLootRecipients();

		// Raise OnAggro triggers
		controlled.raiseTrigger(trigger_event::OnAggro);

		// Apply initial spell cooldowns
		for (const auto &entry : controlled.getEntry().creaturespells())
		{
			const auto *spell = controlled.getProject().spells.getById(entry.spellid());
			if (!spell)
			{
				continue;
			}

			// Creature is a ranged one
			if (spell->attributes(0) & game::spell_attributes::Ranged)
			{
				m_isRanged = true;
			}

			if (entry.mininitialcooldown() > 0)
			{
				UInt32 initialCooldown = 0;
				if (entry.maxinitialcooldown() > entry.mininitialcooldown())
				{
					auto initialCdDist =
					    std::uniform_int_distribution<UInt32>(entry.mininitialcooldown(), entry.maxinitialcooldown());
					initialCooldown = initialCdDist(randomGenerator);
				}
				else
				{
					initialCooldown = entry.mininitialcooldown();
				}

				// Apply cooldown
				controlled.setCooldown(entry.spellid(), initialCooldown);
			}
		}

		if (!controlled.getEntry().creaturespells().empty())
		{
			m_onAutoAttackDone = controlled.doneMeleeAttack.connect([this](GameUnit * victim, game::VictimState state)
			{
				m_nextActionCountdown.setEnd(getCurrentTime() + 1);
			});
		}

		// Entered the combat state
		m_entered = true;

		// Choose next action
		chooseNextAction();
	}

	void CreatureAICombatState::onLeave()
	{
		// Reset all events here to prevent them being fired in another ai state
		m_nextActionCountdown.cancel();
		m_onThreatened.disconnect();
		m_getThreat.disconnect();
		m_setThreat.disconnect();
		m_getTopThreatener.disconnect();
		m_onControlledMoved.disconnect();
		m_onMoveTargetChanged.disconnect();
		m_onStunChanged.disconnect();
		m_onRootChanged.disconnect();
		m_onVictimMoved.disconnect();

		auto &controlled = getControlled();

		// Stop movement!
		controlled.getMover().stopMovement();

		// All remaining threateners are no longer in combat with this unit
		for (auto &pair : m_threat)
		{
			pair.second.threatener->removeAttackingUnit(getControlled());
		}

		// Unit is no longer flagged for combat
		controlled.removeFlag(unit_fields::UnitFlags, game::unit_flags::InCombat);

		// Stop auto attack
		controlled.cancelCast(game::spell_interrupt_flags::None);
		controlled.stopAttack();
		controlled.setVictim(nullptr);
	}

	void CreatureAICombatState::addThreat(GameUnit &threatener, float amount)
	{
		// No negative threat
		if (amount < 0.0f) {
			amount = 0.0f;
		}

		// No aggro on dead units
		if (!threatener.isAlive()) {
			return;
		}

		// No aggro on friendly units
		const auto &faction = getControlled().getFactionTemplate();
		if (threatener.isFriendlyTo(faction)) {
			return;
		}

		// Add threat amount (Note: A value of 0 is fine here, as it will still add an
		// entry to the threat list)
		UInt64 guid = threatener.getGuid();
		auto it = m_threat.find(guid);
		if (it == m_threat.end())
		{
			// Insert new entry
			it = m_threat.insert(m_threat.begin(), std::make_pair(guid, ThreatEntry(&threatener, 0.0f)));

			// Watch for unit killed signal
			m_killedSignals[guid] = threatener.killed.connect([this, guid, &threatener](GameUnit * killer)
			{
				removeThreat(threatener);
			});

			// Watch for unit despawned signal
			m_despawnedSignals[guid] = threatener.despawned.connect([this, &threatener](GameObject & despawned)
			{
				removeThreat(threatener);
			});

			// Add this unit to the list of attacking units
			threatener.addAttackingUnit(getControlled());
		}

		auto &threatEntry = it->second;
		threatEntry.amount += amount;

		m_lastThreatTime = getCurrentTime();

		// If not casting right now and already initialized, choose next action
		if (!m_isCasting && m_entered)
			chooseNextAction();
	}

	void CreatureAICombatState::removeThreat(GameUnit &threatener)
	{
		UInt64 guid = threatener.getGuid();
		auto it = m_threat.find(guid);
		if (it != m_threat.end())
		{
			m_threat.erase(it);
		}

		auto killedIt = m_killedSignals.find(guid);
		if (killedIt != m_killedSignals.end())
		{
			m_killedSignals.erase(killedIt);
		}

		auto despawnedIt = m_despawnedSignals.find(guid);
		if (despawnedIt != m_despawnedSignals.end())
		{
			m_despawnedSignals.erase(despawnedIt);
		}

		auto &controlled = getControlled();
		threatener.removeAttackingUnit(controlled);

		if (controlled.getVictim() == &threatener ||
		        m_threat.empty())
		{
			controlled.stopAttack();
			controlled.cancelCast(game::spell_interrupt_flags::None);

			chooseNextAction();
		}
	}

	float CreatureAICombatState::getThreat(GameUnit &threatener)
	{
		UInt64 guid = threatener.getGuid();
		auto it = m_threat.find(guid);
		if (it == m_threat.end())
		{
			return 0.0f;
		}
		else
		{
			return it->second.amount;
		}
	}

	void CreatureAICombatState::setThreat(GameUnit &threatener, float amount)
	{
		UInt64 guid = threatener.getGuid();
		auto it = m_threat.find(guid);
		if (it != m_threat.end())
		{
			it->second.amount = amount;
		}
	}

	GameUnit *CreatureAICombatState::getTopThreatener()
	{
		float highestThreat = -1.0f;
		GameUnit *topThreatener = nullptr;
		for (auto &entry : m_threat)
		{
			if (entry.second.amount > highestThreat)
			{
				topThreatener = entry.second.threatener;
				highestThreat = entry.second.amount;
			}
		}
		return topThreatener;
	}

	void CreatureAICombatState::updateVictim()
	{
		GameCreature &controlled = getControlled();
		GameUnit *victim = controlled.getVictim();
		bool rooted = controlled.isRooted();

		// Now, determine the victim with the highest threat value
		float highestThreat = -1.0f;
		GameUnit *newVictim = nullptr;
		for (auto &entry : m_threat)
		{
			bool isInRange = true;

			// If we are rooted, we need to check for nearby targets to start attacking them
			if (rooted)
			{
				isInRange =
				    (controlled.getDistanceTo(*entry.second.threatener, true) <= entry.second.threatener->getMeleeReach() + controlled.getMeleeReach());
			}

			if (entry.second.amount > highestThreat && isInRange)
			{
				newVictim = entry.second.threatener;
				highestThreat = entry.second.amount;
			}
		}

		if (newVictim &&
		        newVictim != victim)
		{
			controlled.setVictim(newVictim);
		}
		else if (!newVictim)
		{
			controlled.setVictim(nullptr);
		}
	}

	void CreatureAICombatState::chaseTarget(GameUnit &target)
	{
		// Skip movement in case of trigger
		if (!getControlled().isCombatMovementEnabled())
		{
			return;
		}

		const float combatRange = getControlled().getMeleeReach() + target.getMeleeReach();

		math::Vector3 currentLocation;
		auto &mover = getControlled().getMover();

		// If we are moving, check if the current TARGET LOCATION is not in range instead of checking
		// if the current location is not in attack range. Only THEN we need to calculate a new movement
		// path.
		currentLocation = (mover.isMoving() ? mover.getTarget() : mover.getCurrentLocation());

		math::Vector3 newTargetLocation = target.getLocation();
		const float distance =
		    (newTargetLocation - currentLocation).squared_length();

		// Check distance and whether we need to move
		if (distance > (combatRange * combatRange))
		{
			math::Vector3 direction = (newTargetLocation - currentLocation);
			if (direction.normalize() != 0.0f)
			{
				// Adjust target location since we don't want to stand IN the target
				newTargetLocation = newTargetLocation - (direction * target.getMeleeReach());
			}

			// Chase the target
			getControlled().getMover().moveTo(newTargetLocation);
		}
	}

	void CreatureAICombatState::chooseNextAction()
	{
		GameCreature &controlled = getControlled();
		if (!controlled.isCombatMovementEnabled())
		{
			return;
		}

		m_onVictimMoved.disconnect();

		// First, determine our current victim
		updateVictim();

		// We should have a valid victim here, otherwise there is nothing to do but to reset
		GameUnit *victim = controlled.getVictim();
		if (!victim)
		{
			// No victim found (threat list probably empty or rooted)
			if (!controlled.isRooted())
			{
				// Warning: this will destroy the current AI state.
				getAI().reset();
			}
			return;
		}

		// So now we choose from the creature spells, ordered by priority
		auto &entry = controlled.getEntry();

		// Did we try to cast a spell last time?
		if (m_lastSpellEntry == nullptr && 
			entry.creaturespells_size() > 0)
		{
			UInt32 lowestCooldown = 0;
			float distance = controlled.getDistanceTo(*victim);

			UInt32 highestPriority = 0;
			const proto::UnitSpellEntry *validSpellEntry = nullptr;
			for (const auto &spelldef : entry.creaturespells())
			{
				UInt32 cd = controlled.getCooldown(spelldef.spellid());
				if (lowestCooldown == 0 && cd > 0)
				{
					lowestCooldown = cd;
				}
				else if (lowestCooldown > 0 && cd > 0 && cd < lowestCooldown)
				{
					lowestCooldown = cd;
				}

				// Skip this spell as we already found a spell with higher priority value
				if (spelldef.priority() < highestPriority) {
					continue;
				}

				if (spelldef.minrange() > 0.0f &&
					distance < spelldef.minrange())
				{
					// We are too close
					continue;
				}
				else if (spelldef.maxrange() > 0.0f &&
					distance > spelldef.maxrange())
				{
					// Too far away
					continue;
				}
				
				if (spelldef.priority() > highestPriority)
				{
					// Check for cooldown and only if valid, use it
					if (cd == 0)
					{
						highestPriority = spelldef.priority();
						validSpellEntry = &spelldef;
					}
				}
				else	// Equal priority
				{
					// We already have a valid spell at that priority level
					if (validSpellEntry) {
						continue;
					}

					// Check if that spell is on cooldown
					if (cd == 0)
					{
						validSpellEntry = &spelldef;
					}
				}
			}

			// We parsed all spells, now check if there is a spell that isn't on cooldown
			if (validSpellEntry)
			{
				const auto *spellEntry = controlled.getProject().spells.getById(validSpellEntry->spellid());
				if (spellEntry)
				{
					// If spell can not be casted while moving, stop movement if any
					if (spellEntry->interruptflags() & game::spell_interrupt_flags::Movement)
					{
						controlled.getMover().stopMovement();
					}

					// Look at the target
					controlled.setOrientation(controlled.getAngle(*victim));

					// Save some variables which are needed in CreatureAICombatState::onSpellCast method
					m_lastCastTime = spellEntry->casttime();
					m_lastSpellEntry = validSpellEntry;
					m_lastSpell = spellEntry;
					m_customCooldown = 0;
					if (validSpellEntry->mincooldown() > 0)
					{
						if (validSpellEntry->maxcooldown() > validSpellEntry->mincooldown())
						{
							std::uniform_int_distribution<UInt32> cooldownDist(validSpellEntry->mincooldown(), validSpellEntry->maxcooldown());
							m_customCooldown = cooldownDist(randomGenerator);
						}
						else
						{
							m_customCooldown = validSpellEntry->mincooldown();
						}
					}
					else if(validSpellEntry->mincooldown() < 0)
					{
						if (spellEntry->attributes(0) & game::spell_attributes::Ranged)
						{
							m_customCooldown = controlled.getUInt32Value(unit_fields::RangedAttackTime);
						}
						else
						{
							m_customCooldown = spellEntry->categorycooldown();
							if (m_customCooldown == 0)
							{
								m_customCooldown = spellEntry->cooldown();
							}
						}
					}

					// Cast that spell
					SpellTargetMap targetMap;
					targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
					targetMap.m_unitTarget = victim->getGuid();
					if (validSpellEntry->target() == 0)
					{
						targetMap.m_unitTarget = controlled.getGuid();
					}

					controlled.castSpell(std::move(targetMap), validSpellEntry->spellid(), -1, m_lastCastTime, false, 0, 
						std::bind(&CreatureAICombatState::onSpellCast, this, std::placeholders::_1));
					m_isCasting = true;
					return;
				}
			}
			else if (lowestCooldown > 0)
			{
				m_nextActionCountdown.setEnd(getCurrentTime() + lowestCooldown + 100);
			}
		}

		// Watch for victim move signal
		if (!m_isRanged)
		{
			m_onVictimMoved = victim->moved.connect([this](GameObject & moved, math::Vector3 oldPosition, float oldO)
			{
				chaseTarget(static_cast<GameUnit &>(moved));
			});
		}
		
		// No spell cast - start auto attack
		if (m_lastCastResult != game::spell_cast_result::FailedLineOfSight &&
		        m_lastCastResult != game::spell_cast_result::FailedOutOfRange)
		{
			// In all these cases, simply start auto attacking our target
			controlled.startAttack();
		}
		else if(m_isRanged)
		{
			chaseTarget(*victim);
		}

		// Run towards our target if we aren't a ranged caster
		if (!m_isRanged)
		{
			chaseTarget(*victim);
		}
	}

	void CreatureAICombatState::onSpellCast(game::SpellCastResult result)
	{
		m_lastCastResult = result;
		m_isCasting = false;
		
		if (result == game::spell_cast_result::CastOkay)
		{
			// Apply custom cooldown if needed
			if (m_customCooldown > 0 && m_lastSpell)
			{
				getControlled().setCooldown(m_lastSpell->id(), m_customCooldown);
			}

			// Cast succeeded: If that spell has a cast time, choose next action after that amount of time
			if (m_lastCastTime > 0)
			{
				// Stop auto attack
				getControlled().stopAttack();

				// Add a little delay so that this event occurs AFTER the spell cast succeeded
				GameTime delay = getCurrentTime() + m_lastCastTime + 100;
				m_nextActionCountdown.setEnd(delay);
			}
			else
			{
				// Instant spell cast, so choose the next action
				m_nextActionCountdown.setEnd(getCurrentTime() + 100);
			}
		}
		else
		{
			// There was an error, now take action based on the error
			switch (result)
			{
			case game::spell_cast_result::FailedOutOfRange:
			case game::spell_cast_result::FailedLineOfSight:
			case game::spell_cast_result::FailedNoPower:
				m_nextActionCountdown.setEnd(getCurrentTime() + 1);
				return;
			case game::spell_cast_result::FailedCasterDead:
				// Don't do anything because we are dead by now!
				return;
			default:
				// Choose the next action
				m_nextActionCountdown.setEnd(getCurrentTime() + 1);
				break;
			}
		}

		// Reset variables
		m_customCooldown = 0;
		m_lastSpellEntry = nullptr;
		m_lastSpell = nullptr;
		m_lastCastTime = 0;
	}

	void CreatureAICombatState::onDamage(GameUnit &attacker)
	{
		auto &controlled = getControlled();

		// Only player characters will tag for loot (no, not even player pets)
		if (attacker.getTypeId() != object_type::Character) {
			return;
		}

		if (!controlled.isTagged())
		{
			// Add attacking unit to the list of loot recipients
			controlled.addLootRecipient(attacker.getGuid());

			// If the attacking player is in a group...
			GameCharacter *character = static_cast<GameCharacter *>(&attacker);
			auto groupId = character->getGroupId();
			if (groupId != 0)
			{
				math::Vector3 location(attacker.getLocation());

				// Find nearby group members and make them loot recipients, too
				controlled.getWorldInstance()->getUnitFinder().findUnits(Circle(location.x, location.y, 200.0f), [&attacker, &controlled, groupId](GameUnit & unit) -> bool
				{
					// Only characters
					if (unit.getTypeId() != object_type::Character)
					{
						return true;
					}

					// Check characters group
					GameCharacter *character = static_cast<GameCharacter *>(&unit);
					if (character->getGroupId() == groupId &&
					character->getGuid() != attacker.getGuid())
					{
						controlled.addLootRecipient(unit.getGuid());
					}

					return true;
				});
			}

			getControlled().addFlag(unit_fields::DynamicFlags, game::unit_dynamic_flags::OtherTagger);
		}
	}

	void CreatureAICombatState::onCombatMovementChanged()
	{
		// Maybe react on this state change
		if (getControlled().isCombatMovementEnabled())
		{
			chooseNextAction();
		}
		else
		{
			auto &controlled = getControlled();
			controlled.setVictim(nullptr);
		}
	}
}
