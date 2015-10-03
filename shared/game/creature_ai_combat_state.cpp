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
#include "game_creature.h"
#include "game_character.h"
#include "world_instance.h"
#include "data/faction_template_entry.h"
#include "binary_io/vector_sink.h"
#include "game_protocol/game_protocol.h"
#include "each_tile_in_sight.h"
#include "data/trigger_entry.h"
#include "common/constants.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	CreatureAICombatState::CreatureAICombatState(CreatureAI &ai, GameUnit &victim)
		: CreatureAIState(ai)
		, m_moveUpdate(ai.getControlled().getTimers())
	{
		// Add initial threat
		addThreat(victim, 0.0f);
		m_moveUpdate.ended.connect([this]()
		{
			float angle = 0.0f;
			auto *victim = getControlled().getVictim();
			if (victim)
			{
				angle = getControlled().getAngle(*victim);
			}

			getControlled().relocate(m_targetX, m_targetY, m_targetZ, angle);
		});
	}

	CreatureAICombatState::~CreatureAICombatState()
	{
	}

	void CreatureAICombatState::onEnter()
	{
		auto &controlled = getControlled();

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

		// Raise OnAggro triggers
		auto &entry = controlled.getEntry();
		auto it = entry.triggersByEvent.find(trigger_event::OnAggro);
		if (it != entry.triggersByEvent.end())
		{
			for (const auto *trigger : it->second)
			{
				trigger->execute(*trigger, &controlled);
			}
		}
	}

	void CreatureAICombatState::onLeave()
	{
		auto &controlled = getControlled();

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
		if (factionA.isFriendlyTo(factionB))
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
				// TODO
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
		// Check distance and whether we need to move
		// TODO: If this creature is a ranged one or casts spells, it need special treatment
		const float distance = getControlled().getDistanceTo(target);
		const float combatRange = getControlled().getMeleeReach() + target.getMeleeReach();
		if (distance > combatRange)
		{
			GameTime moveTime = (distance / 7.5f) * constants::OneSecond;

			// Move (TODO: Better way to do this)
			float tmp = 0.0f;
			target.getLocation(m_targetX, m_targetY, m_targetZ, tmp);

			// Send move packet
			TileIndex2D tile;
			if (getControlled().getTileIndex(tile))
			{
				float o;
				Vector<float, 3> oldPosition;
				getControlled().getLocation(oldPosition[0], oldPosition[1], oldPosition[2], o);
				Vector<float, 3> newPosition(m_targetX, m_targetY, m_targetZ);

				std::vector<char> buffer;
				io::VectorSink sink(buffer);
				game::Protocol::OutgoingPacket packet(sink);
				game::server_write::monsterMove(packet, getControlled().getGuid(), oldPosition, newPosition, moveTime);

				forEachSubscriberInSight(
					getControlled().getWorldInstance()->getGrid(),
					tile,
					[&packet, &buffer](ITileSubscriber &subscriber)
				{
					subscriber.sendPacket(packet, buffer);
				});
			}

			m_moveUpdate.setEnd(getCurrentTime() + moveTime);
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
			GameCharacter *character = static_cast<GameCharacter*>(&attacker);
			controlled.setLootRecipient(attacker.getGuid(), character->getGroupId());

			getControlled().addFlag(unit_fields::DynamicFlags, game::unit_dynamic_flags::OtherTagger);
		}
	}

}
