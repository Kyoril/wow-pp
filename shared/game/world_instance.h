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
#include "game/game_object.h"
#include "common/id_generator.h"
#include "data/map_entry.h"
#include "data/data_load_context.h"
#include "game/visibility_grid.h"
#include "creature_spawner.h"
#include "world_object_spawner.h"
#include "boost/signals2.hpp"
#include "map.h"
#include <memory>
#include <vector>

namespace wowpp
{
	class WorldInstanceManager;
	class GameUnit;
	class GameCreature;
	class WorldObject;
	class Universe;

	inline UInt32 createMapGUID(UInt32 low, UInt32 map) { return static_cast<UInt32>(low | (map << 16)); }

	/// Manages one instance of a game world.
	class WorldInstance final
	{
	private:

		/// Loaded static map data like nav meshs, world geometry etc.
		static std::map<UInt32, Map> MapData;

	private:

		typedef std::unordered_map<UInt64, GameObject*> GameObjectsById;
		typedef std::vector<std::unique_ptr<CreatureSpawner>> CreatureSpawners;
		typedef std::vector<std::unique_ptr<WorldObjectSpawner>> ObjectSpawners;
		typedef std::map<UInt64, std::shared_ptr<GameCreature>> SummonedCreatures;
		
	public:

		/// Fired when the world instance is about to be destroyed. TODO: Not fired at the moment.
		boost::signals2::signal<void()> willBeDestroyed;

	public:

		/// Creates a new instance of the WorldInstance class and initializes it.
		/// @param mapEntry Map data of which to create an instance of.
		/// @param id The unique id of this instance.
		/// @param visibilityGrid An instance of a visibility grid to be used by this world instance.
		/// @param objectIdGenerator Instance of an id generator which will be used to increment creature low GUID values as they spawn.
		/// @param getRace Callback to obtain informations about a race.
		/// @param getClass Callback to obtain informations about a class.
		/// @param getLevel Callback to obtain informations about a level.
		explicit WorldInstance(WorldInstanceManager &manager, 
			Universe &universe,
			const MapEntry &mapEntry,
			UInt32 id, 
			std::unique_ptr<VisibilityGrid> visibilityGrid,
			IdGenerator<UInt64> &objectIdGenerator,
			DataLoadContext::GetRace getRace,
			DataLoadContext::GetClass getClass,
			DataLoadContext::GetLevel getLevel,
			DataLoadContext::GetSpell getSpell,
			const String &dataPath
			);

		/// Creates a new creature which will belong to this world instance. However,
		/// the creature won't be spawned yet. This method is used by CreatureSpawner.
		std::shared_ptr<GameCreature> spawnCreature(
			const UnitEntry &entry,
			float x, float y, float z, float o,
			float randomWalkRadius);
		std::shared_ptr<GameCreature> spawnSummonedCreature(
			const UnitEntry &entry,
			float x, float y, float z, float o);
		std::shared_ptr<WorldObject> spawnWorldObject(
			const ObjectEntry &entry,
			float x, float y, float z, float o,
			float radius);

		/// Gets the manager of this and all other instances.
		WorldInstanceManager &getManager() { return m_manager; }
		/// Gets the id of this instance.
		UInt32 getId() const { return m_id; }
		/// Gets the map id of this instance.
		UInt32 getMapId() const { return m_mapEntry.id; }
		/// 
		const VisibilityGrid &getGrid() const { return *m_visibilityGrid; }
		/// 
		VisibilityGrid &getGrid() { return *m_visibilityGrid; }
		/// 
		Universe &getUniverse() { return m_universe; }
		/// Adds a game object to this world instance.
		void addGameObject(GameObject& added);
		/// Removes a specific game object from this world.
		void removeGameObject(GameObject &remove);
		/// Finds a game object by it's guid.
		GameObject *findObjectByGUID(UInt64 guid);
		/// Updates this world instance. Should be called once per tick.
		void update();
		/// Gets the map data of this instance. Note that instances share the same map data to save
		/// memory.
		Map *getMapData() { return m_map; }
		/// 
		CreatureSpawner *findCreatureSpawner(const String &name);
		/// 
		WorldObjectSpawner *findObjectSpawner(const String &name);

		/// Calls a specific callback method for every game object added to the world.
		/// An object can be everything, from a player over a creature to a chest.
		template<typename F>
		void foreachObject(F f)
		{
			for (auto &object : m_objectsById)
			{
				f(*object.second);
			}
		}

	private:

		void onObjectMoved(GameObject &object, float oldX, float oldY, float oldZ, float oldO);

	private:

		WorldInstanceManager &m_manager;
		Universe &m_universe;
		std::unique_ptr<VisibilityGrid> m_visibilityGrid;
		IdGenerator<UInt64> &m_objectIdGenerator;
		GameObjectsById m_objectsById;
		const MapEntry &m_mapEntry;
		UInt32 m_id;
		DataLoadContext::GetRace m_getRace;
		DataLoadContext::GetClass m_getClass;
		DataLoadContext::GetLevel m_getLevel;
		DataLoadContext::GetSpell m_getSpell;
		CreatureSpawners m_creatureSpawners;
		std::map<String, CreatureSpawner*> m_creatureSpawnsByName;
		ObjectSpawners m_objectSpawners;
		std::map<String, WorldObjectSpawner*> m_objectSpawnsByName;
		SummonedCreatures m_creatureSummons;
		Map *m_map;
	};
}
