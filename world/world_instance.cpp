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

#include "world_instance.h"
#include "world_instance_manager.h"
#include "log/default_log_levels.h"
#include "data/unit_entry.h"
#include "game/game_unit.h"
#include "creature_spawner.h"
#include "tile_visibility_change.h"
#include <algorithm>
#include <array>
#include <memory>
#include <cassert>

namespace wowpp
{
	WorldInstance::WorldInstance(WorldInstanceManager &manager, 
		const MapEntry &mapEntry,
		UInt32 id, 
		std::unique_ptr<VisibilityGrid> visibilityGrid,
		IdGenerator<UInt64> &objectIdGenerator,
		DataLoadContext::GetRace getRace,
		DataLoadContext::GetClass getClass,
		DataLoadContext::GetLevel getLevel
		)
		: m_manager(manager)
		, m_visibilityGrid(std::move(visibilityGrid))
		, m_objectIdGenerator(objectIdGenerator)
		, m_mapEntry(mapEntry)
		, m_id(id)
		, m_getRace(getRace)
		, m_getClass(getClass)
		, m_getLevel(getLevel)
	{
		// Add creature spawners
		for (auto &spawn : m_mapEntry.spawns)
		{
			// Create a new spawner
			std::unique_ptr<CreatureSpawner> spawn(new CreatureSpawner(
				*this,
				*spawn.unit,
				spawn.maxCount,
				spawn.respawnDelay,
				spawn.position[0], spawn.position[1], spawn.position[2],
				spawn.rotation,
				spawn.radius));
			m_creatureSpawners.push_back(std::move(spawn));
		}

		ILOG("Created instance of map " << m_mapEntry.id);
	}

	std::shared_ptr<GameUnit> WorldInstance::spawnCreature(
		const UnitEntry &entry,
		float x, float y, float z, float o,
		float randomWalkRadius)
	{
		// Create the unit
		auto spawned = std::make_shared<GameUnit>(
			m_getRace,
			m_getClass,
			m_getLevel);
		spawned->initialize();
		spawned->setGuid(createGUID(m_objectIdGenerator.generateId(), entry.id, high_guid::Unit));
		spawned->setMapId(m_mapEntry.id);
		spawned->relocate(x, y, z, o);

		spawned->setLevel(entry.minLevel);
		spawned->setClass(entry.unitClass);														//TODO
		spawned->setUInt32Value(unit_fields::FactionTemplate, entry.allianceFactionID);			//TODO
		spawned->setGender(game::gender::Max);
		//spawned->setUInt32Value(unit_fields::Bytes0, 0x00020000);								//TODO
		spawned->setUInt32Value(unit_fields::DisplayId, entry.maleModel);						//TODO
		spawned->setUInt32Value(unit_fields::NativeDisplayId, entry.maleModel);					//TODO
		spawned->setUInt32Value(unit_fields::BaseHealth, 20);									//TODO
		spawned->setUInt32Value(unit_fields::Health, entry.minLevelHealth);
		spawned->setUInt32Value(unit_fields::MaxHealth, entry.minLevelHealth);
		spawned->setUInt32Value(object_fields::Entry, entry.id);
		//spawned->setUInt32Value(unit_fields::Bytes2, 0x00001001);								//TODO
		spawned->setUInt32Value(unit_fields::UnitFlags, 0x0008);

		return spawned;
	}

	void WorldInstance::update()
	{
		// Iterate all game objects added to this world which need to be updated
		for (auto &gameObject : m_objectsById)
		{
			//TODO: Update object

		}
	}

	void WorldInstance::addGameObject(GameObject& added)
	{
		// Assign new guid to object if needed
		if (added.getGuid() == 0)
		{
			const UInt64 lower = static_cast<UInt64>(m_objectIdGenerator.generateId());
			const UInt64 entry = static_cast<UInt64>(added.getUInt32Value(object_fields::Entry));
			added.setGuid(createGUID(lower, entry, entry ? high_guid::Unit : high_guid::Player));
		}

		// Add this game object to the list of objects
		m_objectsById.insert(
			std::make_pair(added.getGuid(), &added));
	}

	void WorldInstance::removeGameObject(GameObject &remove)
	{
		// Remove game object from the object list
		m_objectsById.erase(remove.getGuid());
	}
}
