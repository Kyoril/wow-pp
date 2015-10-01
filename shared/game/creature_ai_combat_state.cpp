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
#include "game_creature.h"
#include "world_instance.h"
#include "data/faction_template_entry.h"
#include "binary_io/vector_sink.h"
#include "game_protocol/game_protocol.h"
#include "each_tile_in_sight.h"
#include "data/trigger_entry.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	CreatureAICombatState::CreatureAICombatState(CreatureAI &ai, GameUnit &victim)
		: CreatureAIState(ai)
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

		// Flag controlled unit for combat
		controlled.addFlag(unit_fields::UnitFlags, game::unit_flags::InCombat);

		// Watch for threat events
		controlled.threatened.connect([this](GameUnit &threatener, float amount)
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
		// TODO

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
		}

		auto &threatEntry = it->second;
		threatEntry.amount += amount;

		// Attacker should enter combat
		threatener.addFlag(unit_fields::UnitFlags, game::unit_flags::InCombat);
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
		}
		else if (!newVictim)
		{
			// No victim found (threat list probably empty?)
			// Return to idle state
			getAI().idle();
		}
	}

}
