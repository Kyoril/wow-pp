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

#include "player.h"
#include "player_manager.h"
#include "log/default_log_levels.h"
#include "game/world_instance_manager.h"
#include "game/world_instance.h"
#include "game/each_tile_in_sight.h"
#include "game/universe.h"
#include "data/project.h"
#include "data/unit_entry.h"
#include "game/game_creature.h"
#include <iomanip>
#include <cassert>
#include <limits>

using namespace std;

namespace wowpp
{
	Player::Player(PlayerManager &manager, RealmConnector &realmConnector, WorldInstanceManager &worldInstanceManager, DatabaseId characterId, std::shared_ptr<GameCharacter> character, WorldInstance &instance, Project &project)
		: m_manager(manager)
		, m_realmConnector(realmConnector)
		, m_worldInstanceManager(worldInstanceManager)
		, m_characterId(characterId)
		, m_character(std::move(character))
		, m_logoutCountdown(instance.getUniverse().getTimers())
		, m_instance(instance)
		, m_lastError(attack_swing_error::Unknown)
		, m_lastFallTime(0)
		, m_lastFallZ(0.0f)
		, m_project(project)
		, m_loot(nullptr)
		, m_clientDelayMs(0)
		, m_nextDelayReset(0)
		, m_clientTicks(0)
		, m_groupUpdate(instance.getUniverse().getTimers())
	{
		m_logoutCountdown.ended.connect(
			std::bind(&Player::onLogout, this));
		m_onSpawn = m_character->spawned.connect(
			std::bind(&Player::onSpawn, this));
		m_onDespawn = m_character->despawned.connect(
			std::bind(&Player::onDespawn, this));
		m_onTileChange = m_character->tileChangePending.connect(
			std::bind(&Player::onTileChangePending, this, std::placeholders::_1, std::placeholders::_2));
		m_onProfChanged = m_character->proficiencyChanged.connect(
			std::bind(&Player::onProficiencyChanged, this, std::placeholders::_1, std::placeholders::_2));
		m_onAtkSwingErr = m_character->autoAttackError.connect(
			std::bind(&Player::onAttackSwingError, this, std::placeholders::_1));
		m_onInvFailure = m_character->inventoryChangeFailure.connect(
			std::bind(&Player::onInventoryChangeFailure, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		m_onComboPoints = m_character->comboPointsChanged.connect(
			std::bind(&Player::onComboPointsChanged, this));
		m_onXP = m_character->experienceGained.connect(
			std::bind(&Player::onExperienceGained, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		m_onCastError = m_character->spellCastError.connect(
			std::bind(&Player::onSpellCastError, this, std::placeholders::_1, std::placeholders::_2));
		m_onGainLevel = m_character->levelGained.connect(
			std::bind(&Player::onLevelGained, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, 
													std::placeholders::_5, std::placeholders::_6, std::placeholders::_7, std::placeholders::_8));
		m_onAuraUpdate = m_character->auraUpdated.connect(
			std::bind(&Player::onAuraUpdated, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
		m_onTargetAuraUpdate = m_character->targetAuraUpdated.connect(
			std::bind(&Player::onTargetAuraUpdated, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
		m_onTeleport = m_character->teleport.connect(
			std::bind(&Player::onTeleport, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));

		m_groupUpdate.ended.connect([this]()
		{
			float x, y, z, o;
			m_character->getLocation(x, y, z, o);

			// Get group id
			auto groupId = m_character->getGroupId();
			std::vector<UInt64> nearbyMembers;

			// Determine nearby party members
			m_instance.getUnitFinder().findUnits(Circle(x, y, 100.0f), [this, groupId, &nearbyMembers](GameUnit &unit) -> bool
			{
				// Only characters
				if (unit.getTypeId() != object_type::Character)
				{
					return true;
				}

				// Check characters group
				GameCharacter *character = static_cast<GameCharacter*>(&unit);
				if (character->getGroupId() == groupId &&
					character->getGuid() != getCharacterGuid())
				{
					nearbyMembers.push_back(character->getGuid());
				}

				return true;
			});

			// Send packet to world node
			m_realmConnector.sendCharacterGroupUpdate(*m_character, nearbyMembers);
			m_groupUpdate.setEnd(getCurrentTime() + constants::OneSecond * 3);
		});

		// Trigger regeneration for our character
		m_character->startRegeneration();
	}

	void Player::logoutRequest()
	{
		// Make our character sit down
		auto standState = unit_stand_state::Sit;
		m_character->setByteValue(unit_fields::Bytes1, 0, standState);
		sendProxyPacket(
			std::bind(game::server_write::standStateUpdate, std::placeholders::_1, standState));

		// Root our character
		auto flags = m_character->getUInt32Value(unit_fields::UnitFlags);
		m_character->setUInt32Value(unit_fields::UnitFlags, flags | 0x00040000);
		sendProxyPacket(
			std::bind(game::server_write::forceMoveRoot, std::placeholders::_1, m_character->getGuid(), 2));

		// Setup the logout countdown
		m_logoutCountdown.setEnd(
			getCurrentTime() + (20 * constants::OneSecond));
	}

	void Player::cancelLogoutRequest()
	{
		// Unroot
		auto flags = m_character->getUInt32Value(unit_fields::UnitFlags);
		m_character->setUInt32Value(unit_fields::UnitFlags, flags & ~0x00040000);
		sendProxyPacket(
			std::bind(game::server_write::forceMoveUnroot, std::placeholders::_1, m_character->getGuid(), 0));

		// Stand up again
		auto standState = unit_stand_state::Stand;
		m_character->setByteValue(unit_fields::Bytes1, 0, standState);
		sendProxyPacket(
			std::bind(game::server_write::standStateUpdate, std::placeholders::_1, standState));

		// Cancel the countdown
		m_logoutCountdown.cancel();
	}

	void Player::onLogout()
	{
		// Remove the character from the world
		m_instance.removeGameObject(*m_character);
		m_character.reset();

		// Notify the realm
		m_realmConnector.notifyWorldInstanceLeft(m_characterId, pp::world_realm::world_left_reason::Logout);
		
		// Remove player
		m_manager.playerDisconnected(*this);
	}
	
	TileIndex2D Player::getTileIndex() const
	{
		TileIndex2D tile;
		
		float x, y, z, o;
		m_character->getLocation(x, y, z, o);

		// Get a list of potential watchers
		auto &grid = m_instance.getGrid();
		grid.getTilePosition(x, y, z, tile[0], tile[1]);
		
		return tile;
	}

	void Player::chatMessage(game::ChatMsg type, game::Language lang, const String &receiver, const String &channel, const String &message)
	{
		if (!m_character)
		{
			WLOG("No character assigned!");
			return;
		}

		// Check if this is a whisper message
		if (type == game::chat_msg::Whisper)
		{
			// Get realm name and receiver name
			std::vector<String> names;
			split(receiver, '-', names);

			// There has to be a realm name since we are a world node and only receive whisper messages in case of cross
			// realm whispers
			if (names.size() != 2)
			{
				WLOG("Cross realm whisper: No realm name received!");
				return;
			}

			for (auto &str : names)
			{
				capitalize(str);
			}

			// Change language if needed so that whispers are always readable
			if (lang != game::language::Addon) lang = game::language::Universal;

			// Build full name
			std::ostringstream strm;
			strm << names[0] << "-" << names[1];
			String fullName = strm.str();

			// Iterate through the players (TODO: Make this more performant)
			for (const auto &player : m_manager.getPlayers())
			{
				if (player->getRealmName() == names[1] &&
					player->getCharacter()->getName() == names[0])
				{
					// Check faction
					const bool isAllianceA = ((game::race::Alliance & (1 << (player->getCharacter()->getRace() - 1))) == (1 << (player->getCharacter()->getRace() - 1)));
					const bool isAllianceB = ((game::race::Alliance & (1 << (m_character->getRace() - 1))) == (1 << (m_character->getRace() - 1)));
					if (isAllianceA != isAllianceB)
					{
						sendProxyPacket(
							std::bind(game::server_write::chatWrongFaction, std::placeholders::_1));
						return;
					}

					if (player->isIgnored(getCharacterGuid()))
					{
						WLOG("TODO: Target player ignores us.");
						return;
					}

					// Send whisper message
					player->sendProxyPacket(
						std::bind(game::server_write::messageChat, std::placeholders::_1, game::chat_msg::Whisper, lang, std::cref(channel), m_characterId, std::cref(message), m_character.get()));

					// If not an addon message, send reply message
					if (lang != game::language::Addon)
					{
						sendProxyPacket(
							std::bind(game::server_write::messageChat, std::placeholders::_1, game::chat_msg::Reply, lang, std::cref(channel), player->getCharacterGuid(), std::cref(message), m_character.get()));
					}
					return;
				}
			}

			// Could not find any player using that name
			sendProxyPacket(
				std::bind(game::server_write::chatPlayerNotFound, std::placeholders::_1, std::cref(fullName)));
		}
		else
		{
			float x, y, z, o;
			m_character->getLocation(x, y, z, o);

			// Get a list of potential watchers
			auto &grid = m_instance.getGrid();

			// Get tile index
			TileIndex2D tile;
			grid.getTilePosition(x, y, z, tile[0], tile[1]);

			// Get all potential characters
			std::vector<ITileSubscriber*> subscribers;
			forEachTileInSight(
				grid,
				tile,
				[this, &subscribers](VisibilityTile &tile)
			{
				for (auto * const subscriber : tile.getWatchers().getElements())
				{
					if(!isIgnored(subscriber->getControlledObject()->getGuid())) subscribers.push_back(subscriber);
				}
			});

			if (type != game::chat_msg::Say &&
				type != game::chat_msg::Yell &&
				type != game::chat_msg::Emote &&
				type != game::chat_msg::TextEmote)
			{
				WLOG("Unsupported chat mode");
				return;
			}

			if (!m_character->isAlive())
			{
				return;
			}

			// Create the chat packet
			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);
			game::server_write::messageChat(packet, type, lang, channel, m_character->getGuid(), message, m_character.get());

			const float chatRange = (type == game::chat_msg::Yell || type == game::chat_msg::Emote || type == game::chat_msg::TextEmote) ? 300.0f : 25.0f;
			for (auto * const subscriber : subscribers)
			{
				const float distance = m_character->getDistanceTo(*subscriber->getControlledObject());
				if (distance <= chatRange)
				{
					subscriber->sendPacket(packet, buffer);
				}
			}
		}
	}

	void Player::sendPacket(game::Protocol::OutgoingPacket &packet, const std::vector<char> &buffer)
	{
		// Send the proxy packet to the realm server
		m_realmConnector.sendProxyPacket(m_characterId, packet.getOpCode(), packet.getSize(), buffer);
	}

	void Player::onSpawn()
	{
		// Send self spawn packet
		sendProxyPacket(
			std::bind(game::server_write::initWorldStates, std::placeholders::_1, m_character->getMapId(), m_character->getZone()));
		sendProxyPacket(
			std::bind(game::server_write::loginSetTimeSpeed, std::placeholders::_1, 0));

		// Blocks
		std::vector<std::vector<char>> blocks;

		// Create item spawn packets
		for (auto &item : m_character->getItems())
		{
			const UInt16 &slot = item.first;
			const auto &instance = item.second;

			// Check if we need to send that item
			const bool sendItemToPlayer = (
				slot < player_bank_bag_slots::End ||
				(slot >= player_key_ring_slots::Start && slot < player_key_ring_slots::End)
				);
			if (sendItemToPlayer)
			{
				// Spawn this item
				std::vector<char> createItemBlock;
				io::VectorSink createItemSink(createItemBlock);
				io::Writer createItemWriter(createItemSink);
				{
					UInt8 updateType = 0x02;						// Item
					UInt8 updateFlags = 0x08 | 0x10;				// 
					UInt8 objectTypeId = 0x01;						// Item

					UInt64 guid = instance->getGuid();

					// Header with object guid and type
					createItemWriter
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
					createItemWriter.sink().write((const char*)&packGUID[0], size);
					createItemWriter
						<< io::write<NetUInt8>(objectTypeId)
						<< io::write<NetUInt8>(updateFlags);

					// Lower-GUID update?
					if (updateFlags & 0x08)
					{
						createItemWriter
							<< io::write<NetUInt32>(guidLowerPart(guid));
					}

					// High-GUID update?
					if (updateFlags & 0x10)
					{
						createItemWriter
							<< io::write<NetUInt32>((guid << 48) & 0x0000FFFF);
					}

					// Write values update
					instance->writeValueUpdateBlock(createItemWriter, *m_character, true);
				}
				blocks.emplace_back(std::move(createItemBlock));
			}
		}

		// Write create object packet
		std::vector<char> createBlock;
		io::VectorSink sink(createBlock);
		io::Writer writer(sink);
		{
			UInt8 updateType = 0x03;						// Player
			UInt8 updateFlags = 0x01 | 0x10 | 0x20 | 0x40;	// UPDATEFLAG_SELF | UPDATEFLAG_ALL | UPDATEFLAG_LIVING | UPDATEFLAG_HAS_POSITION
			UInt8 objectTypeId = 0x04;						// Player

			UInt64 guid = m_character->getGuid();

			// Header with object guid and type
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
			writer
				<< io::write<NetUInt8>(objectTypeId);

			writer
				<< io::write<NetUInt8>(updateFlags);

			// Write movement update
			{
				UInt32 moveFlags = 0x00;
				writer
					<< io::write<NetUInt32>(moveFlags)
					<< io::write<NetUInt8>(0x00)
					<< io::write<NetUInt32>(mTimeStamp());	//TODO: Time

				// Position & Rotation
				float x, y, z, o;
				m_character->getLocation(x, y, z, o);

				writer
					<< io::write<float>(x)
					<< io::write<float>(y)
					<< io::write<float>(z)
					<< io::write<float>(o);

				// Fall time
				writer
					<< io::write<NetUInt32>(0);

				// Speeds8
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
						<< io::write<NetUInt32>((guid << 48) & 0x0000FFFF);
					break;
				default:
					writer
						<< io::write<NetUInt32>(0);
				}
			}

			// Write values update
			m_character->writeValueUpdateBlock(writer, *m_character, true);

			// Add block
			blocks.emplace_back(std::move(createBlock));
		}

		// Send packet
		sendProxyPacket(
			std::bind(game::server_write::compressedUpdateObject, std::placeholders::_1, std::cref(blocks)));

		// Send time sync request packet
		sendProxyPacket(
			std::bind(game::server_write::timeSyncReq, std::placeholders::_1, 0));

		// Find our tile
		TileIndex2D tileIndex = getTileIndex();
		VisibilityTile &tile = m_instance.getGrid().requireTile(tileIndex);
		tile.getWatchers().add(this);
	}

	void Player::onDespawn()
	{
		// Send character data to the realm
		m_realmConnector.sendCharacterData(*m_character);

		// Find our tile
		TileIndex2D tileIndex = getTileIndex();
		VisibilityTile &tile = m_instance.getGrid().requireTile(tileIndex);
		tile.getWatchers().remove(this);
	}

	void Player::onTileChangePending(VisibilityTile &oldTile, VisibilityTile &newTile)
	{
		// We no longer watch for changes on our old tile
		oldTile.getWatchers().remove(this);

		// Convert position to ADT position, and to ADT cell position
		auto newPos = newTile.getPosition();
		auto adtPos = makeVector<TileIndex>(newPos[0] / 16, newPos[1] / 16);
		auto localPos = makeVector<TileIndex>(newPos[0] - (adtPos[0] * 16), newPos[1] - (adtPos[1] * 16));

		// Check zone exploration
		auto *map = m_instance.getMapData();
		if (map)
		{
			// Lets check our Z coordinate
			float x, y, z, o;
			m_character->getLocation(x, y, z, o);
			float terrainHeight = map->getHeightAt(x, y);
			//DLOG("Z Comparison for " << x << " " << y << ": " << z << " (Terrain: " << terrainHeight << ")");

			// Get or load map tile
			auto *tile = map->getTile(adtPos);
			if (tile)
			{
				// Find local tile
				auto &area = tile->areas.cellAreas[localPos[1] + localPos[0] * 16];
				auto *areaZone = m_project.zones.getById(area.areaId);
				if (areaZone)
				{
					// Update players zone field
					auto *topLevelZone = areaZone;
					while (topLevelZone->parent != nullptr)
					{
						topLevelZone = topLevelZone->parent;
					}

					m_character->setZone(topLevelZone->id);

					// Exploration
					UInt32 exploration = areaZone->explore;
					int offset = exploration / 32;

					UInt32 val = (UInt32)(1 << (exploration % 32));
					UInt32 currFields = m_character->getUInt32Value(character_fields::ExploredZones_1 + offset);

					if (!(currFields & val))
					{
						m_character->setUInt32Value(character_fields::ExploredZones_1 + offset, (UInt32)(currFields | val));

						if (m_character->getLevel() >= 70)
						{
							sendProxyPacket(
								std::bind(game::server_write::explorationExperience, std::placeholders::_1, areaZone->id, 0));
						}
						else
						{
							const Int32 diff = static_cast<Int32>(m_character->getLevel()) - static_cast<Int32>(areaZone->level);
							UInt32 xp = 0;

							if (diff < -5)
							{
								const auto *levelEntry = m_project.levels.getById(m_character->getLevel() + 5);
								xp = (levelEntry ? levelEntry->explorationBaseXP : 0);
							}
							else if (diff > 5)
							{
								Int32 xpPct = (100 - ((diff - 5) * 5));
								if (xpPct > 100)
								{
									xpPct = 100;
								}
								else if (xpPct < 0)
								{
									xpPct = 0;
								}

								const auto *levelEntry = m_project.levels.getById(areaZone->level);
								xp = (levelEntry ? levelEntry->explorationBaseXP : 0) * xpPct / 100;
							}
							else
							{
								const auto *levelEntry = m_project.levels.getById(areaZone->level);
								xp = (levelEntry ? levelEntry->explorationBaseXP : 0);
							}

							// Calculate experience points
							if (xp > 0) m_character->rewardExperience(nullptr, xp);
							sendProxyPacket(
								std::bind(game::server_write::explorationExperience, std::placeholders::_1, areaZone->id, xp));
						}
					}
				}
			}
		}

		auto &grid = m_instance.getGrid();

		// Spawn ourself for new watchers
		forEachTileInSightWithout(
			grid,
			newTile.getPosition(),
			oldTile.getPosition(),
			[this](VisibilityTile &tile)
		{
			for(auto * subscriber : tile.getWatchers().getElements())
			{
				auto *character = subscriber->getControlledObject();
				if (!character)
					continue;

				// Create spawn message blocks
				std::vector<std::vector<char>> spawnBlocks;
				createUpdateBlocks(*m_character, *character, spawnBlocks);

				std::vector<char> buffer;
				io::VectorSink sink(buffer);
				game::Protocol::OutgoingPacket packet(sink);
				game::server_write::compressedUpdateObject(packet, spawnBlocks);

				assert(subscriber != this);
				subscriber->sendPacket(packet, buffer);
			}

			for (auto *object : tile.getGameObjects().getElements())
			{
				std::vector<std::vector<char>> createBlock;
				createUpdateBlocks(*object, *m_character, createBlock);

				this->sendProxyPacket(
					std::bind(game::server_write::compressedUpdateObject, std::placeholders::_1, std::cref(createBlock)));
			}
		});

		// Despawn ourself for old watchers
		auto guid = m_character->getGuid();
		forEachTileInSightWithout(
			grid,
			oldTile.getPosition(),
			newTile.getPosition(),
			[guid, this](VisibilityTile &tile)
		{
			// Create the chat packet
			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);
			game::server_write::destroyObject(packet, guid, false);

			for (auto * subscriber : tile.getWatchers().getElements())
			{
				assert(subscriber != this);
				subscriber->sendPacket(packet, buffer);
			}

			for (auto *object : tile.getGameObjects().getElements())
			{
				this->sendProxyPacket(
					std::bind(game::server_write::destroyObject, std::placeholders::_1, object->getGuid(), false));
			}
		});

		// Make us a watcher of the new tile
		newTile.getWatchers().add(this);
	}

	void Player::onProficiencyChanged(Int32 itemClass, UInt32 mask)
	{
		if (itemClass >= 0)
		{
			sendProxyPacket(
				std::bind(game::server_write::setProficiency, std::placeholders::_1, itemClass, mask));
		}
	}

	void Player::onAttackSwingError(AttackSwingError error)
	{
		// Nothing to do here, since the client will automatically repeat the last error message periodically
		if (error == m_lastError)
			return;

		// Save last error
		m_lastError = error;

		switch (error)
		{
			case attack_swing_error::CantAttack:
			{
				sendProxyPacket(
					std::bind(game::server_write::attackSwingCantAttack, std::placeholders::_1));
				break;
			}

			case attack_swing_error::NotStanding:
			{
				sendProxyPacket(
					std::bind(game::server_write::attackSwingNotStanding, std::placeholders::_1));
				break;
			}

			case attack_swing_error::OutOfRange:
			{
				sendProxyPacket(
					std::bind(game::server_write::attackSwingNotInRange, std::placeholders::_1));
				break;
			}

			case attack_swing_error::WrongFacing:
			{
				sendProxyPacket(
					std::bind(game::server_write::attackSwingBadFacing, std::placeholders::_1));
				break;
			}

			case attack_swing_error::TargetDead:
			{
				sendProxyPacket(
					std::bind(game::server_write::attackSwingDeadTarget, std::placeholders::_1));
				break;
			}

			case attack_swing_error::Success:
			{
				// Nothing to do here, since the auto attack was a success. This event is only fired to
				// reset the variable.
				break;
			}

			default:
			{
				WLOG("Unknown attack swing error code: " << error);
				break;
			}
		}
	}

	void Player::handleAutoStoreLootItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 lootSlot;
		if (!game::client_read::autoStoreLootItem(packet, lootSlot))
		{
			WLOG("Could not read packet data");
			return;
		}

		// Check if the player is looting
		if (!m_loot)
		{
			WLOG("Player isn't looting anything");
			return;
		}

		// Try to get loot at slot
		auto *lootItem = m_loot->getLootDefinition(lootSlot);
		if (!lootItem)
		{
			sendProxyPacket(
				std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::SlotIsEmpty, nullptr, nullptr));
			return;
		}

		// Already looted?
		if (lootItem->isLooted)
		{
			sendProxyPacket(
				std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::AlreadyLooted, nullptr, nullptr));
			return;
		}

		// Check if we can store that item
		ItemPosCountVector slots;
		auto result = m_character->canStoreItem(0xFF, 0xFF, slots, *lootItem->definition.item, lootItem->count, false, nullptr);
		if (result != game::inventory_change_failure::Okay)
		{
			sendProxyPacket(
				std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, result, nullptr, nullptr));
			return;
		}

		if (slots.empty())
		{
			sendProxyPacket(
				std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::BagFull, nullptr, nullptr));
			return;
		}

		static UInt64 lootCounter = 0x800000;

		for (auto &pos : slots)
		{
			auto *itemAtSlot= m_character->getItemByPos(0xFF, pos.position);

			// Loot items and add them
			auto inst = std::make_shared<GameItem>(*lootItem->definition.item);
			inst->initialize();
			inst->setUInt64Value(object_fields::Guid, createEntryGUID(lootCounter++, lootItem->definition.item->id, guid_type::Item));
			inst->setUInt32Value(item_fields::StackCount, pos.count);
			m_character->addItem(inst, pos.position);

			// Spawn this item
			std::vector<std::vector<char>> blocks;
			std::vector<char> createItemBlock;
			if (itemAtSlot == nullptr)
			{
				io::VectorSink createItemSink(createItemBlock);
				io::Writer createItemWriter(createItemSink);
				{
					UInt8 updateType = 0x02;						// Item
					UInt8 updateFlags = 0x08 | 0x10;				// 
					UInt8 objectTypeId = 0x01;						// Item

					UInt64 guid = inst->getGuid();

					// Header with object guid and type
					createItemWriter
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
					createItemWriter.sink().write((const char*)&packGUID[0], size);
					createItemWriter
						<< io::write<NetUInt8>(objectTypeId)
						<< io::write<NetUInt8>(updateFlags);

					// Lower-GUID update?
					if (updateFlags & 0x08)
					{
						createItemWriter
							<< io::write<NetUInt32>(guidLowerPart(guid));
					}

					// High-GUID update?
					if (updateFlags & 0x10)
					{
						createItemWriter
							<< io::write<NetUInt32>((guid << 48) & 0x0000FFFF);
					}

					// Write values update
					inst->writeValueUpdateBlock(createItemWriter, *m_character, true);
				}
			}
			else
			{
				io::VectorSink sink(createItemBlock);
				io::Writer writer(sink);
				{
					UInt8 updateType = 0x00;						// Update type (0x00 = UPDATE_VALUES)

					// Header with object guid and type
					UInt64 guid = itemAtSlot->getGuid();
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
					itemAtSlot->writeValueUpdateBlock(writer, *m_character, false);
				}

				itemAtSlot->clearUpdateMask();
			}

			// Send packet
			blocks.emplace_back(std::move(createItemBlock));
			sendProxyPacket(
				std::bind(game::server_write::compressedUpdateObject, std::placeholders::_1, std::cref(blocks)));
			
			// TODO: Group broadcasting

			sendProxyPacket(
				std::bind(game::server_write::itemPushResult, std::placeholders::_1, 
					m_character->getGuid(), std::cref(*inst), true, false, 0xFF, pos.position, pos.count, pos.count));
		}

		DLOG("CMSG_AUTO_STORE_LOOT_ITEM(loot slot: " << UInt32(lootSlot) << ")");
		m_loot->takeItem(lootSlot);
	}

