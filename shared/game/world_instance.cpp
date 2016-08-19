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
#include "world_instance.h"
#include "world_instance_manager.h"
#include "log/default_log_levels.h"
#include "proto_data/project.h"
#include "game_unit.h"
#include "game_creature.h"
#include "game_world_object.h"
#include "creature_spawner.h"
#include "tile_visibility_change.h"
#include "visibility_tile.h"
#include "each_tile_in_region.h"
#include "binary_io/vector_sink.h"
#include "each_tile_in_sight.h"
#include "common/utilities.h"
#include "trigger_handler.h"
#include "creature_ai.h"
#include "universe.h"
#include "unit_mover.h"

namespace wowpp
{
	namespace
	{
		static TileIndex2D getObjectTile(GameObject &object, VisibilityGrid &grid)
		{
			math::Vector3 location(object.getLocation());

			TileIndex2D gridIndex;
			grid.getTilePosition(location, gridIndex[0], gridIndex[1]);

			return gridIndex;
		}

		static void createValueUpdateBlock(GameObject &object, GameCharacter &receiver, std::vector<std::vector<char>> &out_blocks)
		{
			// Write create object packet
			std::vector<char> createBlock;
			io::VectorSink sink(createBlock);
			io::Writer writer(sink);
			{
				UInt8 updateType = 0x00;						// Update type (0x00 = UPDATE_VALUES)

				// Header with object guid and type
				UInt64 guid = object.getGuid();
				writer
				        << io::write<NetUInt8>(updateType);

				UInt64 guidCopy = guid;
				UInt8 packGUID[8 + 1];
				packGUID[0] = 0;
				size_t size = 1;
				for (UInt8 i = 0; guidCopy != 0; ++i)
				{
					if (guidCopy & 0xFF)
					{
						packGUID[0] |= UInt8(1 << i);
						packGUID[size] = UInt8(guidCopy & 0xFF);
						++size;
					}

					guidCopy >>= 8;
				}
				writer.sink().write((const char *)&packGUID[0], size);

				// Write values update
				object.writeValueUpdateBlock(writer, receiver, false);
			}

			// Add block
			out_blocks.push_back(createBlock);
		}
	}

	std::map<UInt32, Map> WorldInstance::MapData;

	WorldInstance::WorldInstance(
	    WorldInstanceManager &manager,
	    Universe &universe,
	    game::ITriggerHandler &triggerHandler,
	    proto::Project &project,
	    const proto::MapEntry &mapEntry,
	    UInt32 id,
	    std::unique_ptr<UnitFinder> unitFinder,
	    std::unique_ptr<VisibilityGrid> visibilityGrid,
	    IdGenerator<UInt64> &objectIdGenerator,
	    const String &dataPath
	)
		: m_manager(manager)
		, m_universe(universe)
		, m_triggerHandler(triggerHandler)
		, m_unitFinder(std::move(unitFinder))
		, m_visibilityGrid(std::move(visibilityGrid))
		, m_objectIdGenerator(objectIdGenerator)
		, m_itemIdGenerator(1)		// Start at an id of 1 as 0 is invalid
		, m_project(project)
		, m_mapEntry(mapEntry)
		, m_id(id)
		, m_map(nullptr)
	{
		// Create map instance if needed
		auto mapIt = MapData.find(m_mapEntry.id());
		if (mapIt == MapData.end())
		{
			// Load map
			MapData.insert(std::make_pair(m_mapEntry.id(), Map(m_mapEntry, dataPath)));
			mapIt = MapData.find(m_mapEntry.id());
			if (mapIt != MapData.end()) {
				m_map = &mapIt->second;
			}
		}
		else
		{
			m_map = &mapIt->second;
		}

		// Add object spawners
		for (int i = 0; i < m_mapEntry.objectspawns_size(); ++i)
		{
			// Create a new spawner
			const auto &spawn = m_mapEntry.objectspawns(i);

			const auto *objectEntry = m_project.objects.getById(spawn.objectentry());
			assert(objectEntry);

			std::unique_ptr<WorldObjectSpawner> spawner(new WorldObjectSpawner(
			            *this,
			            *objectEntry,
			            spawn.maxcount(),
			            spawn.respawndelay(),
			            math::Vector3(spawn.positionx(), spawn.positiony(), spawn.positionz()),
			            spawn.orientation(),
			{ spawn.rotationw(), spawn.rotationx(), spawn.rotationy(), spawn.rotationz() },
			spawn.radius(),
			spawn.animprogress(),
			spawn.state()));
			m_objectSpawners.push_back(std::move(spawner));
			if (!spawn.name().empty())
			{
				m_objectSpawnsByName[spawn.name()] = m_objectSpawners.back().get();
			}
		}

		// Add creature spawners
		for (int i = 0; i < m_mapEntry.unitspawns_size(); ++i)
		{
			// Create a new spawner
			const auto &spawn = m_mapEntry.unitspawns(i);

			const auto *unitEntry = m_project.units.getById(spawn.unitentry());
			assert(unitEntry);

			std::unique_ptr<CreatureSpawner> spawner(new CreatureSpawner(
				*this,
				*unitEntry,
				spawn));
			/*
			            spawn.maxcount(),
			            spawn.respawndelay(),
			            math::Vector3(spawn.positionx(), spawn.positiony(), spawn.positionz()),
			            spawn.rotation(),
			            spawn.defaultemote(),
			            spawn.radius(),
			            spawn.isactive(),
			            spawn.respawn(),
						movement));*/

			m_creatureSpawners.push_back(std::move(spawner));

			if (!spawn.name().empty())
			{
				m_creatureSpawnsByName[spawn.name()] = m_creatureSpawners.back().get();
			}
		}

		ILOG("Created instance of map " << m_mapEntry.id());
	}

