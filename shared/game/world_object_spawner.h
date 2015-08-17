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

#pragma once

#include "common/typedefs.h"
#include "common/countdown.h"
#include "data/object_entry.h"
#include <boost/optional.hpp>

namespace wowpp
{
	class WorldInstance;
	class GameObject;
	class GameUnit;
	class WorldObject;

	/// 
	class WorldObjectSpawner final
	{
	public:
		/// 
		explicit WorldObjectSpawner(
			WorldInstance &world,
			const ObjectEntry &entry,
			size_t maxCount,
			GameTime respawnDelay,
			float centerX,
			float centerY,
			float centerZ,
			boost::optional<float> orientation,
			const std::array<float, 4> &rotation,
			float radius,
			UInt32 animProgress,
			UInt32 state);
		virtual ~WorldObjectSpawner();

	private:

		/// Spawns one more creature and adds it to the world.
		void spawnOne();
		/// Callback which is fired after a respawn delay invalidated.
		void onSpawnTime();
		/// Callback which is fired after a creature despawned.
		void onRemoval(GameObject &removed);
		/// Setup a new respawn timer.
		void setRespawnTimer();

	private:

		typedef std::vector<std::shared_ptr<WorldObject>> OwnedObjects;

		WorldInstance &m_world;
		const ObjectEntry &m_entry;
		const size_t m_maxCount;
		const GameTime m_respawnDelay;
		const float m_centerX, m_centerY, m_centerZ;
		const boost::optional<float> m_orientation;
		const std::array<float, 4> &m_rotation;
		const float m_radius;
		size_t m_currentlySpawned;
		OwnedObjects m_objects;
		Countdown m_respawnCountdown;
		UInt32 m_animProgress;
		UInt32 m_state;
	};
}