	void Player::handleAutoEquipItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 srcBag, srcSlot;
		if (!game::client_read::autoEquipItem(packet, srcBag, srcSlot))
		{
			WLOG("Could not read packet data");
			return;
		}

		// Get item
		auto *item = m_character->getItemByPos(srcBag, srcSlot);
		if (!item)
		{
			return;
		}

		UInt8 targetSlot = 0xFF;

		// Check if item is equippable
		const auto &entry = item->getEntry();
		switch (entry.inventoryType)
		{
		case game::inventory_type::Ammo:
			// TODO
			break;
		case game::inventory_type::Head :
			targetSlot = player_equipment_slots::Head;
			break;
		case game::inventory_type::Cloak:
			targetSlot = player_equipment_slots::Back;
			break;
		case game::inventory_type::Neck:
			targetSlot = player_equipment_slots::Neck;
			break;
		case game::inventory_type::Feet:
			targetSlot = player_equipment_slots::Feet;
			break;
		case game::inventory_type::Body:
			targetSlot = player_equipment_slots::Body;
			break;
		case game::inventory_type::Chest:
		case game::inventory_type::Robe:
			targetSlot = player_equipment_slots::Chest;
			break;
		case game::inventory_type::Legs:
			targetSlot = player_equipment_slots::Legs;
			break;
		case game::inventory_type::Shoulders:
			targetSlot = player_equipment_slots::Shoulders;
			break;
		case game::inventory_type::MainHandWeapon:
			targetSlot = player_equipment_slots::Mainhand;
			break;
		case game::inventory_type::OffHandWeapon:
		case game::inventory_type::Shield:
			targetSlot = player_equipment_slots::Offhand;
			break;
		case game::inventory_type::Weapon:
			targetSlot = player_equipment_slots::Mainhand;	// TODO
			break;
		case game::inventory_type::Finger:
			targetSlot = player_equipment_slots::Finger1;
			break;
		case game::inventory_type::Trinket:
			targetSlot = player_equipment_slots::Trinket1;
			break;
		case game::inventory_type::Wrists:
			targetSlot = player_equipment_slots::Wrists;
			break;
		case game::inventory_type::Tabard:
			targetSlot = player_equipment_slots::Tabard;
			break;
		case game::inventory_type::Hands:
			targetSlot = player_equipment_slots::Hands;
			break;
		case game::inventory_type::Waist:
			targetSlot = player_equipment_slots::Waist;
			break;
		default:
			WLOG("TODO");
			break;
		}

