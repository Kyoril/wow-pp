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
#include "creature_spawner.h"
#include "world_instance.h"
#include "world_instance_manager.h"
#include "game_unit.h"
#include "game_creature.h"
#include "common/erase_by_move.h"
#include "log/default_log_levels.h"
#include "common/utilities.h"
#include "shared/proto_data/units.pb.h"
#include "universe.h"

namespace wowpp
{
	CreatureSpawner::CreatureSpawner(
	    WorldInstance &world,
	    const proto::UnitEntry &entry,
		const proto::UnitSpawnEntry &spawnEntry)
		: m_world(world)
		, m_entry(entry)
		, m_spawnEntry(spawnEntry)
		, m_active(spawnEntry.isactive())
		, m_respawn(spawnEntry.respawn())
		, m_currentlySpawned(0)
		, m_respawnCountdown(world.getUniverse().getTimers())
		, m_location(spawnEntry.positionx(), spawnEntry.positiony(), spawnEntry.positionz())
	{
		if (m_active)
		{
			for (size_t i = 0; i < m_spawnEntry.maxcount(); ++i)
			{
				spawnOne();
			}
		}

		m_respawnCountdown.ended.connect(
		    std::bind(&CreatureSpawner::onSpawnTime, this));
	}

	CreatureSpawner::~CreatureSpawner()
	{
	}

	void CreatureSpawner::spawnOne()
	{
		// TODO: Generate random point and if needed, random rotation
		const math::Vector3 location(m_spawnEntry.positionx(), m_spawnEntry.positiony(), m_spawnEntry.positionz());
		const float o = m_spawnEntry.rotation();

		// Spawn a new creature
		auto spawned = m_world.spawnCreature(m_entry, location, o, m_spawnEntry.radius());
		spawned->setRandomPointGenerator(std::bind(&CreatureSpawner::randomPoint, this));
		spawned->setFloatValue(object_fields::ScaleX, m_entry.scale());
		if (m_spawnEntry.defaultemote() != 0)
		{
			spawned->setUInt32Value(unit_fields::NpcEmoteState, m_spawnEntry.defaultemote());
		}
		spawned->clearUpdateMask();
		
		game::CreatureMovement movement = game::creature_movement::None;
		if (m_spawnEntry.movement() >= game::creature_movement::Invalid)
		{
			WLOG("Invalid movement type for creature spawn - spawn ignored");
		}
		else
		{
			movement = static_cast<game::CreatureMovement>(m_spawnEntry.movement());
		}
		spawned->setMovementType(movement);
		
		// Update stand state
		spawned->setStandState(static_cast<UnitStandState>(m_spawnEntry.standstate()));

		// watch for destruction
		spawned->destroy = std::bind(&CreatureSpawner::onRemoval, this, std::placeholders::_1);
		m_world.addGameObject(*spawned);

		// Remember that creature
		m_creatures.push_back(std::move(spawned));
		++m_currentlySpawned;
	}

	void CreatureSpawner::onSpawnTime()
	{
		spawnOne();
		setRespawnTimer();
	}

	void CreatureSpawner::onRemoval(GameObject &removed)
	{
		--m_currentlySpawned;

		const auto i = std::find_if(
		                   std::begin(m_creatures),
		                   std::end(m_creatures),
		                   [&removed](const std::shared_ptr<GameUnit> &element)
		{
			return (element.get() == &removed);
		});

		ASSERT(i != m_creatures.end());
		eraseByMove(m_creatures, i);

		setRespawnTimer();
	}

	void CreatureSpawner::setRespawnTimer()
	{
		if (m_currentlySpawned >= m_spawnEntry.maxcount())
		{
			return;
		}

		m_respawnCountdown.setEnd(
		    getCurrentTime() + m_spawnEntry.respawndelay());
	}

	const math::Vector3 & CreatureSpawner::randomPoint()
	{
		auto *mapData = m_world.getMapData();
		if (mapData)
		{
			if (mapData->getRandomPointOnGround(m_location, 5.0f, m_randomPoint))
			{
				return m_randomPoint;
			}
			else
			{
				return m_location;
			}
		}
	}

	void CreatureSpawner::setState(bool active)
	{
		if (m_active != active)
		{
			if (active && !m_currentlySpawned)
			{
				for (size_t i = 0; i < m_spawnEntry.maxcount(); ++i)
				{
					spawnOne();
				}
			}
			else
			{
				m_respawnCountdown.cancel();
			}

			m_active = active;
		}
	}

	void CreatureSpawner::setRespawn(bool enabled)
	{
		if (m_respawn != enabled)
		{
			if (!enabled)
			{
				m_respawnCountdown.cancel();
			}
			else
			{
				setRespawnTimer();
			}

			m_respawn = enabled;
		}
	}

}
