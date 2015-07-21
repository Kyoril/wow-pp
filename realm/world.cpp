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
			{
				handleLogin(packet);
				break;
			}

			case world_packet::WorldInstanceEntered:
			{
				handleWorldInstanceEntered(packet);
				break;
			}

			case world_packet::WorldInstanceLeft:
			{
				handleWorldInstanceLeft(packet);
				break;
			}

			case world_packet::WorldInstanceError:
			{
				handleWorldInstanceError(packet);
				break;
			}

			case world_packet::ClientProxyPacket:
			{
				handleClientProxyPacket(packet);
				break;
			}

			case world_packet::CharacterData:
			{
				handleCharacterData(packet);
				break;
			}

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
		std::vector<UInt32> mapIds;
		if (!world_read::login(packet, protocolVersion, mapIds, m_instances))
		{
			return;
		}

		// Notify
		ILOG("World node announcement: " << mapIds.size() << " maps and " << m_instances.size() << " instances");

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

	void World::enterWorldInstance(DatabaseId characterDbId, const GameCharacter &character, const std::vector<pp::world_realm::ItemData> &out_items)
	{
		// Get a list of spells
		std::vector<UInt32> spellIds;
		for (const auto *spell : character.getSpells())
		{
			spellIds.push_back(spell->id);
		}

		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::realm_write::characterLogIn, std::placeholders::_1, characterDbId, std::cref(character), std::cref(spellIds), std::cref(out_items)));
	}

	void World::leaveWorldInstance(DatabaseId characterDbId, pp::world_realm::WorldLeftReason reason)
	{
		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::realm_write::leaveWorldInstance, std::placeholders::_1, characterDbId, reason));
	}

	void World::sendProxyPacket(DatabaseId characterId, UInt16 opCode, UInt32 size, const std::vector<char> &buffer)
	{
		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::realm_write::clientProxyPacket, std::placeholders::_1, characterId, opCode, size, std::cref(buffer)));
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

		ILOG("World instance ready for character " << characterDbId << ": " << instanceId);

		// Store instance id if not already available
		m_instances.push_back(instanceId);
		std::unique(m_instances.begin(), m_instances.end());

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

	void World::sendChatMessage(NetUInt64 characterGuid, game::ChatMsg type, game::Language lang, const String &receiver, const String &channel, const String &message)
	{
		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::realm_write::chatMessage, std::placeholders::_1, characterGuid, type, lang, std::cref(receiver), std::cref(channel), std::cref(message)));
	}

	void World::handleCharacterData(pp::IncomingPacket &packet)
	{
		// Read the character ID first
		UInt64 characterId;
		if (!(packet >> io::read<NetUInt64>(characterId)))
		{
			return;
		}

		// Find the player using this character
		auto *player = m_playerManager.getPlayerByCharacterId(characterId);
		if (!player)
		{
			// Maybe the player disconnected already - create a temporary character copy and save this copy
			std::unique_ptr<GameCharacter> character(new GameCharacter(
				m_playerManager.getTimers(),
				std::bind(&RaceEntryManager::getById, &m_project.races, std::placeholders::_1),
				std::bind(&ClassEntryManager::getById, &m_project.classes, std::placeholders::_1),
				std::bind(&LevelEntryManager::getById, &m_project.levels, std::placeholders::_1)));
			character->initialize();

			if (!(packet >> *character))
			{
				// Error
				ELOG("Error reading character data");
				return;
			}

			// Save the character data
			m_database.saveGameCharacter(*character);
			return;
		}
		else
		{
			// Update character data
			if (!(packet >> *player->getGameCharacter()))
			{
				// Error
				ELOG("Error reading character data");
				return;
			}

			// Save character
			player->saveCharacter();
		}
	}

}
