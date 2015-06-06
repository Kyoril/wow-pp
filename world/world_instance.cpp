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
#include "player_manager.h"
#include "player.h"
#include "realm_connector.h"
#include "log/default_log_levels.h"
#include "data/unit_entry.h"
#include "game/game_unit.h"
#include "creature_spawner.h"
#include "tile_visibility_change.h"
#include "visibility_tile.h"
#include "each_tile_in_region.h"
#include "binary_io/vector_sink.h"
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/bind/bind.hpp>
#include <algorithm>
#include <array>
#include <memory>
#include <cassert>

namespace wowpp
{
	// Helper functions. TODO: Put these in headers in case they are needed elsewhere
	namespace
	{
		TileIndex2D getObjectTile(GameObject &object, VisibilityGrid &grid)
		{
			float x, y, z, o;
			object.getLocation(x, y, z, o);

			TileIndex2D gridIndex;
			grid.getTilePosition(x, y, z, gridIndex[0], gridIndex[1]);

			return gridIndex;
		}

		void createUpdateBlocks(GameObject &object, std::vector<std::vector<char>> &out_blocks)
		{
			float x, y, z, o;
			object.getLocation(x, y, z, o);

			// Write create object packet
			std::vector<char> createBlock;
			io::VectorSink sink(createBlock);
			io::Writer writer(sink);
			{
				UInt8 updateType = 0x02;						// Update type (0x02 = CREATE_OBJECT)
				if (object.getTypeId() == object_type::Character ||
					object.getTypeId() == object_type::Corpse ||
					object.getTypeId() == object_type::DynamicObject ||
					object.getTypeId() == object_type::Container)
				{
					updateType = 0x03;		// CREATE_OBJECT_2
				}
				UInt8 updateFlags = 0x10 | 0x20 | 0x40;			// UPDATEFLAG_ALL | UPDATEFLAG_LIVING | UPDATEFLAG_HAS_POSITION
				UInt8 objectTypeId = object.getTypeId();		// 

				// Header with object guid and type
				UInt64 guid = object.getGuid();
				writer
					<< io::write<NetUInt8>(updateType)
					<< io::write<NetUInt8>(0xFF) << io::write<NetUInt64>(guid)
					<< io::write<NetUInt8>(objectTypeId);

				writer
					<< io::write<NetUInt8>(updateFlags);

				// Write movement update
				{
					UInt32 moveFlags = 0x00;
					writer
						<< io::write<NetUInt32>(moveFlags)
						<< io::write<NetUInt8>(0x00)
						<< io::write<NetUInt32>(getCurrentTime());

					// Position & Rotation
					writer
						<< io::write<float>(x)
						<< io::write<float>(y)
						<< io::write<float>(z)
						<< io::write<float>(o);

					// Fall time
					writer
						<< io::write<NetUInt32>(0);

					// Speeds
					writer
						<< io::write<float>(2.5f)				// Walk
						<< io::write<float>(7.0f)				// Run
						<< io::write<float>(4.5f)				// Backwards
						<< io::write<NetUInt32>(0x40971c71)		// Swim
						<< io::write<NetUInt32>(0x40200000)		// Swim Backwards
						<< io::write<float>(7.0f)				// Fly
						<< io::write<float>(4.5f)				// Fly Backwards
						<< io::write<float>(3.1415927);			// Turn (radians / sec: PI)
				}

				// Lower-GUID update?
				if (updateFlags & 0x08)
				{
					writer
						<< io::write<NetUInt32>(guidLowerPart(guid));
				}

				// High-GUID update?
				if (updateFlags & 0x10)
				{
					switch (objectTypeId)
					{
						case object_type::Object:
						case object_type::Item:
						case object_type::Container:
						case object_type::GameObject:
						case object_type::DynamicObject:
						case object_type::Corpse:
							writer
								<< io::write<NetUInt32>(guidHiPart(guid));
							break;
						default:
							writer
								<< io::write<NetUInt32>(0);
					}
				}

				// Write values update
				object.writeValueUpdateBlock(writer, true);
			}

			// Add block
			out_blocks.push_back(createBlock);
		}

