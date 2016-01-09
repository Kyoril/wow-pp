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

namespace wowpp
{
	CreatureAICombatState::CreatureAICombatState(CreatureAI &ai, GameUnit &victim)
		: CreatureAIState(ai)
		, m_moveReached(ai.getControlled().getTimers())
		, m_moveUpdated(ai.getControlled().getTimers())
		, m_moveStart(0)
		, m_moveEnd(0)
		, m_lastThreatTime(0)
	{
		// Add initial threat
		addThreat(victim, 0.0f);
		m_moveReached.ended.connect([this]()
		{
			m_moveUpdated.cancel();

			float angle = 0.0f;
			auto *victim = getControlled().getVictim();
			if (victim)
			{
				angle = getControlled().getAngle(*victim);
			}

			float targetX = m_targetX;
			float targetY = m_targetY;
			float targetZ = m_targetZ;

			// Update creatures position
			auto strongUnit = getControlled().shared_from_this();
			std::weak_ptr<GameObject> weakUnit(strongUnit);
			getControlled().getWorldInstance()->getUniverse().post([weakUnit, targetX, targetY, targetZ, angle]()
			{
				auto strongUnit = weakUnit.lock();
				if (strongUnit)
				{
					strongUnit->relocate(targetX, targetY, targetZ, angle);
				}
			});

			auto &homePos = getAI().getHome().position;
			if (getCurrentTime() >= (m_lastThreatTime + constants::OneSecond * 10) &&
				getControlled().getDistanceTo(homePos, false) >= 60.0f)
			{
				getAI().reset();
			}
		});

		m_moveUpdated.ended.connect([this]()
		{
			GameTime time = getCurrentTime();
			if (time >= m_moveEnd)
				return;

			// Calculate new position
			float o = getControlled().getOrientation();
			math::Vector3 oldPosition(getControlled().getLocation()), oldTarget(m_targetX, m_targetY, m_targetZ);
			if (m_moveStart != 0 && m_moveEnd > m_moveStart)
			{
				// Interpolate positions
				const float t = static_cast<float>(static_cast<double>(time - m_moveStart) / static_cast<double>(m_moveEnd - m_moveStart));
				oldPosition = oldPosition.lerp(oldTarget, t);
			}

			m_moveStart = time;

			const GameTime duration = constants::OneSecond / 4;
			if (time < m_moveEnd - duration)
			{
				m_moveUpdated.setEnd(time + duration);
			}

			// Update creatures position
			auto strongUnit = getControlled().shared_from_this();
			std::weak_ptr<GameObject> weakUnit(strongUnit);
			getControlled().getWorldInstance()->getUniverse().post([weakUnit, oldPosition, o]()
			{
				auto strongUnit = weakUnit.lock();
				if (strongUnit)
				{
					strongUnit->relocate(oldPosition[0], oldPosition[1], oldPosition[2], o);
				}
			});

			auto &homePos = getAI().getHome().position;
			if (getCurrentTime() >= (m_lastThreatTime + constants::OneSecond * 10) &&
				getControlled().getDistanceTo(homePos, false) >= 60.0f)
			{
				getAI().reset();
			}
		});
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

		// Calculate new position
		float o = getControlled().getOrientation();
		math::Vector3 oldPosition(getControlled().getLocation()), oldTarget(m_targetX, m_targetY, m_targetZ);
		if (m_moveStart != 0 && m_moveEnd > m_moveStart)
		{
			// Interpolate positions
			const float t = static_cast<float>(static_cast<double>(getCurrentTime() - m_moveStart) / static_cast<double>(m_moveEnd - m_moveStart));
			oldPosition = oldPosition.lerp(oldTarget, t);
		}

		// Update creatures position
		auto strongUnit = getControlled().shared_from_this();
		std::weak_ptr<GameObject> weakUnit(strongUnit);
		getControlled().getWorldInstance()->getUniverse().post([weakUnit, oldPosition, o]()
		{
			auto strongUnit = weakUnit.lock();
			if (strongUnit)
			{
				strongUnit->relocate(oldPosition.x, oldPosition.y, oldPosition.z, o);
			}
		});

		// Send move packet
		TileIndex2D tile;
		if (getControlled().getTileIndex(tile))
		{
			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);
			game::server_write::monsterMove(packet, getControlled().getGuid(), oldPosition, oldPosition, 0);

			forEachSubscriberInSight(
				getControlled().getWorldInstance()->getGrid(),
				tile,
				[&packet, &buffer](ITileSubscriber &subscriber)
			{
				subscriber.sendPacket(packet, buffer);
			});
		}

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

