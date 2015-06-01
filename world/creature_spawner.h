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
#include "data/unit_entry.h"
#include <boost/optional.hpp>

namespace wowpp
{
	class WorldInstance;
	class GameUnit;

	/// Manages a spawn point and all creatures which are spawned by this point info.
	/// It also adds spawned creatures to the given world instance.
	class CreatureSpawner final
	{
	public:
		/// Creates a new instance of the CreatureSpawner class and initializes it.
		/// @param world The world instance this spawner belongs to. Creatures will be spawned in this world.
		/// @param entry Base data of the creatures to be spawned.
		/// @param maxCount Maximum number of creatures to spawn.
		/// @param respawnDelay The delay in milliseconds until a creature of this spawn point will respawn.
		/// @param centerX The x coordinate of the spawn center.
		/// @param centerY The y coordinate of the spawn center.
		/// @param centerZ The z coordinate of the spawn center.
		/// @param rotation The rotation of the creatures. If nothing provided, will be randomized.
		/// @param radius The radius in which creatures will spawn. Also used as the maximum random walk distance.
		explicit CreatureSpawner(
			WorldInstance &world,
			const UnitEntry &entry,
			size_t maxCount,
			GameTime respawnDelay,
			float centerX,
			float centerY,
			float centerZ,
			boost::optional<float> rotation,
			float radius);
		virtual ~CreatureSpawner();

	private:

		/// Spawns one more creature and adds it to the world.
		void spawnOne();
		/// Callback which is fired after a respawn delay invalidated.
		void onSpawnTime();
		/// Callback which is fired after a creature despawned.
		void onRemoval(GameUnit &removed);
		/// Setup a new respawn timer.
		void setRespawnTimer();

	private:

		typedef std::vector<std::shared_ptr<GameUnit>> OwnedCreatures;

		WorldInstance &m_world;
		const UnitEntry &m_entry;
		const size_t m_maxCount;
		const GameTime m_respawnDelay;
		const float m_centerX, m_centerY, m_centerZ;
		const boost::optional<float> m_rotation;
		const float m_radius;
		size_t m_currentlySpawned;
		OwnedCreatures m_creatures;
		Countdown m_respawnCountdown;
	};
}