	std::shared_ptr<GameCreature> WorldInstance::spawnCreature(
	    const proto::UnitEntry &entry,
	    math::Vector3 position,
	    float o,
	    float randomWalkRadius)
	{
		// Create the unit
		auto spawned = std::make_shared<GameCreature>(
		                   m_project,
		                   m_universe.getTimers(),
		                   entry);
		spawned->initialize();
		spawned->setGuid(createEntryGUID(m_objectIdGenerator.generateId(), entry.id(), guid_type::Unit));	// RealmID (TODO: these spawns don't need to have a specific realm id)
		spawned->setMapId(m_mapEntry.id());
		spawned->relocate(position, o);

		return spawned;
	}

	std::shared_ptr<GameCreature> WorldInstance::spawnSummonedCreature(const proto::UnitEntry &entry, math::Vector3 position, float o)
	{
		// Create the unit
		auto spawned = std::make_shared<GameCreature>(
		                   m_project,
		                   m_universe.getTimers(),
		                   entry);
		spawned->setGuid(createEntryGUID(m_objectIdGenerator.generateId(), entry.id(), guid_type::Unit));	// RealmID (TODO: these spawns don't need to have a specific realm id)
		spawned->initialize();
		spawned->setMapId(m_mapEntry.id());
		spawned->relocate(position, o);

		m_creatureSummons.insert(std::make_pair(spawned->getGuid(), spawned));
		spawned->destroy = [this](GameObject &obj) 
		{
			auto it = m_creatureSummons.find(obj.getGuid());
			if (it != m_creatureSummons.end())
			{
				m_creatureSummons.erase(it);
			}
		};

		return spawned;
	}

	std::shared_ptr<WorldObject> WorldInstance::spawnWorldObject(const proto::ObjectEntry &entry, math::Vector3 position, float o, float radius)
	{
		// Create the unit
		auto spawned = std::make_shared<WorldObject>(
		                   m_project,
		                   m_universe.getTimers(),
		                   entry);
		spawned->setGuid(createEntryGUID(m_objectIdGenerator.generateId(), entry.id(), guid_type::GameObject));	// RealmID (TODO: these spawns don't need to have a specific realm id)
		spawned->initialize();
		spawned->setMapId(m_mapEntry.id());
		spawned->relocate(position, o);

		return spawned;
	}

	void WorldInstance::update()
	{
		// Iterate all game objects added to this world which need to be updated
		if (!m_objectUpdates.empty())
		{
			for (auto &obj : m_objectUpdates)
			{
				updateObject(*obj);
			}
		}

		m_objectUpdates.clear();
	}

	void WorldInstance::flushObjectUpdate(UInt64 guid)
	{
		auto *object = findObjectByGUID(guid);
		if (object)
		{
			updateObject(*object);
		}
	}

