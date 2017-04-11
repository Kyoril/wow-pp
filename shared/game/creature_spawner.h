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

#pragma once

#include "common/typedefs.h"
#include "common/countdown.h"
#include "defines.h"
#include "math/vector3.h"

namespace wowpp
{
	class WorldInstance;
	class GameObject;
	class GameUnit;
	class GameCreature;
	namespace proto
	{
		class UnitEntry;
		class UnitSpawnEntry;
	}

	/// Manages a spawn point and all creatures which are spawned by this point info.
	/// It also adds spawned creatures to the given world instance.
	class CreatureSpawner final
	{
		typedef std::vector<std::shared_ptr<GameCreature>> OwnedCreatures;

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
			const proto::UnitEntry &entry,
			const proto::UnitSpawnEntry &spawnEntry);
		virtual ~CreatureSpawner();

		///
		const OwnedCreatures &getCreatures() const {
			return m_creatures;
		}
		///
		bool isActive() const {
			return m_active;
		}
		///
		bool isRespawnEnabled() const {
			return m_respawn;
		}
		///
		void setState(bool active);
		///
		void setRespawn(bool enabled);
		/// Gets a random movement point in the spawn radius.
		const math::Vector3 &randomPoint();

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

		WorldInstance &m_world;
		const proto::UnitEntry &m_entry;
		const proto::UnitSpawnEntry &m_spawnEntry;
		bool m_active;
		bool m_respawn;
		size_t m_currentlySpawned;
		OwnedCreatures m_creatures;
		Countdown m_respawnCountdown;
		math::Vector3 m_location;
		std::vector<math::Vector3> m_randomPoints;
		Int8 m_lastPoint;
	};
}