		void createValueUpdateBlock(GameObject &object, std::vector<std::vector<char>> &out_blocks)
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
					<< io::write<NetUInt8>(updateType)
					<< io::write<NetUInt8>(0xFF) << io::write<NetUInt64>(guid);

				// Write values update
				object.writeValueUpdateBlock(writer, false);
			}

			// Add block
			out_blocks.push_back(createBlock);
		}

		template <class T, class OnTile, class OnSubscriber>
		static void forEachTileAndEachSubscriber(
			T tilesBegin,
			T tilesEnd,
			const OnTile &onTile,
			const OnSubscriber &onSubscriber)
		{
			for (T t = tilesBegin; t != tilesEnd; ++t)
			{
				auto &tile = *t;
				onTile(tile);

				for (Player * const subscriber : tile.getWatchers().getElements())
				{
					onSubscriber(*subscriber);
				}
			}
		}

		static inline std::vector<VisibilityTile *> getTilesInSight(
			VisibilityGrid &grid,
			const TileIndex2D &center)
		{
			std::vector<VisibilityTile *> tiles;

			for (TileIndex y = center.y() - constants::PlayerZoneSight;
				y <= static_cast<TileIndex>(center.y() + constants::PlayerZoneSight); ++y)
			{
				for (TileIndex x = center.x() - constants::PlayerZoneSight;
					x <= static_cast<TileIndex>(center.x() + constants::PlayerZoneSight); ++x)
				{
					auto *const tile = grid.getTile(TileIndex2D(x, y));

					if (tile)
					{
						tiles.push_back(tile);
					}
				}
			}

			return tiles;
		}

		template <class T, class OnSubscriber>
		static void forEachSubscriber(
			T tilesBegin,
			T tilesEnd,
			const OnSubscriber &onSubscriber)
		{
			forEachTileAndEachSubscriber(
				tilesBegin,
				tilesEnd,
				[](VisibilityTile &) {},
				onSubscriber);
		}

		template <class OnSubscriber>
		void forEachSubscriberInSight(
			VisibilityGrid &grid,
			const TileIndex2D &center,
			const OnSubscriber &onSubscriber)
		{
			const auto tiles = getTilesInSight(grid, center);
			forEachSubscriber(
				boost::make_indirect_iterator(tiles.begin()),
				boost::make_indirect_iterator(tiles.end()),
				onSubscriber);
		}

		void doFullSightVisibilityChange(
			GameObject &object,
			VisibilityTile &tile,
			TileVisibility visibility,
			VisibilityGrid &grid
			)
		{
			std::array<VisibilityTile *, constants::PlayerScopeAreaCount> changedTiles;
			auto changedTilesEnd = changedTiles.begin();
			copyTilePtrsInArea(
				grid,
				getSightArea(tile.getPosition()),
				/*ref*/ changedTilesEnd
				);

			TileVisibilityChange change;
			change.changed[visibility] =
				TileVisibilityChange::TileRange(
				&changedTiles[0],
				&changedTiles[0] + std::distance(changedTiles.begin(), changedTilesEnd));

			if (isPlayerGUID(object.getGuid()))
			{
				size_t objCount = 0;
				size_t tileCount = 0;
				for (auto &it : change.changed[visibility])
				{
					tileCount++;
					objCount += it->getGameObjects().size();
				}

				// Display object count
				DLOG("Spawning " << objCount << " objects from " << tileCount << " tiles");
			}
			
			//entity.tileSightChanged(change);
		}

		static inline bool isInSight(
			const TileIndex2D &first,
			const TileIndex2D &second)
		{
			const auto diff = abs(second - first);
			return
				(static_cast<size_t>(diff.x()) <= constants::PlayerZoneSight) &&
				(static_cast<size_t>(diff.y()) <= constants::PlayerZoneSight);
		}

		template <class OnTile>
		void forEachTileInSightWithout(
			VisibilityGrid &grid,
			const TileIndex2D &center,
			const TileIndex2D &excluded,
			const OnTile &onTile
			)
		{
			for (TileIndex y = center.y() - constants::PlayerZoneSight;
				y <= static_cast<TileIndex>(center.y() + constants::PlayerZoneSight); ++y)
			{
				for (TileIndex x = center.x() - constants::PlayerZoneSight;
					x <= static_cast<TileIndex>(center.x() + constants::PlayerZoneSight); ++x)
				{
					auto *const tile = grid.getTile(TileIndex2D(x, y));

					if (tile &&
						!isInSight(excluded, tile->getPosition()))
					{
						onTile(*tile);
					}
				}
			}
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
		spawned->setUInt32Value(object_fields::Entry, entry.id);
		spawned->setFloatValue(object_fields::ScaleX, entry.scale);
		spawned->setUInt32Value(unit_fields::FactionTemplate, entry.hordeFactionID);			//TODO
		spawned->setGender(game::gender::Male);
		//spawned->setUInt32Value(unit_fields::Bytes0, 0x00020000);								//TODO
		spawned->setUInt32Value(unit_fields::DisplayId, entry.maleModel);						//TODO
		spawned->setUInt32Value(unit_fields::NativeDisplayId, entry.maleModel);					//TODO
		spawned->setUInt32Value(unit_fields::BaseHealth, 20);									//TODO
		spawned->setUInt32Value(unit_fields::Health, entry.minLevelHealth);
		spawned->setUInt32Value(unit_fields::MaxHealth, entry.minLevelHealth);
		//spawned->setUInt32Value(unit_fields::Bytes2, 0x00001001);								//TODO
		spawned->setUInt32Value(unit_fields::UnitFlags, entry.unitFlags);
		//spawned->setUInt32Value(unit_fields::UnitFlags2, entry.extraFlags);


		spawned->setByteValue(unit_fields::Bytes2, 1, 16);

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
				//TODO send updates to all subscribers in sight
				TileIndex2D center = getObjectTile(*object, *m_visibilityGrid);
				forEachSubscriberInSight(
					*m_visibilityGrid,
					center,
					[&object](Player &subscriber)
				{
					// Create update blocks
					std::vector<std::vector<char>> blocks;
					createValueUpdateBlock(*object, blocks);

					if (!blocks.empty() && blocks[0].size() > 100)
					{
						// Send an update
						subscriber.sendProxyPacket(
							std::bind(game::server_write::compressedUpdateObject, std::placeholders::_1, std::cref(blocks)));
					}
					else
					{
						// Send an update
						subscriber.sendProxyPacket(
							std::bind(game::server_write::updateObject, std::placeholders::_1, std::cref(blocks)));
					}
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

		Player *player = nullptr;
		std::array<VisibilityTile *, constants::PlayerScopeAreaCount> changedTiles;

		// Get grid tile
		auto &tile = m_visibilityGrid->requireTile(gridIndex);

		if (isPlayerGUID(guid))
		{
			player = m_manager.getPlayerManager().getPlayerByCharacterGuid(guid);
			if (!player)
			{
				//
				ELOG("Couldn't find player instance!");
				return;
			}

			// Spawn nearby objects
			auto changedTilesEnd = changedTiles.begin();
			copyTilePtrsInArea(
				*m_visibilityGrid,
				getSightArea(tile.getPosition()),
				/*ref*/ changedTilesEnd
				);

			// Spawn objects
			for (auto &changed : changedTiles)
			{
				for (auto it = changed->getGameObjects().begin(); it != changed->getGameObjects().end(); ++it)
				{
					auto &object = *it;

					// Create update block
					std::vector<std::vector<char>> blocks;
					createUpdateBlocks(*object, blocks);
					
					// Send it
					player->sendProxyPacket(
						std::bind(game::server_write::compressedUpdateObject, std::placeholders::_1, std::cref(blocks)));
				}
			}
		}

		// Add to the tile
		tile.getGameObjects().add(&added);
		
		// Create update packet
		std::vector<std::vector<char>> blocks;
		createUpdateBlocks(added, blocks);

		// Notify all watchers about the new object

		// Spawn ourself for new watchers
		forEachTileInSight(
			*m_visibilityGrid,
			tile.getPosition(),
			[&blocks](VisibilityTile &tile)
		{
			for (const auto * subscriber : tile.getWatchers().getElements())
			{
				subscriber->sendProxyPacket(
					std::bind(game::server_write::compressedUpdateObject, std::placeholders::_1, std::cref(blocks)));
			}
		});

		// Make us a watcher for these tiles so that we will be notified about new spawns
		if (player)
		{
			tile.getWatchers().add(player);
		}

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

		// Remove game object from the object list
		m_objectsById.erase(guid);

		if (isPlayerGUID(guid))
		{
			Player *player = m_manager.getPlayerManager().getPlayerByCharacterGuid(guid);
			if (!player)
			{
				//
				ELOG("Couldn't find player instance!");
				return;
			}
			
			tile.getWatchers().remove(player);
		}

		// Despawn ourself for new watchers
		forEachTileInSight(
			*m_visibilityGrid,
			tile.getPosition(),
			[&remove](VisibilityTile &tile)
		{
			for (const auto * subscriber : tile.getWatchers().getElements())
			{
				subscriber->sendProxyPacket(
					std::bind(game::server_write::destroyObject, std::placeholders::_1, remove.getGuid(), false));
			}
		});
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

			Player * player = nullptr;

			auto guid = object.getGuid();
			if (isPlayerGUID(guid))
			{
				player = m_manager.getPlayerManager().getPlayerByCharacterGuid(guid);
			}

			if (player)
			{
				// Unsubscribe the old tile
				oldTile->getWatchers().remove(player);

				// Create spawn message blocks
				std::vector<std::vector<char>> spawnBlocks;
				createUpdateBlocks(object, spawnBlocks);

				// Spawn ourself for new watchers
				forEachTileInSightWithout(
					*m_visibilityGrid,
					newIndex,
					oldIndex,
					[&spawnBlocks, &player](VisibilityTile &tile)
				{
					for(const auto * subscriber : tile.getWatchers().getElements())
					{
						if (subscriber != player)
						{
							subscriber->sendProxyPacket(
								std::bind(game::server_write::compressedUpdateObject, std::placeholders::_1, std::cref(spawnBlocks)));
						}
					}

					// Spawn objects of this tile for the player
					for (auto *object : tile.getGameObjects().getElements())
					{
						std::vector<std::vector<char>> createBlock;
						createUpdateBlocks(*object, createBlock);

						player->sendProxyPacket(
							std::bind(game::server_write::compressedUpdateObject, std::placeholders::_1, std::cref(createBlock)));
					}
				});

				// Despawn ourself for old watchers
				forEachTileInSightWithout(
					*m_visibilityGrid,
					oldIndex,
					newIndex,
					[guid, &player](VisibilityTile &tile)
				{
					for (const auto * subscriber : tile.getWatchers().getElements())
					{
						if (subscriber != player)
						{
							subscriber->sendProxyPacket(
								std::bind(game::server_write::destroyObject, std::placeholders::_1, guid, false));
						}
					}

					// Despawn old objects for the player
					for (auto *object : tile.getGameObjects().getElements())
					{
						player->sendProxyPacket(
							std::bind(game::server_write::destroyObject, std::placeholders::_1, object->getGuid(), false));
					}
				});

				// Subscribe the new tile
				newTile->getWatchers().add(player);
			}

			// Add the object
			newTile->getGameObjects().add(&object);
		}
	}

}
