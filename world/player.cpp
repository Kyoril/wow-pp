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

#include "player.h"
#include "player_manager.h"
#include "log/default_log_levels.h"
#include "game/world_instance_manager.h"
#include "game/world_instance.h"
#include "game/each_tile_in_sight.h"
#include <cassert>
#include <limits>

using namespace std;

namespace wowpp
{
	Player::Player(PlayerManager &manager, RealmConnector &realmConnector, WorldInstanceManager &worldInstanceManager, DatabaseId characterId, std::shared_ptr<GameCharacter> character, WorldInstance &instance)
		: m_manager(manager)
		, m_realmConnector(realmConnector)
		, m_worldInstanceManager(worldInstanceManager)
		, m_characterId(characterId)
		, m_character(std::move(character))
		, m_logoutCountdown(worldInstanceManager.getTimerQueue())
		, m_instance(instance)
	{
		m_logoutCountdown.ended.connect(
			std::bind(&Player::onLogout, this));
		m_onSpawn = m_character->spawned.connect(
			std::bind(&Player::onSpawn, this));
		m_onDespawn = m_character->despawned.connect(
			std::bind(&Player::onDespawn, this));
		m_onTileChange = m_character->tileChangePending.connect(
			std::bind(&Player::onTileChangePending, this, std::placeholders::_1, std::placeholders::_2));
		onProfChanged = m_character->proficiencyChanged.connect(
			std::bind(&Player::onProficiencyChanged, this, std::placeholders::_1, std::placeholders::_2));
		m_onAtkSwingErr = m_character->autoAttackError.connect(
			std::bind(&Player::onAttackSwingError, this, std::placeholders::_1));
		onInvFailure = m_character->inventoryChangeFailure.connect(
			std::bind(&Player::onInventoryChangeFailure, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		m_onComboPoints = m_character->comboPointsChanged.connect(
			std::bind(&Player::onComboPointsChanged, this));
		m_onXP = m_character->experienceGained.connect(
			std::bind(&Player::onExperienceGained, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

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

		// Setup the logout countdown
		m_logoutCountdown.setEnd(
			getCurrentTime() + (20 * constants::OneSecond));
	}

	void Player::cancelLogoutRequest()
	{
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
				[&subscribers](VisibilityTile &tile)
			{
				for (auto * const subscriber : tile.getWatchers().getElements())
				{
					subscribers.push_back(subscriber);
				}
			});

			if (type != game::chat_msg::Say &&
				type != game::chat_msg::Yell)
			{
				WLOG("Unsupported chat mode");
				return;
			}

			// Create the chat packet
			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);
			game::server_write::messageChat(packet, type, lang, channel, m_character->getGuid(), message, m_character.get());

			const float chatRange = (type == game::chat_msg::Yell) ? 300.0f : 25.0f;
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
					instance->writeValueUpdateBlock(createItemWriter, true);
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
					<< io::write<NetUInt32>(static_cast<UInt32>(getCurrentTime()));	//TODO: Time

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
			m_character->writeValueUpdateBlock(writer, true);

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

		// Create spawn message blocks
		std::vector<std::vector<char>> spawnBlocks;
		createUpdateBlocks(*m_character, spawnBlocks);



		auto &grid = m_instance.getGrid();

		// Spawn ourself for new watchers
		forEachTileInSightWithout(
			grid,
			newTile.getPosition(),
			oldTile.getPosition(),
			[&spawnBlocks, this](VisibilityTile &tile)
		{
			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);
			game::server_write::compressedUpdateObject(packet, spawnBlocks);

			for(auto * subscriber : tile.getWatchers().getElements())
			{
				assert(subscriber != this);
				subscriber->sendPacket(packet, buffer);
			}

			for (auto *object : tile.getGameObjects().getElements())
			{
				std::vector<std::vector<char>> createBlock;
				createUpdateBlocks(*object, createBlock);

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

		DLOG("CMSG_AUTO_STORE_LOOT_ITEM(loot slot: " << UInt32(lootSlot) << ")");

		// TODO
		sendProxyPacket(
			std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::InternalBagError, nullptr, nullptr));
	}

	void Player::handleAutoEquipItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 srcBag, srcSlot;
		if (!game::client_read::autoEquipItem(packet, srcBag, srcSlot))
		{
			WLOG("Could not read packet data");
			return;
		}

		DLOG("CMSG_AUTO_EQUIP_ITEM(bag: " << UInt32(srcBag) << ", slot: " << UInt32(srcSlot) << ")");

		// TODO
		sendProxyPacket(
			std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::InternalBagError, nullptr, nullptr));
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

}
