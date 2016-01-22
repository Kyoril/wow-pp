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

#include "creature_ai_combat_state.h"
#include "creature_ai.h"
#include "defines.h"
#include "universe.h"
#include "game_creature.h"
#include "game_character.h"
#include "world_instance.h"
#include "proto_data/faction_helper.h"
#include "shared/proto_data/units.pb.h"
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
	{
		// Add initial threat
		addThreat(victim, 0.0f);
	}

	CreatureAICombatState::~CreatureAICombatState()
	{
	}

	void CreatureAICombatState::onEnter()
	{
		auto &controlled = getControlled();
		auto id = controlled.getEntry().id();

		// Flag controlled unit for combat
		controlled.addFlag(unit_fields::UnitFlags, game::unit_flags::InCombat);

		// Watch for threat events
		m_onThreatened = controlled.threatened.connect([this](GameUnit &threatener, float amount)
		{
			addThreat(threatener, amount);
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
				updateVictim();
			}
			else
			{
				// Do not watch for unit motion
				m_onVictimMoved.disconnect();

				// No longer attack unit if stunned
				getControlled().cancelCast();
				getControlled().stopAttack();
				getControlled().setUInt64Value(unit_fields::Target, 0);
			}
		});
		m_onRootChanged = getControlled().rootStateChanged.connect([this](bool rooted)
		{
			updateVictim();
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
				[&packet, &buffer](ITileSubscriber &subscriber)
			{
				subscriber.sendPacket(packet, buffer);
			});
		}

		// No more loot recipients
		controlled.removeLootRecipients();

		// Raise OnAggro triggers
		controlled.raiseTrigger(trigger_event::OnAggro);
	}

	void CreatureAICombatState::onLeave()
	{
		auto &controlled = getControlled();
		auto id = controlled.getEntry().id();

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
		controlled.stopAttack();

	}

	void CreatureAICombatState::addThreat(GameUnit &threatener, float amount)
	{
		// No negative threat
		if (amount < 0.0f) amount = 0.0f;

		// No aggro on dead units
		if (!threatener.isAlive())
			return;

		// No aggro on friendly units
		const auto &factionB = threatener.getFactionTemplate();
		const auto &factionA = getControlled().getFactionTemplate();
		if (isFriendlyTo(factionA, factionB))
			return;

		// Add threat amount (Note: A value of 0 is fine here, as it will still add an
		// entry to the threat list)
		UInt64 guid = threatener.getGuid();
		auto it = m_threat.find(guid);
		if (it == m_threat.end())
		{
			// Insert new entry
			it = m_threat.insert(m_threat.begin(), std::make_pair(guid, ThreatEntry(&threatener, 0.0f)));

			// Watch for unit killed signal
			m_killedSignals[guid] = threatener.killed.connect([this, guid, &threatener](GameUnit *killer)
			{
				removeThreat(threatener);
			});
			
			// Watch for unit despawned signal
			m_despawnedSignals[guid] = threatener.despawned.connect([this, &threatener](GameObject &despawned)
			{
				removeThreat(threatener);
			});

			// Add this unit to the list of attacking units
			threatener.addAttackingUnit(getControlled());
		}

		auto &threatEntry = it->second;
		threatEntry.amount += amount;

		m_lastThreatTime = getCurrentTime();
		updateVictim();
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

		threatener.removeAttackingUnit(getControlled());
		if (getControlled().getVictim() == &threatener ||
			m_threat.empty())
		{
			updateVictim();
		}
	}

	void CreatureAICombatState::updateVictim()
	{
		// First, determine the current victim
		auto &controlled = getControlled();
		auto *victim = controlled.getVictim();

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
			// Start attacking the new target
			controlled.startAttack(*newVictim);
			chaseTarget(*newVictim); 

			// Watch for victim move signal
			m_onVictimMoved = newVictim->moved.connect([this](GameObject &moved, math::Vector3 oldPosition, float oldO)
			{
				chaseTarget(static_cast<GameUnit&>(moved));
			});
		}
		else if (!newVictim)
		{
			m_onVictimMoved.disconnect();
			controlled.stopAttack();

			if (m_threat.empty())
			{
				// No victim found (threat list probably empty?). Warning: this will destroy
				// the current state.
				getAI().reset();
			}
		}
	}

	void CreatureAICombatState::chaseTarget(GameUnit &target)
	{
		auto currentLocation = getControlled().getMover().getCurrentLocation();
		const auto &newTargetLocation = target.getLocation();

		const float distance =
			(newTargetLocation - currentLocation).length();

		// Check distance and whether we need to move
		// TODO: If this creature is a ranged one or casts spells, it need special treatment
		const float combatRange = getControlled().getMeleeReach() + target.getMeleeReach();
		if (distance > combatRange)
		{
			// Chase the target
			getControlled().getMover().moveTo(newTargetLocation);
		}
	}

	void CreatureAICombatState::onDamage(GameUnit &attacker)
	{
		auto &controlled = getControlled();
		
		// Only player characters will tag for loot (no, not even player pets)
		if (attacker.getTypeId() != object_type::Character)
			return;

		if (!controlled.isTagged())
		{
			// Add attacking unit to the list of loot recipients
			controlled.addLootRecipient(attacker.getGuid());

			// If the attacking player is in a group...
			GameCharacter *character = static_cast<GameCharacter*>(&attacker);
			auto groupId = character->getGroupId();
			if (groupId != 0)
			{
				math::Vector3 location(attacker.getLocation());

				// Find nearby group members and make them loot recipients, too
				controlled.getWorldInstance()->getUnitFinder().findUnits(Circle(location.x, location.y, 200.0f), [&attacker, &controlled, groupId](GameUnit &unit) -> bool
				{
					// Only characters
					if (unit.getTypeId() != object_type::Character)
					{
						return true;
					}

					// Check characters group
					GameCharacter *character = static_cast<GameCharacter*>(&unit);
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

}
