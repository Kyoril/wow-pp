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

#include "world.h"
#include "world_manager.h"
#include "database.h"
#include "log/default_log_levels.h"
#include "common/clock.h"
#include "wowpp_protocol/wowpp_protocol.h"
#include "wowpp_protocol/wowpp_world_realm.h"
#include "binary_io/vector_sink.h"
#include "binary_io/writer.h"
#include "data/project.h"
#include "player_manager.h"
#include "player.h"
#include "player_group.h"
#include <algorithm>
#include <cassert>
#include <limits>

using namespace std;

namespace wowpp
{
	World::World(WorldManager &manager, PlayerManager &playerManager, Project &project, IDatabase &database, std::shared_ptr<Client> connection, String address, String realmName)
		: m_manager(manager)
		, m_playerManager(playerManager)
		, m_project(project)
		, m_database(database)
		, m_connection(std::move(connection))
		, m_address(std::move(address))
		, m_authed(false)
		, m_realmName(std::move(realmName))
	{
		assert(m_connection);

		m_connection->setListener(*this);
	}

	void World::destroy()
	{
		m_connection->resetListener();
		m_connection.reset();

		m_manager.worldDisconnected(*this);
	}

	void World::connectionLost()
	{
		ILOG("World " << m_address << " disconnected");
		
		// Disconnect all players which are logged in to this world server
		onConnectionLost();

		destroy();
	}

	void World::connectionMalformedPacket()
	{
		ILOG("World " << m_address << " sent malformed packet");
		destroy();
	}

	void World::connectionPacketReceived(pp::IncomingPacket &packet)
	{
		using namespace wowpp::pp::world_realm;

		const auto packetId = packet.getId();
		switch (packetId)
		{
			case world_packet::Login:
				handleLogin(packet);
				break;
			case world_packet::WorldInstanceEntered:
				handleWorldInstanceEntered(packet);
				break;
			case world_packet::WorldInstanceLeft:
				handleWorldInstanceLeft(packet);
				break;
			case world_packet::WorldInstanceError:
				handleWorldInstanceError(packet);
				break;
			case world_packet::ClientProxyPacket:
				handleClientProxyPacket(packet);
				break;
			case world_packet::CharacterData:
				handleCharacterData(packet);
				break;
			case world_packet::TeleportRequest:
				handleTeleportRequest(packet);
				break;
			case world_packet::CharacterGroupUpdate:
				handleCharacterGroupUpdate(packet);
				break;
			default:
			{
				WLOG("Unknown packet received from world " << m_address
					<< " - ID: " << static_cast<UInt32>(packetId)
					<< "; Size: " << packet.getSource()->size() << " bytes");
				break;
			}
		}
	}

	void World::handleLogin(pp::IncomingPacket &packet)
	{
		using namespace wowpp::pp::world_realm;

		// Read login packet
		UInt32 protocolVersion;
		MapList mapIds;
		std::vector<UInt32> instances;
		if (!world_read::login(packet, protocolVersion, mapIds, instances))
		{
			return;
		}

		// Apply instances
		for (auto &instance : instances)
		{
			m_instances.add(instance);
		}

		// Check all maps
		for (auto &mapId : mapIds)
		{
			auto map = m_project.maps.getById(mapId);
			if (!map)
			{
				WLOG("Map id " << mapId << " is unknown");
				continue;
			}

			// Is the map id already handled?
			if (m_manager.getWorldByMapId(mapId))
			{
				WLOG("Map id " << mapId << " is already handled by another world server");
				continue;;
			}
			else
			{
				m_mapIds.push_back(mapId);
			}
		}

		// Empty vector?
		if (m_mapIds.empty())
		{
			WLOG("No supported map ids: World server at " << m_address << " will be of no use.");
			m_connection->sendSinglePacket(
				std::bind(realm_write::loginAnswer, std::placeholders::_1, login_result::MapsAlreadyInUse, std::cref(m_realmName)));
			return;
		}

		// Successfully logged in
		ILOG("World node registered successfully");
		m_authed = true;
		m_connection->sendSinglePacket(
			std::bind(realm_write::loginAnswer, std::placeholders::_1, login_result::Success, std::cref(m_realmName)));
	}

