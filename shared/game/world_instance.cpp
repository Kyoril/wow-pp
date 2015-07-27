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
#include "game_unit.h"
#include "game_creature.h"
#include "creature_spawner.h"
#include "tile_visibility_change.h"
#include "visibility_tile.h"
#include "each_tile_in_region.h"
#include "binary_io/vector_sink.h"
#include <boost/bind/bind.hpp>
#include "each_tile_in_sight.h"
#include "common/utilities.h"
#include <algorithm>
#include <array>
#include <memory>
#include <cassert>

namespace wowpp
{
	namespace
	{
		static TileIndex2D getObjectTile(GameObject &object, VisibilityGrid &grid)
		{
			float x, y, z, o;
			object.getLocation(x, y, z, o);

			TileIndex2D gridIndex;
			grid.getTilePosition(x, y, z, gridIndex[0], gridIndex[1]);

			return gridIndex;
		}

		static void createValueUpdateBlock(GameObject &object, std::vector<std::vector<char>> &out_blocks)
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
				writer.sink().write((const char*)&packGUID[0], size);

				// Write values update
				object.writeValueUpdateBlock(writer, false);
			}

			// Add block
			out_blocks.push_back(createBlock);
		}
	}

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
			std::unique_ptr<CreatureSpawner> spawner(new CreatureSpawner(
				*this,
				*spawn.unit,
				spawn.maxCount,
				spawn.respawnDelay,
				spawn.position[0], spawn.position[1], spawn.position[2],
				spawn.rotation,
				spawn.radius));
			m_creatureSpawners.push_back(std::move(spawner));
		}

		ILOG("Created instance of map " << m_mapEntry.id);
	}

	std::shared_ptr<GameCreature> WorldInstance::spawnCreature(
		const UnitEntry &entry,
		float x, float y, float z, float o,
		float randomWalkRadius)
	{
		// Create the unit
		auto spawned = std::make_shared<GameCreature>(
			m_manager.getTimerQueue(),
			m_getRace,
			m_getClass,
			m_getLevel,
			entry);
		spawned->initialize();
		spawned->setGuid(createEntryGUID(m_objectIdGenerator.generateId(), entry.id, guid_type::Unit));	// RealmID (TODO: these spawns don't need to have a specific realm id)
		spawned->setMapId(m_mapEntry.id);
		spawned->relocate(x, y, z, o);

		return spawned;
	}

	void WorldInstance::update()
	{
		// Iterate all game objects added to this world which need to be updated
		for (auto &gameObject : m_objectsById)
		{
			// Update values changed...
			auto *object = gameObject.second;
			if (object->wasUpdated())
			{
				// Send updates to all subscribers in sight
				TileIndex2D center = getObjectTile(*object, *m_visibilityGrid);
				forEachSubscriberInSight(
					*m_visibilityGrid,
					center,
					[&object](ITileSubscriber &subscriber)
				{
					// Create update blocks
					std::vector<std::vector<char>> blocks;
					createValueUpdateBlock(*object, blocks);

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
				object->clearUpdateMask();
			}
		}
	}

	void WorldInstance::addGameObject(GameObject& added)
	{
		auto guid = added.getGuid();

		// Add this game object to the list of objects
		m_objectsById.insert(
			std::make_pair(guid, &added));

		// Get object location
		float x, y, z, o;
		added.getLocation(x, y, z, o);
		
		// Transform into grid location
		TileIndex2D gridIndex;
		if (!m_visibilityGrid->getTilePosition(x, y, z, gridIndex[0], gridIndex[1]))
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

		// Create update packet
		std::vector<std::vector<char>> blocks;
		createUpdateBlocks(added, blocks);

		// Create the packet
		std::vector<char> buffer;
		io::VectorSink sink(buffer);
		game::Protocol::OutgoingPacket packet(sink);
		game::server_write::compressedUpdateObject(packet, blocks);

		// Spawn ourself for new watchers
		forEachTileInSight(
			*m_visibilityGrid,
			tile.getPosition(),
			[&packet, &buffer](VisibilityTile &tile)
		{
			for (auto * subscriber : tile.getWatchers().getElements())
			{
				subscriber->sendPacket(packet, buffer);
			}
		});

		// Notify about being spawned
		added.spawned();

		// Watch for object location changes
		added.moved.connect(
			boost::bind(&WorldInstance::onObjectMoved, this, _1, _2, _3, _4, _5));
	}

	void WorldInstance::removeGameObject(GameObject &remove)
	{
		auto guid = remove.getGuid();

		// Transform into grid location
		TileIndex2D gridIndex = getObjectTile(remove, *m_visibilityGrid);

		// Get grid tile
		auto &tile = m_visibilityGrid->requireTile(gridIndex);
		tile.getGameObjects().remove(&remove);

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
			[&packet, &buffer](VisibilityTile &tile)
		{
			for (auto * subscriber : tile.getWatchers().getElements())
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

	GameObject * WorldInstance::findObjectByGUID(UInt64 guid)
	{
		auto it = m_objectsById.find(guid);
		if (it == m_objectsById.end())
		{
			return nullptr;
		}

		return it->second;
	}

	void WorldInstance::onObjectMoved(GameObject &object, float oldX, float oldY, float oldZ, float oldO)
	{
		// Calculate old tile index
		TileIndex2D oldIndex;
		m_visibilityGrid->getTilePosition(oldX, oldY, oldZ, oldIndex[0], oldIndex[1]);

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

			// Notify watchers about the pending tile change
			object.tileChangePending(*oldTile, *newTile);

			// Add the object
			newTile->getGameObjects().add(&object);
		}
	}
}