		if (targetSlot >= player_equipment_slots::End)
		{
			// Not equippable
			return;
		}

		// Get item at target slot
		m_character->swapItem(srcSlot | 0xFF00, targetSlot | 0xFF00);
	}

	void Player::handleAutoStoreBagItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 srcBag, srcSlot, dstBag;
		if (!game::client_read::autoStoreBagItem(packet, srcBag, srcSlot, dstBag))
		{
			WLOG("Could not read packet data");
			return;
		}

		DLOG("CMSG_AUTO_STORE_BAG_ITEM(src bag: " << UInt32(srcBag) << ", src slot: " << UInt32(srcSlot) << ", dst bag: " << UInt32(dstBag) << ")");

		// TODO
		sendProxyPacket(
			std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::InternalBagError, nullptr, nullptr));
	}

	void Player::handleSwapItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 srcBag, srcSlot, dstBag, dstSlot;
		if (!game::client_read::swapItem(packet, dstBag, dstSlot, srcBag, srcSlot))
		{
			WLOG("Could not read packet data");
			return;
		}

		DLOG("CMSG_SWAP_ITEM(src bag: " << UInt32(srcBag) << ", src slot: " << UInt32(srcSlot) << ", dst bag: " << UInt32(dstBag) << ", dst slot: " << UInt32(dstSlot) << ")");

		// TODO
		sendProxyPacket(
			std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::InternalBagError, nullptr, nullptr));
	}

	void Player::handleSwapInvItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 srcSlot, dstSlot;
		if (!game::client_read::swapInvItem(packet, srcSlot, dstSlot))
		{
			WLOG("Could not read packet data");
			return;
		}

		// We don't need to do anything
		if (srcSlot == dstSlot)
			return;

		// Validate source and dest slot
		if (!m_character->isValidItemPos(player_inventory_slots::Bag_0, srcSlot))
		{
			sendProxyPacket(
				std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::ItemNotFound, nullptr, nullptr));
			return;
		}

		if (!m_character->isValidItemPos(player_inventory_slots::Bag_0, dstSlot))
		{
			sendProxyPacket(
				std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::ItemDoesNotGoToSlot, nullptr, nullptr));
			return;
		}

		UInt16 src = ((player_inventory_slots::Bag_0 << 8) | srcSlot);
		UInt16 dst = ((player_inventory_slots::Bag_0 << 8) | dstSlot);
		m_character->swapItem(src, dst);
	}

	void Player::handleSplitItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 srcBag, srcSlot, dstBag, dstSlot, count;
		if (!game::client_read::splitItem(packet, srcBag, srcSlot, dstBag, dstSlot, count))
		{
			WLOG("Could not read packet data");
			return;
		}

		DLOG("CMSG_SPLIT_ITEM(src bag: " << UInt32(srcBag) << ", src slot: " << UInt32(srcSlot) << ", dst bag: " << UInt32(dstBag) << ", dst slot: " << UInt32(dstSlot) << ", count: " << UInt32(count) << ")");

		// TODO
		sendProxyPacket(
			std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::InternalBagError, nullptr, nullptr));
	}

	void Player::handleAutoEquipItemSlot(game::Protocol::IncomingPacket &packet)
	{
		UInt64 itemGUID;
		UInt8 dstSlot;
		if (!game::client_read::autoEquipItemSlot(packet, itemGUID, dstSlot))
		{
			WLOG("Could not read packet data");
			return;
		}

		DLOG("CMSG_AUTO_EQUIP_ITEM_SLOT(item: 0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << itemGUID << std::dec << ", dst slot: " << UInt32(dstSlot) << ")");

		// TODO
		sendProxyPacket(
			std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::InternalBagError, nullptr, nullptr));
	}

	void Player::handleDestroyItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 bag, slot, count, data1, data2, data3;
		if (!game::client_read::destroyItem(packet, bag, slot, count, data1, data2, data3))
		{
			WLOG("Could not read packet data");
			return;
		}

		DLOG("CMSG_DESTROY_ITEM(bag: " << UInt32(bag) << ", slot: " << UInt32(slot) << ", count: " << UInt32(count) << ", data1: " << UInt32(data1) << ", data2: " << UInt32(data2) << ", data3: " << UInt32(data3) << ")");

		// TODO
		sendProxyPacket(
			std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::InternalBagError, nullptr, nullptr));
	}

	void Player::handleLoot(game::Protocol::IncomingPacket &packet)
	{
		UInt64 objectGuid;
		if (!game::client_read::loot(packet, objectGuid))
		{
			WLOG("Could not read packet data");
			return;
		}

		if (!m_character->isAlive())
		{
			WLOG("Dead players can't loot");
			return;
		}

		// Find game object
		GameObject *lootObject = m_character->getWorldInstance()->findObjectByGUID(objectGuid);
		if (!lootObject)
		{
			WLOG("No loot object found!");
			return;
		}

		// Is it a creature?
		if (lootObject->getTypeId() == object_type::Unit)
		{
			GameCreature *creature = reinterpret_cast<GameCreature*>(lootObject);
			if (creature->isAlive())
			{
				WLOG("Target creature is not dead and thus has no loot");
				return;
			}

			// Get loot from creature
			auto *loot = creature->getUnitLoot();
			if (loot && !loot->isEmpty())
			{
				m_loot = loot;
				m_onLootInvalidate = creature->despawned.connect([this](GameObject &despawned)
				{
					releaseLoot();
				});
				m_onLootCleared = m_loot->cleared.connect([this]()
				{
					releaseLoot();
				});

				sendProxyPacket(
					std::bind(game::server_write::lootResponse, std::placeholders::_1, objectGuid, game::loot_type::Corpse, std::cref(*loot)));

				m_character->addFlag(unit_fields::UnitFlags, game::unit_flags::Looting);
			}
		}
		else
		{
			// TODO
			WLOG("Only creatures are lootable at the moment.");
			sendProxyPacket(
				std::bind(game::server_write::lootResponseError, std::placeholders::_1, objectGuid, game::loot_type::None, game::loot_error::Locked));
		}
	}

	void Player::handleLootMoney(game::Protocol::IncomingPacket &packet)
	{
		if (!game::client_read::lootMoney(packet))
		{
			WLOG("Could not read packet data");
			return;
		}

		if (!m_loot)
		{
			WLOG("Player isn't looting anything");
			return;
		}

		// Find loot recipients
		auto lootGuid = m_loot->getLootGuid();
		auto *world = m_character->getWorldInstance();
		if (!world)
		{
			WLOG("Player is not in world instance");
			return;
		}

		// Get loot object
		GameObject *object = world->findObjectByGUID(lootGuid);
		if (!object)
		{
			WLOG("Loot object not found");
			return;
		}

		// Warning: takeGold may make m_loot invalid, because the loot will be reset if it is empty.
		UInt32 lootGold = m_loot->getGold();
		if (lootGold == 0)
		{
			WLOG("No gold available to loot");
			return;
		}

		// Check if it's a creature
		std::vector<GameCharacter*> recipients;
		if (object->getTypeId() == object_type::Unit)
		{
			GameCreature *creature = reinterpret_cast<GameCreature*>(object);
			creature->forEachLootRecipient([&recipients](GameCharacter &recipient)
			{
				recipients.push_back(&recipient);
			});

			// Share gold
			lootGold /= recipients.size();
			if (lootGold == 0)
			{
				lootGold = 1;
			}
		}
		else
		{
			WLOG("Unsupported loot object");
			return;
		}

		// Reward with gold
		for (auto *recipient : recipients)
		{
			UInt32 coinage = recipient->getUInt32Value(character_fields::Coinage);
			if (coinage >= std::numeric_limits<UInt32>::max() - lootGold)
			{
				coinage = std::numeric_limits<UInt32>::max();
			}
			else
			{
				coinage += lootGold;
			}
			recipient->setUInt32Value(character_fields::Coinage, coinage);

			// Notify players
			auto *player = m_manager.getPlayerByCharacterGuid(recipient->getGuid());
			if (player)
			{
				if (recipients.size() > 1)
				{
					player->sendProxyPacket(
						std::bind(game::server_write::lootMoneyNotify, std::placeholders::_1, lootGold));
				}

				// TODO: Put this packet into the LootInstance class or in an event callback maybe
				if (player->isLooting(lootGuid))
				{
					player->sendProxyPacket(
						std::bind(game::server_write::lootClearMoney, std::placeholders::_1));
				}
			}
		}

		// Take gold (WARNING: May reset m_loot as loot may become empty after this)
		m_loot->takeGold();
	}

	void Player::handleLootRelease(game::Protocol::IncomingPacket &packet)
	{
		UInt64 creatureId;
		if (!game::client_read::lootRelease(packet, creatureId))
		{
			WLOG("Could not read packet data");
			return;
		}

		releaseLoot();
	}

	void Player::onInventoryChangeFailure(game::InventoryChangeFailure failure, GameItem *itemA, GameItem *itemB)
	{
		// Send network packet about inventory change failure
		sendProxyPacket(
			std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, failure, itemA, itemB));
	}

	void Player::onComboPointsChanged()
	{
		sendProxyPacket(
			std::bind(game::server_write::updateComboPoints, std::placeholders::_1, m_character->getComboTarget(), m_character->getComboPoints()));
	}

	void Player::onExperienceGained(UInt64 victimGUID, UInt32 baseXP, UInt32 restXP)
	{
		sendProxyPacket(
			std::bind(game::server_write::logXPGain, std::placeholders::_1, victimGUID, baseXP, restXP, false));	// TODO: Refer a friend support
	}

	void Player::onSpellCastError(const SpellEntry &spell, game::SpellCastResult result)
	{
		sendProxyPacket(
			std::bind(game::server_write::castFailed, std::placeholders::_1, result, std::cref(spell), 0));
	}

	void Player::onLevelGained(UInt32 previousLevel, Int32 healthDiff, Int32 manaDiff, Int32 statDiff0, Int32 statDiff1, Int32 statDiff2, Int32 statDiff3, Int32 statDiff4)
	{
		UInt32 level = getCharacter()->getLevel();
		sendProxyPacket(
			std::bind(game::server_write::levelUpInfo, 
				std::placeholders::_1, 
				level,
				healthDiff,
				manaDiff,
				statDiff0,
				statDiff1,
				statDiff2,
				statDiff3,
				statDiff4)
			);
	}

	void Player::onAuraUpdated(UInt8 slot, UInt32 spellId, Int32 duration, Int32 maxDuration)
	{
		sendProxyPacket(
			std::bind(game::server_write::updateAuraDuration, std::placeholders::_1, slot, duration));

		sendProxyPacket(
			std::bind(game::server_write::setExtraAuraInfo, std::placeholders::_1, getCharacterGuid(), slot, spellId, maxDuration, duration));
	}

	void Player::onTargetAuraUpdated(UInt64 target, UInt8 slot, UInt32 spellId, Int32 duration, Int32 maxDuration)
	{
		sendProxyPacket(
			std::bind(game::server_write::setExtraAuraInfoNeedUpdate, std::placeholders::_1, target, slot, spellId, maxDuration, duration));
	}

	void Player::onTeleport(UInt16 map, float x, float y, float z, float o)
	{
		// If it's the same map, just send success notification
		if (m_character->getMapId() == map)
		{
			// Apply character position
			m_character->relocate(x, y, z, o);

			// Reset movement info
			MovementInfo info = m_character->getMovementInfo();
			info.moveFlags = game::movement_flags::None;
			info.x = x;
			info.y = y;
			info.z = z;
			info.o = o;
			sendProxyPacket(
				std::bind(game::server_write::moveTeleportAck, std::placeholders::_1, m_character->getGuid(), std::cref(info)));

			// Update movement info
			m_character->setMovementInfo(std::move(info));
		}
		else
		{
			// Send transfer pending state. This will show up the loading screen at the client side and will
			// tell the realm where our character should be sent to
			sendProxyPacket(
				std::bind(game::server_write::transferPending, std::placeholders::_1, map, 0, 0));
			m_realmConnector.sendTeleportRequest(m_characterId, map, x, y, z, o);

			// Remove the character from the world
			m_instance.removeGameObject(*m_character);
			m_character.reset();

			// Notify the realm
			m_realmConnector.notifyWorldInstanceLeft(m_characterId, pp::world_realm::world_left_reason::Teleport);

			// Destroy player instance
			m_manager.playerDisconnected(*this);
		}
	}

	void Player::handleRepopRequest(game::Protocol::IncomingPacket &packet)
	{
		if (!m_character)
		{
			return;
		}

		if (m_character->isAlive())
		{
			return;
		}

		WLOG("TODO: repop at nearest graveyard!");
		m_character->revive(m_character->getUInt32Value(unit_fields::MaxHealth), 0);
	}

	void Player::setFallInfo(UInt32 time, float z)
	{
		m_lastFallTime = time;
		m_lastFallZ = z;
	}

	void Player::releaseLoot()
	{
		if (m_loot)
		{
			m_onLootCleared.disconnect();
			m_onLootInvalidate.disconnect();

			sendProxyPacket(
				std::bind(game::server_write::lootReleaseResponse, std::placeholders::_1, m_loot->getLootGuid()));
			m_loot = nullptr;

			m_character->removeFlag(unit_fields::UnitFlags, game::unit_flags::Looting);
		}
	}

	bool Player::isIgnored(UInt64 guid) const
	{
		return guid == 504403158265495562;
	}

	void Player::handleMovementCode(game::Protocol::IncomingPacket &packet, UInt16 opCode)
	{
		MovementInfo info;
		if (!game::client_read::moveHeartBeat(packet, info))
		{
			WLOG("Could not read packet data");
			return;
		}

		UInt64 currentTime = getCurrentTime();
		if (m_nextDelayReset <= currentTime)
		{
			m_clientDelayMs = 0;
			m_nextDelayReset = currentTime + constants::OneSecond * 30;
		}

		if (opCode != game::client_packet::MoveStop)
		{
			auto flags = m_character->getUInt32Value(unit_fields::UnitFlags);
			if ((flags & 0x00040000) != 0 ||
				(flags & 0x00000004) != 0)
			{
				WLOG("Character is unable to move, ignoring move packet");
				return;
			}
		}

		// Sender guid
		auto guid = m_character->getGuid();

		// Get object location
		float x, y, z, o;
		m_character->getLocation(x, y, z, o);

		// Store movement information
		m_character->setMovementInfo(info);

		// Transform into grid location
		TileIndex2D gridIndex;
		auto &grid = getWorldInstance().getGrid();
		if (!grid.getTilePosition(x, y, z, gridIndex[0], gridIndex[1]))
		{
			// TODO: Error?
			ELOG("Could not resolve grid location!");
			return;
		}

		UInt32 msTime = mTimeStamp();
		if (m_clientDelayMs == 0)
		{
			m_clientDelayMs = msTime - info.time;
		}
		UInt32 move_time = (info.time - (msTime - m_clientDelayMs)) + 500 + msTime;

		// Get grid tile
		auto &tile = grid.requireTile(gridIndex);
		info.time = move_time;

		// Notify all watchers about the new object
		forEachTileInSight(
			getWorldInstance().getGrid(),
			gridIndex,
			[this, &info, opCode, guid, move_time](VisibilityTile &tile)
		{
			for (auto &watcher : tile.getWatchers())
			{
				if (watcher != this)
				{
					// Convert timestamps
					//info.time = watcher->convertTimestamp(move_time, m_clientTicks) + 500;
					//info.fallTime = watcher->convertTimestamp(info.fallTime, m_clientTicks) + 500;

					// Create the chat packet
					std::vector<char> buffer;
					io::VectorSink sink(buffer);
					game::Protocol::OutgoingPacket movePacket(sink);
					game::server_write::movePacket(movePacket, opCode, guid, info);

					watcher->sendPacket(movePacket, buffer);
				}
			}
		});

		//TODO: Verify new location

		UInt32 lastFallTime = 0;
		float lastFallZ = 0.0f;
		getFallInfo(lastFallTime, lastFallZ);

		// Fall damage
		if (opCode == game::client_packet::MoveFallLand)
		{
			if (info.fallTime >= 1100)
			{
				float deltaZ = lastFallZ - info.z;
				if (m_character->isAlive())
				{
					float damageperc = 0.018f * deltaZ - 0.2426f;
					if (damageperc > 0)
					{
						const UInt32 maxHealth = m_character->getUInt32Value(unit_fields::MaxHealth);
						UInt32 damage = (UInt32)(damageperc * maxHealth);
						float height = info.z;

						if (damage > 0)
						{
							//Prevent fall damage from being more than the player maximum health
							if (damage > maxHealth) damage = maxHealth;

							std::vector<char> dmgBuffer;
							io::VectorSink dmgSink(dmgBuffer);
							game::Protocol::OutgoingPacket dmgPacket(dmgSink);
							game::server_write::environmentalDamageLog(dmgPacket, getCharacterGuid(), 2, damage, 0, 0);

							// Deal damage
							forEachTileInSight(
								getWorldInstance().getGrid(),
								gridIndex,
								[&dmgPacket, &dmgBuffer](VisibilityTile &tile)
							{
								for (auto &watcher : tile.getWatchers())
								{
									watcher->sendPacket(dmgPacket, dmgBuffer);
								}
							});

							m_character->dealDamage(damage, 0, nullptr, true);
							if (!m_character->isAlive())
							{
								WLOG("TODO: Durability damage!");
								sendProxyPacket(
									std::bind(game::server_write::durabilityDamageDeath, std::placeholders::_1));
							}
						}
					}
				}
			}
		}

		if (opCode == game::client_packet::MoveFallLand ||
			lastFallTime > info.fallTime/* ||
										info.z < lastFallZ*/)
		{
			setFallInfo(info.fallTime, info.z);
		}

		// Update position
		m_character->relocate(info.x, info.y, info.z, info.o);
	}

	void Player::handleTimeSyncResponse(game::Protocol::IncomingPacket &packet)
	{
		UInt32 counter, ticks;
		if (!game::client_read::timeSyncResponse(packet, counter, ticks))
		{
			WLOG("Could not read packet data");
			return;
		}

		DLOG("TIME SYNC RESPONSE " << m_character->getName() << ": Counter " << counter << "; Ticks: " << ticks);
		m_clientTicks = ticks;
	}

	UInt32 Player::convertTimestamp(UInt32 otherTimestamp, UInt32 otherTick) const
	{
		UInt32 otherDiff = otherTimestamp - otherTick;
		return m_clientTicks + otherDiff;
	}

	void Player::addIgnore(UInt64 guid)
	{
		if (!m_ignoredGUIDs.contains(guid)) m_ignoredGUIDs.add(guid);
	}

	void Player::removeIgnore(UInt64 guid)
	{
		if (m_ignoredGUIDs.contains(guid)) m_ignoredGUIDs.remove(guid);
	}

	void Player::updateCharacterGroup(UInt64 group)
	{
		if (m_character)
		{
			UInt64 oldGroup = m_character->getGroupId();
			m_character->setGroupId(group);

			m_character->forceFieldUpdate(unit_fields::Health);
			m_character->forceFieldUpdate(unit_fields::MaxHealth);

			// For every group member in range, also update health fields
			TileIndex2D tileIndex;
			m_character->getTileIndex(tileIndex);
			forEachSubscriberInSight(
				m_character->getWorldInstance()->getGrid(),
				tileIndex,
				[oldGroup, group](ITileSubscriber &subscriber)
			{
				auto *character = subscriber.getControlledObject();
				if (character)
				{
					if ((oldGroup != 0 && character->getGroupId() == oldGroup) ||
						(group != 0 && character->getGroupId() == group))
					{
						character->forceFieldUpdate(unit_fields::Health);
						character->forceFieldUpdate(unit_fields::MaxHealth);
					}
				}
			});
		}

		if (group != 0)
		{
			// Trigger next group member update
			m_groupUpdate.setEnd(getCurrentTime() + (constants::OneSecond * 3));
		}
		else
		{
			// Stop group member update
			m_groupUpdate.cancel();
		}
	}

	void Player::handleLearnTalent(game::Protocol::IncomingPacket &packet)
	{
		UInt32 talentId = 0, rank = 0;
		if (!game::client_read::learnTalent(packet, talentId, rank))
		{
			WLOG("Could not read packet data");
			return;
		}

		// Try to find talent
		auto *talent = m_project.talents.getById(talentId);
		if (!talent)
		{
			WLOG("Could not find requested talent id " << talentId);
			return;
		}

		// Check rank
		if (talent->ranks.size() < rank)
		{
			WLOG("Talent " << talentId << " does offer " << talent->ranks.size() << " ranks, but rank " << rank << " is requested!");
			return;
		}

		// TODO: Check whether the player is allowed to learn that talent, based on his class

		// Check if another talent is required
		if (talent->dependsOn != nullptr)
		{
			// Check if we have learned the requested talent rank
			auto *dependantRank = talent->dependsOn->ranks[talent->dependsOnRank];
			if (!m_character->hasSpell(dependantRank->id))
			{
				WLOG("Dependent talent not learned!");
				return;
			}
		}

		// Check if another spell is required
		if (talent->dependsOnSpell != nullptr)
		{
			if (!m_character->hasSpell(talent->dependsOnSpell->id))
			{
				WLOG("Dependent spell not learned!");
				return;
			}
		}

		// Check if we have enough talent points
		UInt32 freeTalentPoints = m_character->getUInt32Value(character_fields::CharacterPoints_1);
		if (freeTalentPoints == 0)
		{
			WLOG("Not enough talent points available");
			return;
		}

		// Remove all previous learnt ranks, learn the highest one
		for (UInt32 i = 0; i < rank; ++i)
		{
			if (m_character->removeSpell(*talent->ranks[i]))
			{
				// TODO: Send packet maybe?
			}
		}

		// Add new spell, and maybe remove old spells
		m_character->addSpell(*talent->ranks[rank]);
		if ((talent->ranks[rank]->attributes & spell_attributes::Passive) != 0)
		{
			SpellTargetMap targets;
			targets.m_targetMap = game::spell_cast_target_flags::Unit;
			targets.m_unitTarget = m_character->getGuid();
			m_character->castSpell(std::move(targets), talent->ranks[rank]->id, 0, false, GameUnit::SpellSuccessCallback());
		}

		sendProxyPacket(
			std::bind(game::server_write::learnedSpell, std::placeholders::_1, talent->ranks[rank]->id));
	}

	void Player::handleUseItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 bagId = 0, slotId = 0, spellCount = 0, castCount = 0;
		UInt64 itemGuid = 0;
		SpellTargetMap targetMap;
		if (!game::client_read::useItem(packet, bagId, slotId, spellCount, castCount, itemGuid, targetMap))
		{
			WLOG("Could not read packet data");
			return;
		}

		// Get item
		auto *item = m_character->getItemByPos(bagId, slotId);
		if (!item)
		{
			sendProxyPacket(
				std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::ItemNotFound, nullptr, nullptr));
			return;
		}

		if (item->getGuid() != itemGuid)
		{
			sendProxyPacket(
				std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::ItemNotFound, nullptr, nullptr));
			return;
		}

		auto &entry = item->getEntry();
		for (auto &spell : entry.itemSpells)
		{
			if (!spell.spell)
			{
				continue;
			}

			// Spell effect has to be triggered "on use", not "on equip" etc.
			if (spell.trigger != 0 && spell.trigger != 5)
			{
				continue;
			}

			UInt64 time = 0;
			const auto *castTime = m_project.castTimes.getById(spell.spell->castTimeIndex);
			if (castTime)
			{
				time = castTime->castTime;
			}
			m_character->castSpell(std::move(targetMap), spell.spell->id, time, false, GameUnit::SpellSuccessCallback());
		}
	}

	void Player::handleListInventory(game::Protocol::IncomingPacket &packet)
	{
		UInt64 vendorGuid = 0;
		if (!game::client_read::listInventory(packet, vendorGuid))
		{
			WLOG("Could not read packet data");
			return;
		}

		if (!m_character->isAlive())
			return;

		auto *world = m_character->getWorldInstance();
		if (!world)
			return;

		// Try to find that vendor
		GameCreature *vendor = dynamic_cast<GameCreature*>(m_character->getWorldInstance()->findObjectByGUID(vendorGuid));
		if (!vendor)
		{
			// Could not find vendor by GUID or vendor is not a creature
			return;
		}

		// Check if vendor is alive and not in fight
		if (!vendor->isAlive() || vendor->isInCombat())
			return;

		// Check if vendor is not hostile against players
		const auto &vendorFaction = m_character->getFactionTemplate();
		if (vendorFaction.isHostileToPlayers())
			return;

		// Check if that vendor has the vendor flag
		if ((vendor->getUInt32Value(unit_fields::NpcFlags) & game::unit_npc_flags::Vendor) == 0)
			return;

		// Check if the vendor DO sell items
		const auto *vendorEntry = vendor->getEntry().vendorEntry;
		if (!vendorEntry)
			return;

		sendProxyPacket(
			std::bind(game::server_write::listInventory, std::placeholders::_1, vendor->getGuid(), std::cref(vendorEntry->items)));
	}

	void Player::handleBuyItem(game::Protocol::IncomingPacket &packet)
	{
		UInt64 vendorGuid = 0;
		UInt32 item = 0;
		UInt8 count = 0;
		if (!(game::client_read::buyItem(packet, vendorGuid, item, count)))
		{
			WLOG("Coult not read CMSG_BUY_ITEM packet");
			return;
		}

		buyItemFromVendor(vendorGuid, item, 0, 0xFF, count);
	}

	void Player::handleBuyItemInSlot(game::Protocol::IncomingPacket &packet)
	{
		UInt64 vendorGuid = 0, bagGuid = 0;
		UInt32 item = 0;
		UInt8 slot = 0, count = 0;
		if (!(game::client_read::buyItemInSlot(packet, vendorGuid, item, bagGuid, slot, count)))
		{
			WLOG("Coult not read CMSG_BUY_ITEM_IN_SLOT packet");
			return;
		}

		buyItemFromVendor(vendorGuid, item, bagGuid, slot, count);
	}

	void Player::handleSellItem(game::Protocol::IncomingPacket &packet)
	{
		UInt64 vendorGuid = 0, itemGuid = 0;
		UInt8 count = 0;
		if (!(game::client_read::sellItem(packet, vendorGuid, itemGuid, count)))
		{
			WLOG("Coult not read CMSG_SELL_ITEM packet");
			return;
		}

		DLOG("Received CMSG_SELL_ITEM...");
	}

	void Player::buyItemFromVendor(UInt64 vendorGuid, UInt32 itemEntry, UInt64 bagGuid, UInt8 slot, UInt8 count)
	{
		if (count < 1) count = 1;

		const UInt8 MaxBagSlots = 36;
		if (slot != 0xFF && slot >= MaxBagSlots)
			return;

		if (!m_character->isAlive())
			return;

		const auto *item = m_project.items.getById(itemEntry);
		if (!item)
			return;

		auto *world = m_character->getWorldInstance();
		if (!world)
			return;

		GameCreature *creature = dynamic_cast<GameCreature*>(world->findObjectByGUID(vendorGuid));
		if (!creature)
			return;

		const auto *vendorEntry = creature->getEntry().vendorEntry;
		if (!vendorEntry || vendorEntry->items.empty())
			return;

		bool validEntry = false;
		for (auto &entry : vendorEntry->items)
		{
			if (entry.item == item)
			{
				validEntry = true;
				break;
			}
		}

		if (!validEntry)
			return;

		// TODO: Check item amount

		// Check if item is storable
		ItemPosCountVector slots;
		auto result = m_character->canStoreItem(0xFF, slot, slots, *item, count, false);
		if (result != game::inventory_change_failure::Okay)
		{
			// TODO: Send error
			WLOG("Can't store item");
			return;
		}

		// Take money
		UInt32 price = item->buyPrice * count;
		UInt32 money = m_character->getUInt32Value(character_fields::Coinage);
		if (money < price)
		{
			// TODO: Send error message
			WLOG("Not enough money");
			return;
		}

		m_character->setUInt32Value(character_fields::Coinage, money - price);

		static UInt64 vendorCounter = 0x100000;
		for (auto &pos : slots)
		{
			auto *itemAtSlot = m_character->getItemByPos(0xFF, pos.position);

			// Loot items and add them
			auto inst = std::make_shared<GameItem>(*item);
			inst->initialize();
			inst->setUInt64Value(object_fields::Guid, createEntryGUID(vendorCounter++, item->id, guid_type::Item));
			inst->setUInt32Value(item_fields::StackCount, pos.count);
			m_character->addItem(inst, pos.position);

			// Spawn this item
			std::vector<std::vector<char>> blocks;
			std::vector<char> createItemBlock;
			if (itemAtSlot == nullptr)
			{
				io::VectorSink createItemSink(createItemBlock);
				io::Writer createItemWriter(createItemSink);
				{
					UInt8 updateType = 0x02;						// Item
					UInt8 updateFlags = 0x08 | 0x10;				// 
					UInt8 objectTypeId = 0x01;						// Item

					UInt64 guid = inst->getGuid();

					// Header with object guid and type
					createItemWriter
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
					createItemWriter.sink().write((const char*)&packGUID[0], size);
					createItemWriter
						<< io::write<NetUInt8>(objectTypeId)
						<< io::write<NetUInt8>(updateFlags);

					// Lower-GUID update?
					if (updateFlags & 0x08)
					{
						createItemWriter
							<< io::write<NetUInt32>(guidLowerPart(guid));
					}

					// High-GUID update?
					if (updateFlags & 0x10)
					{
						createItemWriter
							<< io::write<NetUInt32>((guid << 48) & 0x0000FFFF);
					}

					// Write values update
					inst->writeValueUpdateBlock(createItemWriter, *m_character, true);
				}
			}
			else
			{
				io::VectorSink sink(createItemBlock);
				io::Writer writer(sink);
				{
					UInt8 updateType = 0x00;						// Update type (0x00 = UPDATE_VALUES)

					// Header with object guid and type
					UInt64 guid = itemAtSlot->getGuid();
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
					itemAtSlot->writeValueUpdateBlock(writer, *m_character, false);
				}

				itemAtSlot->clearUpdateMask();
			}

			// Send packet
			blocks.emplace_back(std::move(createItemBlock));
			sendProxyPacket(
				std::bind(game::server_write::compressedUpdateObject, std::placeholders::_1, std::cref(blocks)));
			sendProxyPacket(
				std::bind(game::server_write::itemPushResult, std::placeholders::_1,
				m_character->getGuid(), std::cref(*inst), false, false, 0xFF, pos.position, pos.count, pos.count));
		}
	}

}