	void World::enterWorldInstance(UInt64 characterGuid, UInt32 instanceId, const GameCharacter &character, const std::vector<pp::world_realm::ItemData> &out_items)
	{
		// Get a list of spells
		std::vector<UInt32> spellIds;
		for (const auto *spell : character.getSpells())
		{
			spellIds.push_back(spell->id);
		}

		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::realm_write::characterLogIn, std::placeholders::_1, characterGuid, instanceId, std::cref(character), std::cref(spellIds), std::cref(out_items)));
	}

	void World::leaveWorldInstance(UInt64 characterGuid, pp::world_realm::WorldLeftReason reason)
	{
		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::realm_write::leaveWorldInstance, std::placeholders::_1, characterGuid, reason));
	}

	void World::sendProxyPacket(UInt64 characterGuid, UInt16 opCode, UInt32 size, const std::vector<char> &buffer)
	{
		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::realm_write::clientProxyPacket, std::placeholders::_1, characterGuid, opCode, size, std::cref(buffer)));
	}

	void World::handleWorldInstanceEntered(pp::IncomingPacket &packet)
	{
		using namespace wowpp::pp::world_realm;

		// Read login packet
		UInt32 instanceId;
		DatabaseId characterDbId;
		UInt64 worldObjectGuid;
		UInt32 mapId, zoneId;
		float x, y, z, o;
		if (!world_read::worldInstanceEntered(packet, characterDbId, worldObjectGuid, instanceId, mapId, zoneId, x, y, z, o))
		{
			return;
		}

		// Store instance id if not already available
		if (!m_instances.contains(instanceId))
		{
			m_instances.add(instanceId);
		}

		// Notify player about this
		auto player = m_playerManager.getPlayerByCharacterId(characterDbId);
		if (!player)
		{
			WLOG("Could not find player with character id " << characterDbId);
			return;
		}

		// Send world instance data
		player->worldInstanceEntered(*this, instanceId, worldObjectGuid, mapId, zoneId, x, y, z, o);
	}

	void World::handleWorldInstanceError(pp::IncomingPacket &packet)
	{
		using namespace wowpp::pp::world_realm;

		// Read login packet
		DatabaseId characterDbId;
		world_instance_error::Type error;
		if (!world_read::worldInstanceError(packet, characterDbId, error))
		{
			return;
		}

		// Not authorised
		if (!m_authed)
		{
			return;
		}

		switch (error)
		{
			case world_instance_error::UnsupportedMap:
			{
				ELOG("Specified map is not supported by the world server at " << m_address);
				break;
			}

			case world_instance_error::TooManyInstances:
			{
				ELOG("World server " << m_address << " already has too many instances running of that map");
				break;
			}

			case world_instance_error::InternalError:
			{
				ELOG("Internal error at the world server at " << m_address);
				break;
			}

			default:
			{
				ELOG("Unknown error code from world server");
				break;
			}
		}

		// TODO: Notify player connection

	}

	void World::handleWorldInstanceLeft(pp::IncomingPacket &packet)
	{
		DatabaseId characterId;
		pp::world_realm::WorldLeftReason reason;
		if (!pp::world_realm::world_read::worldInstanceLeft(packet, characterId, reason))
		{
			return;
		}

		// Notify player about this
		auto player = m_playerManager.getPlayerByCharacterId(characterId);
		if (!player)
		{
			WLOG("Could not find player with character id " << characterId);
			return;
		}

		// Send world instance data
		UInt32 instanceId = 0;	//TODO
		player->worldInstanceLeft(*this, instanceId, reason);
	}

	void World::handleClientProxyPacket(pp::IncomingPacket &packet)
	{
		DatabaseId characterId;
		UInt16 opCode;
		UInt32 size;
		std::vector<char> buffer;
		if (!(pp::world_realm::world_read::clientProxyPacket(packet, characterId, opCode, size, buffer)))
		{
			WLOG("ERROR READING CLIENT PROXY PACKET!");
			return;
		}

		if (opCode == 0)
		{
			WLOG("OPCODE ZERO");
			return;
		}

		if (!m_authed)
		{
			return;
		}

		// Redirect client packet
		auto *player = m_playerManager.getPlayerByCharacterId(characterId);
		if (!player)
		{
			WLOG("Could not find player to redirect packet " << opCode);
			return;
		}

		// Build client packet to send
		std::vector<char> outBuffer;
		io::VectorSink sink(outBuffer);
		game::OutgoingPacket proxyPacket(sink, true);
		proxyPacket
			<< io::write_range(buffer);

		// Send packet to player
		player->sendProxyPacket(opCode, outBuffer);
	}

	void World::sendChatMessage(UInt64 characterGuid, game::ChatMsg type, game::Language lang, const String &receiver, const String &channel, const String &message)
	{
		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::realm_write::chatMessage, std::placeholders::_1, characterGuid, type, lang, std::cref(receiver), std::cref(channel), std::cref(message)));
	}

	void World::handleCharacterData(pp::IncomingPacket &packet)
	{
		const size_t packetStart = packet.getSource()->position();

		// Read the character ID first
		UInt64 characterId;
		if (!(packet >> io::read<NetUInt64>(characterId)))
		{
			return;
		}

		packet.getSource()->seek(packetStart);

		// Find the player using this character
		auto *player = m_playerManager.getPlayerByCharacterId(characterId);
		if (!player)
		{
			// Maybe the player disconnected already - create a temporary character copy and save this copy
			std::shared_ptr<GameCharacter> character(new GameCharacter(
				m_playerManager.getTimers(),
				std::bind(&RaceEntryManager::getById, &m_project.races, std::placeholders::_1),
				std::bind(&ClassEntryManager::getById, &m_project.classes, std::placeholders::_1),
				std::bind(&LevelEntryManager::getById, &m_project.levels, std::placeholders::_1),
				std::bind(&SpellEntryManager::getById, &m_project.spells, std::placeholders::_1)));
			character->initialize();

			std::vector<UInt32> spellIds;
			std::vector<pp::world_realm::ItemData> items;
			if (!(pp::world_realm::world_read::characterData(packet, characterId, *character, spellIds, items)))
			{
				ELOG("Error reading character data");
				return;
			}

			// Save the character data
			m_database.saveGameCharacter(*character, items, spellIds);
			return;
		}
		else
		{
			std::vector<UInt32> spellIds;

			auto &items = player->getItemData();
			items.clear();

			if (!(pp::world_realm::world_read::characterData(packet, characterId, *player->getGameCharacter(), spellIds, items)))
			{
				ELOG("Error reading character data");
				return;
			}

			// Save character
			m_database.saveGameCharacter(*player->getGameCharacter(), items, spellIds);
		}
	}

	void World::handleTeleportRequest(pp::IncomingPacket &packet)
	{
		// Read the character ID first
		UInt64 characterId;
		UInt32 map;
		float x, y, z, o;
		if (!(pp::world_realm::world_read::teleportRequest(packet, characterId, map, x, y, z, o)))
		{
			return;
		}

		// Find the player using this character
		auto *player = m_playerManager.getPlayerByCharacterId(characterId);
		if (!player)
		{
			ELOG("Can't find player by character id - transfer failed");
			return;
		}

		DLOG("INITIALIZING TRANSFER TO: " << map << " - " << x << " / " << y << " / " << z << " / " << o);

		// Initialize transfer
		player->initializeTransfer(map, x, y, z, o);
	}

	void World::handleCharacterGroupUpdate(pp::IncomingPacket &packet)
	{
		UInt64 characterId;
		std::vector<UInt64> nearbyMembers;
		UInt32 map, zone, health, maxHealth, power, maxPower;
		UInt8 powerType, level;
		float x, y, z;
		std::vector<UInt32> auras;
		if (!(pp::world_realm::world_read::characterGroupUpdate(packet, characterId, nearbyMembers, health, maxHealth, powerType, power, maxPower, level, map, zone, x, y, z, auras)))
		{
			return;
		}

		auto *player = m_playerManager.getPlayerByCharacterGuid(characterId);
		if (!player)
		{
			WLOG("Couldn't find player by character id for group update");
			return;
		}

		auto *group = player->getGroup();
		if (!group)
		{
			return;
		}

		// Optimization (if all members are nearby, no update is needed
		if (nearbyMembers.size() >= group->getMemberCount())
		{
			return;
		}

		// We don't want to be notified either
		nearbyMembers.push_back(characterId);

		auto *character = player->getGameCharacter();
		if (character)
		{
			character->setUInt32Value(unit_fields::Level, level);
			character->setUInt32Value(unit_fields::Health, health);
			character->setUInt32Value(unit_fields::MaxHealth, maxHealth);
			character->setByteValue(unit_fields::Bytes0, 3, powerType);
			character->setUInt32Value(unit_fields::Power1 + powerType, power);
			character->setUInt32Value(unit_fields::MaxPower1 + powerType, maxPower);
			character->setMapId(map);
			character->setZone(zone);
			character->relocate(x, y, z, 0.0f);
			// TODO: Auras

			// TODO: Do some heavy optimization here
			group->broadcastPacket(
				std::bind(game::server_write::partyMemberStats, std::placeholders::_1, std::cref(*character)), &nearbyMembers);
		}
	}

	void World::characterGroupChanged(UInt64 characterGuid, UInt64 groupId)
	{
		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::realm_write::characterGroupChanged, std::placeholders::_1, characterGuid, groupId));
	}

	void World::characterIgnoreList(UInt64 characterGuid, const std::vector<UInt64> &list)
	{
		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::realm_write::ignoreList, std::placeholders::_1, characterGuid, std::cref(list)));
	}
	void World::characterAddIgnore(UInt64 characterGuid, UInt64 ignoreGuid)
	{
		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::realm_write::addIgnore, std::placeholders::_1, characterGuid, ignoreGuid));
	}
	void World::characterRemoveIgnore(UInt64 characterGuid, UInt64 removeGuid)
	{
		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::realm_write::removeIgnore, std::placeholders::_1, characterGuid, removeGuid));
	}

	void World::itemData(UInt64 characterGuid, const std::map<UInt16, pp::world_realm::ItemData>& items)
	{
		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::realm_write::itemData, std::placeholders::_1, characterGuid, std::cref(items)));
	}

}
