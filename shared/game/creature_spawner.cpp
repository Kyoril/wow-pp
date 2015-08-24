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

#include "creature_spawner.h"
#include "world_instance.h"
#include "world_instance_manager.h"
#include "game_unit.h"
#include "game_creature.h"
#include "common/erase_by_move.h"
#include "log/default_log_levels.h"
#include "common/utilities.h"
#include "universe.h"
#include <memory>
#include <cassert>

namespace wowpp
{
	CreatureSpawner::CreatureSpawner(
		WorldInstance &world,
		const UnitEntry &entry,
		size_t maxCount,
		GameTime respawnDelay,
		float centerX,
		float centerY,
		float centerZ,
		boost::optional<float> rotation,
		float radius,
		bool active,
		bool respawn)
		: m_world(world)
		, m_entry(entry)
		, m_maxCount(maxCount)
		, m_respawnDelay(respawnDelay)
		, m_centerX(centerX)
		, m_centerY(centerY)
		, m_centerZ(centerZ)
		, m_rotation(rotation)
		, m_radius(radius)
		, m_active(active)
		, m_respawn(respawn)
		, m_currentlySpawned(0)
		, m_respawnCountdown(world.getUniverse().getTimers())
	{
		if (m_active)
		{
			for (size_t i = 0; i < m_maxCount; ++i)
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
		assert(m_currentlySpawned < m_maxCount);

		// TODO: Generate random point and if needed, random rotation
		const float x = m_centerX, y = m_centerY, z = m_centerZ;
		const float o = m_rotation ? *m_rotation : 0.0f;

		// Spawn a new creature
		auto spawned = m_world.spawnCreature(
			m_entry,
			x, y, z, o,
			m_radius
			);
		spawned->setFloatValue(object_fields::ScaleX, m_entry.scale);
		spawned->clearUpdateMask();

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

		assert(i != m_creatures.end());
		eraseByMove(m_creatures, i);

		setRespawnTimer();
	}

	void CreatureSpawner::setRespawnTimer()
	{
		if (m_currentlySpawned >= m_maxCount)
		{
			return;
		}

		m_respawnCountdown.setEnd(
			getCurrentTime() + m_respawnDelay);
	}

	void CreatureSpawner::setState(bool active)
	{
		if (m_active != active)
		{
			if (active && !m_currentlySpawned)
			{
				for (size_t i = 0; i < m_maxCount; ++i)
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