	void WorldInstance::addGameObject(GameObject &added)
	{
		auto guid = added.getGuid();

		// Add this game object to the list of objects
		m_objectsById.insert(
		    std::make_pair(guid, &added));

		// Get object location
		math::Vector3 location(added.getLocation());

		// Transform into grid location
		TileIndex2D gridIndex;
		if (!m_visibilityGrid->getTilePosition(location, gridIndex[0], gridIndex[1]))
		{
			// TODO: Error?
			ELOG("Could not resolve grid location!");
			return;
		}

		// Get grid tile
		auto &tile = m_visibilityGrid->requireTile(gridIndex);

		// Add to the tile
		tile.getGameObjects().add(&added);
		added.setWorldInstance(this);

		// Spawn ourself for new watchers
		forEachTileInSight(
		    *m_visibilityGrid,
		    tile.getPosition(),
		    [&added](VisibilityTile & tile)
		{
			for (auto *subscriber : tile.getWatchers().getElements())
			{
				auto *character = subscriber->getControlledObject();
				if (!character) {
					continue;
				}

				if (!added.canSpawnForCharacter(*character))
				{
					continue;
				}

				// Create update packet
				std::vector<std::vector<char>> blocks;
				createUpdateBlocks(added, *character, blocks);

				// Create the packet
				std::vector<char> buffer;
				io::VectorSink sink(buffer);
				game::Protocol::OutgoingPacket packet(sink);
				game::server_write::compressedUpdateObject(packet, blocks);

				subscriber->sendPacket(packet, buffer);
			}
		});

		// Enable trigger execution
		if (added.isCreature())
		{
			GameUnit *unitObj = reinterpret_cast<GameUnit *>(&added);
			unitObj->unitTrigger.connect([this](const proto::TriggerEntry & trigger, GameUnit & owner) {
				m_triggerHandler.executeTrigger(trigger, game::TriggerContext(&owner), 0);
			});
		}
		else if (added.isWorldObject())
		{
			WorldObject *worldObj = reinterpret_cast<WorldObject *>(&added);
			worldObj->objectTrigger.connect([this](const proto::TriggerEntry & trigger, WorldObject & owner) {
				m_triggerHandler.executeTrigger(trigger, game::TriggerContext(&owner), 0);
			});
		}

		// Notify about being spawned
		added.clearUpdateMask();
		added.spawned();

		// Add to unit finder if it is a unit
		if (added.isCreature() ||
		        added.isGameCharacter())
		{
			GameUnit *unit = dynamic_cast<GameUnit *>(&added);
			if (unit) {
				m_unitFinder->addUnit(*unit);
			}
		}

		// Watch for object location changes
		added.moved.connect(
		    std::bind(&WorldInstance::onObjectMoved, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}

	void WorldInstance::removeGameObject(GameObject &remove)
	{
		auto guid = remove.getGuid();

		// Unit specific despawn logic
		if (remove.isCreature() || remove.isGameCharacter())
		{
			GameUnit *unit = dynamic_cast<GameUnit *>(&remove);
			if (unit) 
			{
				// Remove unit from the unit finder
				m_unitFinder->removeUnit(*unit);

				// Also remove all dynamic objects
				unit->removeAllDynamicObjects();
			}
		}

		// Transform into grid location
		TileIndex2D gridIndex = getObjectTile(remove, *m_visibilityGrid);

		// Get grid tile
		auto &tile = m_visibilityGrid->requireTile(gridIndex);
		tile.getGameObjects().remove(&remove);

		// Remove object from the list of updats
		if (remove.wasUpdated())
		{
			m_objectUpdates.erase(&remove);
		}

		// Create the packet
		std::vector<char> buffer;
		io::VectorSink sink(buffer);
		game::Protocol::OutgoingPacket packet(sink);
		game::server_write::destroyObject(packet, remove.getGuid(), false);

		// Remove game object from the object list
		m_objectsById.erase(guid);
		remove.setWorldInstance(nullptr);

		// Notify absout despawn
		remove.despawned(remove);

		// Despawn for watchers
		forEachTileInSight(
		    *m_visibilityGrid,
		    tile.getPosition(),
		    [&packet, &buffer](VisibilityTile & tile)
		{
			for (auto *subscriber : tile.getWatchers().getElements())
			{
				subscriber->sendPacket(packet, buffer);
			}
		});

		// Destroy this object
		if (remove.destroy)
		{
			remove.destroy(remove);
		}
	}

	GameObject *WorldInstance::findObjectByGUID(UInt64 guid)
	{
		auto it = m_objectsById.find(guid);
		if (it == m_objectsById.end())
		{
			return nullptr;
		}

		return it->second;
	}

	void WorldInstance::onObjectMoved(GameObject &object, const math::Vector3 &oldPosition, float oldO)
	{
		// Calculate old tile index
		TileIndex2D oldIndex;
		m_visibilityGrid->getTilePosition(oldPosition, oldIndex[0], oldIndex[1]);

		// Calculate new tile index
		TileIndex2D newIndex = getObjectTile(object, *m_visibilityGrid);

		// Check if tile changed
		if (oldIndex != newIndex)
		{
			// Get the tiles
			VisibilityTile *oldTile = m_visibilityGrid->getTile(oldIndex);
			assert(oldTile);
			VisibilityTile *newTile = m_visibilityGrid->getTile(newIndex);
			assert(newTile);
            
			// Remove the object
			oldTile->getGameObjects().remove(&object);

			// Send despawn packets
			forEachTileInSightWithout(
				*m_visibilityGrid,
				oldTile->getPosition(),
				newTile->getPosition(),
				[&object](VisibilityTile &tile)
			{
				UInt64 guid = object.getGuid();

				std::vector<char> buffer;
				io::VectorSink sink(buffer);
				game::Protocol::OutgoingPacket packet(sink);
				game::server_write::destroyObject(packet, guid, false);

				// Despawn this object for all subscribers
				for (auto * subscriber : tile.getWatchers().getElements())
				{
					auto *character = subscriber->getControlledObject();
					if (!character)
						continue;
					
					if (!object.canSpawnForCharacter(*character))
						continue;

					// Don't despawn invisible objects that we can't see
					if (object.isCreature() || object.isGameCharacter())
					{
						GameUnit &unit = reinterpret_cast<GameUnit&>(object);
						if (unit.isStealthed() && !character->canDetectStealth(unit))
						{
							continue;
						}
					}

					// This is the subscribers own character - despawn all old objects and skip him
					if (character->getGuid() == guid)
					{
						continue;
					}

					subscriber->sendPacket(packet, buffer);
				}
			});

			// Notify watchers about the pending tile change
			object.tileChangePending(*oldTile, *newTile);

			// Send spawn packets
			forEachTileInSightWithout(
				*m_visibilityGrid,
				newTile->getPosition(),
				oldTile->getPosition(),
				[&object](VisibilityTile &tile)
			{
				// Spawn this new object for all watchers of the new tile
				for (auto * subscriber : tile.getWatchers().getElements())
				{
					auto *character = subscriber->getControlledObject();
					if (!character)
						continue;

					if (!object.canSpawnForCharacter(*character))
						continue;

					// Dont spawn stealthed targets that we can't see yet
					if (object.isCreature() || object.isGameCharacter())
					{
						GameUnit &unit = reinterpret_cast<GameUnit&>(object);
						if (unit.isStealthed() && !character->canDetectStealth(unit))
						{
							continue;
						}
					}

					// This is the subscribers own character - send all new objects to this subscriber
					// and then skip him
					if (character->getGuid() == object.getGuid())
					{
						continue;
					}

					// Create spawn message blocks
					std::vector<std::vector<char>> spawnBlocks;
					createUpdateBlocks(object, *character, spawnBlocks);

					std::vector<char> buffer;
					io::VectorSink sink(buffer);
					game::Protocol::OutgoingPacket packet(sink);
					game::server_write::compressedUpdateObject(packet, spawnBlocks);

					subscriber->sendPacket(packet, buffer);

					// Send movement packets
					if (object.isCreature() || object.isGameCharacter())
					{
						reinterpret_cast<GameUnit&>(object).getMover().sendMovementPackets(*subscriber);
					}
				}
			});

			// Add the object
			newTile->getGameObjects().add(&object);
		}
	}

	void WorldInstance::updateObject(GameObject &object)
	{
		// Send updates to all subscribers in sight
		TileIndex2D center = getObjectTile(object, *m_visibilityGrid);
		forEachSubscriberInSight(
			*m_visibilityGrid,
			center,
			[&object](ITileSubscriber & subscriber)
		{
			auto *character = subscriber.getControlledObject();
			if (!character) {
				return;
			}

			// Create update blocks
			std::vector<std::vector<char>> blocks;
			createValueUpdateBlock(object, *character, blocks);

			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);

			if (!blocks.empty() && blocks[0].size() > 50)
			{
				game::server_write::compressedUpdateObject(packet, blocks);
			}
			else if (!blocks.empty())
			{
				game::server_write::updateObject(packet, blocks);
			}

			subscriber.sendPacket(packet, buffer);
		});

		// We updated the object
		object.clearUpdateMask();
	}

	WorldObjectSpawner *WorldInstance::findObjectSpawner(const String &name)
	{
		auto it = m_objectSpawnsByName.find(name);
		if (it == m_objectSpawnsByName.end())
		{
			return nullptr;
		}

		return it->second;
	}

	void WorldInstance::addUpdateObject(GameObject & object)
	{
		m_objectUpdates.insert(&object);
	}

	void WorldInstance::removeUpdateObject(GameObject & object)
	{
		m_objectUpdates.erase(&object);
	}

	CreatureSpawner *WorldInstance::findCreatureSpawner(const String &name)
	{
		auto it = m_creatureSpawnsByName.find(name);
		if (it == m_creatureSpawnsByName.end())
		{
			return nullptr;
		}

		return it->second;
	}

}
