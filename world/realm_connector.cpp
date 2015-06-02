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

#include "realm_connector.h"
#include "world_instance_manager.h"
#include "world_instance.h"
#include "player_manager.h"
#include "player.h"
#include "wowpp_protocol/wowpp_world_realm.h"
#include "configuration.h"
#include "common/clock.h"
#include "data/project.h"
#include "binary_io/vector_sink.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	static const auto ReconnectDelay = (constants::OneSecond * 4);
	static const auto KeepAliveDelay = (constants::OneMinute / 2);


	RealmConnector::RealmConnector(
		boost::asio::io_service &ioService, 
		WorldInstanceManager &worldInstanceManager, 
		PlayerManager &playerManager, 
		const Configuration &config, 
		Project &project, 
		TimerQueue &timer
		)
		: m_ioService(ioService)
		, m_worldInstanceManager(worldInstanceManager)
		, m_playerManager(playerManager)
		, m_config(config)
		, m_project(project)
		, m_timer(timer)
		, m_host(m_config.realmAddress)
		, m_port(m_config.realmPort)
		
	{
		tryConnect();
	}

	RealmConnector::~RealmConnector()
	{
	}

	void RealmConnector::connectionLost()
	{
		WLOG("Lost connection with the realm server at " << m_host << ":" << m_port);
		m_connection->resetListener();
		m_connection.reset();
		scheduleConnect();
	}

	void RealmConnector::connectionMalformedPacket()
	{
		WLOG("Received malformed packet from the realm server");
		m_connection->resetListener();
		m_connection.reset();
	}

	void RealmConnector::connectionPacketReceived(pp::Protocol::IncomingPacket &packet)
	{
		const auto &packetId = packet.getId();

		switch (packetId)
		{
			case pp::world_realm::realm_packet::LoginAnswer:
			{
				handleLoginAnswer(packet);
				break;
			}

			case pp::world_realm::realm_packet::CharacterLogIn:
			{
				handleCharacterLogin(packet);
				break;
			}

			case pp::world_realm::realm_packet::ClientProxyPacket:
			{
				handleProxyPacket(packet);
				break;
			}

			default:
			{
				// Log about unknown or unhandled packet
				WLOG("Received unknown packet " << static_cast<UInt32>(packetId)
					<< " from realm server at " << m_host << ":" << m_port);
				break;
			}
		}
	}

	bool RealmConnector::connectionEstablished(bool success)
	{
		if (success)
		{
			ILOG("Connected to the realm server");

			io::StringSink sink(m_connection->getSendBuffer());
			pp::OutgoingPacket packet(sink);

			std::vector<UInt32> instanceIds;;

			// Write packet structure
			pp::world_realm::world_write::login(packet,
				m_config.hostedMaps,
				instanceIds
				);

			m_connection->flush();
			scheduleKeepAlive();
		}
		else
		{
			WLOG("Could not connect to the realm server at " << m_host << ":" << m_port);
			scheduleConnect();
		}

		return true;
	}

	void RealmConnector::scheduleConnect()
	{
		const GameTime now = getCurrentTime();
		m_timer.addEvent(
			std::bind(&RealmConnector::tryConnect, this), now + ReconnectDelay);
	}

	void RealmConnector::tryConnect()
	{
		ILOG("Trying to connect to the realm server..");

		m_connection = pp::Connector::create(m_ioService);
		m_connection->connect(m_host, m_port, *this, m_ioService);
	}

	void RealmConnector::scheduleKeepAlive()
	{
	}

	void RealmConnector::onScheduledKeepAlive()
	{
	}

	void RealmConnector::handleLoginAnswer(pp::Protocol::IncomingPacket &packet)
	{
		using namespace pp::world_realm;

		// Read packet
		UInt32 protocolVersion;
		LoginResult result;
		if (!(realm_read::loginAnswer(packet, protocolVersion, result)))
		{
			return;
		}

		// Check result
		switch (result)
		{
			case login_result::Success:
			{
				ILOG("World node successfully registered at the realm server");
				break;
			}

			case login_result::UnknownMap:
			{
				ELOG("Realm does not know any of the maps we sent!");
				return;
			}

			case login_result::MapsAlreadyInUse:
			{
				ELOG("All maps are already handled by another world server");
				return;
			}

			case login_result::InternalError:
			{
				ELOG("Internal server error at the realm server");
				return;
			}

			default:
			{
				ELOG("Unknown login answer from realm server received: " << result);
				return;
			}
		}
	}

	void RealmConnector::handleCharacterLogin(pp::Protocol::IncomingPacket &packet)
	{
		// Read packet from the realm
		DatabaseId requesterDbId;
		std::shared_ptr<GameCharacter> character(new GameCharacter(
			std::bind(&RaceEntryManager::getById, &m_project.races, std::placeholders::_1),
			std::bind(&ClassEntryManager::getById, &m_project.classes, std::placeholders::_1),
			std::bind(&LevelEntryManager::getById, &m_project.levels, std::placeholders::_1)));
		if (!(pp::world_realm::realm_read::characterLogIn(packet, requesterDbId, character.get())))
		{
			// Error: could not read packet
			return;
		}
		
		float x, y, z, o;
		character->getLocation(x, y, z, o);

		//TODO: Find or create world instance and add player to this instance

		// Let's lookup some informations about the requested map
		auto map = m_project.maps.getById(character->getMapId());
		if (!map)
		{
			ELOG("Unsupported map: " << character->getMapId());
			m_connection->sendSinglePacket(
				std::bind(pp::world_realm::world_write::worldInstanceError, std::placeholders::_1, requesterDbId, pp::world_realm::world_instance_error::UnsupportedMap));
			return;
		}

		// We know the map now - check if this is a global map
		WorldInstance *instance = nullptr;
		if (map->instanceType == map_instance_type::Global)
		{
			// It is a global map instance... look for an instance of this map
			instance = m_worldInstanceManager.getInstanceByMapId(map->id);
			if (!instance)
			{
				// The global world instance for this map does not yet exist - we want to
				// create it
				instance = m_worldInstanceManager.createInstance(*map);
				if (!instance)
				{
					ELOG("Could not create world instance for map " << map->id);
					m_connection->sendSinglePacket(
						std::bind(pp::world_realm::world_write::worldInstanceError, std::placeholders::_1, requesterDbId, pp::world_realm::world_instance_error::InternalError));
					return;
				}
			}
		}
		else
		{
			// It is an instanced map - create a new instance (TODO: Group handling etc.)
			instance = m_worldInstanceManager.createInstance(*map);
			if (!instance)
			{
				ELOG("Could not create new world instance for map " << map->id);
				m_connection->sendSinglePacket(
					std::bind(pp::world_realm::world_write::worldInstanceError, std::placeholders::_1, requesterDbId, pp::world_realm::world_instance_error::InternalError));
				return;
			}
		}

		// Get character location
		UInt32 mapId = map->id;
		UInt32 zoneId = 0x0C;	//TODO

		// Player could be added to the world instance - tell the realm
		assert(instance);
		//instance->addGameObject(character);
		
		// Fire signal which should create a player instance for us
		worldInstanceEntered(requesterDbId, character);

		// Send packet
		m_connection->sendSinglePacket(
			std::bind(
				pp::world_realm::world_write::worldInstanceEntered, 
				std::placeholders::_1, 
				requesterDbId,
				character->getGuid(),
				instance->getId(),
				mapId,
				zoneId,
				x,
				y,
				z,
				o
				));

		// Get all objects in the world and add them
		instance->foreachObject([requesterDbId, this, &character](const GameObject &object)
		{
			// Don't spawn ourself
			if (character->getGuid() == object.getGuid())
			{
				return;
			}

			// Blocks
			std::vector<std::vector<char>> blocks;

			float x, y, z, o;
			object.getLocation(x, y, z, o);

			float cx, cy, cz, co;
			character->getLocation(cx, cy, cz, co);

			// Check 2d distance
			if (sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)) > 80.0f)
			{
				return;
			}

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
						<< io::write<NetUInt32>(662834250);	//TODO: Time

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

				// Add block
				blocks.push_back(createBlock);
			}

			// Send it
			std::vector<char> outBuffer;
			io::VectorSink packetSink(outBuffer);
			game::OutgoingPacket outPacket(packetSink);
			game::server_write::compressedUpdateObject(outPacket, blocks);

			this->m_connection->sendSinglePacket(
				std::bind(pp::world_realm::world_write::clientProxyPacket, std::placeholders::_1, requesterDbId, game::server_packet::CompressedUpdateObject, 0x00, std::cref(outBuffer)));
		});
	}

	void RealmConnector::handleProxyPacket(pp::Protocol::IncomingPacket &packet)
	{
		DatabaseId characterId;
		UInt16 opCode;
		UInt32 size;
		std::vector<char> buffer;
		if (!(pp::world_realm::realm_read::clientProxyPacket(packet, characterId, opCode, size, buffer)))
		{
			return;
		}

		// Setup packet
		io::MemorySource source(reinterpret_cast<const char*>(&buffer[0]), reinterpret_cast<const char*>(&buffer[0]) + size);

		// Create incoming packet structure
		game::IncomingPacket clientPacket;
		clientPacket.setSource(&source);

		// Find the sender
		Player *sender = m_playerManager.getPlayerByCharacterId(characterId);
		if (!sender)
		{
			WLOG("Invalid sender id " << characterId << " for proxy packet 0x" << std::hex << std::uppercase << opCode);
			return;
		}

		// Check op code
		switch (opCode)
		{
			case game::client_packet::NameQuery:
			{
				handleNameQuery(*sender, clientPacket);
				break;
			}

			case game::client_packet::CreatureQuery:
			{
				handleCreatureQuery(*sender, clientPacket);
				break;
			}

			case game::client_packet::PlayerLogout:
			{
				DLOG("TODO: CMSG_PLAYER_LOGOUT not handled.");
				break;
			}

			case game::client_packet::LogoutRequest:
			{
				handleLogoutRequest(*sender, clientPacket);
				break;
			}

			case game::client_packet::LogoutCancel:
			{
				handleLogoutCancel(*sender, clientPacket);
				break;
			}

			default:
			{
				// Unhandled packet
				WLOG("Received client proxy packet: " << opCode << " (Size: " << size << " bytes)");
				break;
			}
		}
	}

	void RealmConnector::handleNameQuery(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		// Object guid
		UInt64 objectGuid;
		if (!game::client_read::nameQuery(packet, objectGuid))
		{
			// Could not read packet
			return;
		}

		// Find the player using the requested guid
		Player *player = m_playerManager.getPlayerByCharacterGuid(objectGuid);
		if (!player)
		{
			WLOG("Could not find requested player object with guid " << objectGuid);
			return;
		}

		// Get the character of this player
		auto character = player->getCharacter();

		// Get some values
		auto raceId = character->getRace();
		auto classId = character->getClass();
		auto genderId = character->getGender();
		const auto &name = character->getName();
		String realmName("");	//TODO

		// Send answer
		sender.sendProxyPacket(
			std::bind(game::server_write::nameQueryResponse, std::placeholders::_1, objectGuid, std::cref(name), std::cref(realmName), raceId, genderId, classId));
	}

	void RealmConnector::sendProxyPacket(DatabaseId senderId, UInt16 opCode, UInt32 size, const std::vector<char> &buffer)
	{
		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::world_write::clientProxyPacket, std::placeholders::_1, senderId, opCode, size, std::cref(buffer)));
	}

	void RealmConnector::notifyWorldInstanceLeft(DatabaseId characterId, pp::world_realm::WorldLeftReason reason)
	{
		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::world_write::worldInstanceLeft, std::placeholders::_1, characterId, reason));
	}

	void RealmConnector::handleCreatureQuery(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		// Read the client packet
		UInt32 creatureEntry;
		UInt64 objectGuid;
		if (!game::client_read::creatureQuery(packet, creatureEntry, objectGuid))
		{
			// Could not read packet
			WLOG("Could not read packet data");
			return;
		}

		//TODO: Find creature object and check if it exists

		//TODO: Find creature info by entry
		const UnitEntry *unit = m_project.units.getById(creatureEntry);
		if (unit)
		{
			// Write answer
			ILOG("WORLD: CMSG_CREATURE_QUERY '" << unit->name << "' - Entry: " << creatureEntry << ".");

			// Write answer packet
			sender.sendProxyPacket(
				std::bind(game::server_write::creatureQueryResponse, std::placeholders::_1, std::cref(*unit)));
		}
		else
		{
			WLOG("WORLD: CMSG_CREATURE_QUERY - Guid: " << objectGuid << " Entry: " << creatureEntry << " NO CREATURE INFO!");

			//TODO: Send resulting packet SMSG_CREATURE_QUERY_RESPONSE with only one uin32 value
			//which is creatureEntry | 0x80000000
		}
	}

	void RealmConnector::handleLogoutRequest(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		// Read client packet
		if (!game::client_read::logoutRequest(packet))
		{
			WLOG("Could not read packet data");
			return;
		}

		//TODO check if the player is allowed to log out (is in combat? is moving? is frozen by gm? etc.)

		// Send answer and engage logout process
		sender.sendProxyPacket(
			std::bind(game::server_write::logoutResponse, std::placeholders::_1, true));

		// Start logout countdown or something? ...
		sender.logoutRequest();
	}

	void RealmConnector::handleLogoutCancel(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		// Read client packet
		if (!game::client_read::logoutCancel(packet))
		{
			WLOG("Could not read packet data");
			return;
		}

		// Notify the client that we have received it's request
		sender.sendProxyPacket(
			std::bind(game::server_write::logoutCancelAck, std::placeholders::_1));

		// Cancel the logout process
		sender.logoutRequest();
	}
}