		// Now, determine the victim with the highest threat value
		float highestThreat = -1.0f;
		GameUnit *newVictim = nullptr;
		for (auto &entry : m_threat)
		{
			if (entry.second.amount > highestThreat)
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
			m_onVictimMoved = newVictim->moved.connect([this](GameObject &moved, float oldX, float oldY, float oldZ, float oldO)
			{
				chaseTarget(static_cast<GameUnit&>(moved));
			});
		}
		else if (!newVictim)
		{
			// No victim found (threat list probably empty?). Warning: this will destroy
			// the current state.
			getAI().reset();
		}
	}

	void CreatureAICombatState::chaseTarget(GameUnit &target)
	{
		float o = getControlled().getOrientation(), o2 = target.getOrientation();
		math::Vector3 oldPosition(getControlled().getLocation()), oldTarget(m_targetX, m_targetY, m_targetZ), newTarget(target.getLocation());
		if (m_moveStart != 0 && m_moveEnd > m_moveStart)
		{
			// Interpolate positions
			const float t = static_cast<float>(static_cast<double>(getCurrentTime() - m_moveStart) / static_cast<double>(m_moveEnd - m_moveStart));
			oldPosition = oldPosition.lerp(oldTarget, t);
		}

		o = getControlled().getAngle(newTarget.x, newTarget.z);
		const float distance =
			sqrtf(
				((oldPosition.x - newTarget.x) * (oldPosition.x - newTarget.x)) + 
				((oldPosition.y - newTarget.y) * (oldPosition.y - newTarget.y)) + 
				((oldPosition.z - newTarget.z) * (oldPosition.z - newTarget.z)));

		// Check distance and whether we need to move
		// TODO: If this creature is a ranged one or casts spells, it need special treatment
		const float combatRange = getControlled().getMeleeReach() + target.getMeleeReach();
		if (distance > combatRange)
		{
			GameTime moveTime = (distance / 7.5f) * constants::OneSecond;
			m_moveStart = getCurrentTime();
			m_moveEnd = m_moveStart + moveTime;

			// Delay relocation
			auto strongUnit = getControlled().shared_from_this();
			std::weak_ptr<GameObject> weakUnit(strongUnit);

			getControlled().getWorldInstance()->getUniverse().post([weakUnit, oldPosition, o]()
			{
				auto strongUnit = weakUnit.lock();
				if (strongUnit)
				{
					strongUnit->relocate(oldPosition.x, oldPosition.y, oldPosition.z, o);
				}
			});

			// Move (TODO: Better way to do this)
			m_targetX = newTarget.x; m_targetY = newTarget.y; m_targetZ = newTarget.z;

			// Send move packet
			TileIndex2D tile;
			if (getControlled().getTileIndex(tile))
			{
				std::vector<char> buffer;
				io::VectorSink sink(buffer);
				game::Protocol::OutgoingPacket packet(sink);
				game::server_write::monsterMove(packet, getControlled().getGuid(), oldPosition, newTarget, moveTime);

				forEachSubscriberInSight(
					getControlled().getWorldInstance()->getGrid(),
					tile,
					[&packet, &buffer](ITileSubscriber &subscriber)
				{
					subscriber.sendPacket(packet, buffer);
				});
			}

			m_moveReached.setEnd(m_moveEnd);
			m_moveUpdated.setEnd(m_moveStart + constants::OneSecond / 4);
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
