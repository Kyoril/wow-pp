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

#include "world_object_spawner.h"
#include "world_instance.h"
#include "world_instance_manager.h"
#include "game_world_object.h"
#include "common/erase_by_move.h"
#include "log/default_log_levels.h"
#include "common/utilities.h"
#include "shared/proto_data/objects.pb.h"
#include "universe.h"
#include <memory>
#include <cassert>

namespace wowpp
{
	WorldObjectSpawner::WorldObjectSpawner(
		WorldInstance &world,
		const proto::ObjectEntry &entry,
		size_t maxCount,
		GameTime respawnDelay,
		math::Vector3 center,
		boost::optional<float> orientation,
		const std::array<float, 4> &rotation,
		float radius,
		UInt32 animProgress,
		UInt32 state)
		: m_world(world)
		, m_entry(entry)
		, m_maxCount(maxCount)
		, m_respawnDelay(respawnDelay)
		, m_center(center)
		, m_orientation(orientation)
		, m_rotation(rotation)
		, m_radius(radius)
		, m_currentlySpawned(0)
		, m_respawnCountdown(world.getUniverse().getTimers())
		, m_animProgress(animProgress)
		, m_state(state)
	{
		// Cap AnimProgress to 100, because higher value causes troubles at the client somehow when respawning
		if (m_animProgress > 100) m_animProgress = 100;

		// Immediatly spawn all objects
		for (size_t i = 0; i < m_maxCount; ++i)
		{
			spawnOne();
		}

		m_respawnCountdown.ended.connect(
			std::bind(&WorldObjectSpawner::onSpawnTime, this));
	}

	WorldObjectSpawner::~WorldObjectSpawner()
	{
	}

	void WorldObjectSpawner::spawnOne()
	{
		assert(m_currentlySpawned < m_maxCount);

		// TODO: Generate random point and if needed, random rotation
		const math::Vector3 position(m_center);
		const float o = m_orientation ? *m_orientation : 0.0f;

		// Spawn a new creature
		auto spawned = m_world.spawnWorldObject(m_entry, position, o, m_radius);
		spawned->setFloatValue(object_fields::ScaleX, m_entry.scale());
		spawned->setFloatValue(world_object_fields::Rotation + 0, m_rotation[0]);
		spawned->setFloatValue(world_object_fields::Rotation + 1, m_rotation[1]);
		float rot2 = m_rotation[2], rot3 = m_rotation[3];
		if (rot2 == 0.0f && rot3 == 0.0f)
		{
			rot2 = sin(o / 2);
			rot3 = cos(o / 2);
		}
		spawned->setFloatValue(world_object_fields::Rotation + 2, rot2);
		spawned->setFloatValue(world_object_fields::Rotation + 3, rot3);
		spawned->setFloatValue(world_object_fields::Facing, o);
		spawned->setUInt32Value(world_object_fields::AnimProgress, m_animProgress);
		spawned->setUInt32Value(world_object_fields::State, m_state);

		// watch for destruction
		spawned->destroy = std::bind(&WorldObjectSpawner::onRemoval, this, std::placeholders::_1);
		m_world.addGameObject(*spawned);

		// Remember that creature
		m_objects.push_back(std::move(spawned));
		++m_currentlySpawned;
	}

	void WorldObjectSpawner::onSpawnTime()
	{
		spawnOne();
		setRespawnTimer();
	}

	void WorldObjectSpawner::onRemoval(GameObject &removed)
	{
		--m_currentlySpawned;

		const auto i = std::find_if(
			std::begin(m_objects),
			std::end(m_objects),
			[&removed](const std::shared_ptr<WorldObject> &element)
			{
				return (element.get() == &removed);
			});

		assert(i != m_objects.end());
		eraseByMove(m_objects, i);

		setRespawnTimer();
	}

	void WorldObjectSpawner::setRespawnTimer()
	{
		if (m_currentlySpawned >= m_maxCount)
		{
			return;
		}

		m_respawnCountdown.setEnd(
			getCurrentTime() + m_respawnDelay);
	}
}
