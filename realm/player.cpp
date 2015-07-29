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
#include "configuration.h"
#include "database.h"
#include "common/sha1.h"
#include "log/default_log_levels.h"
#include "common/clock.h"
#include "login_connector.h"
#include "world_manager.h"
#include "world.h"
#include "player_social.h"
#include "common/big_number.h"
#include "binary_io/vector_sink.h"
#include "binary_io/writer.h"
#include "data/project.h"
#include "common/utilities.h"
#include "game/game_item.h"
#include "boost/algorithm/string.hpp"
#include "player_group.h"
#include <cassert>
#include <limits>

using namespace std;
using namespace wowpp::game;

namespace wowpp
{
	Player::Player(Configuration &config, PlayerManager &manager, LoginConnector &loginConnector, WorldManager &worldManager, IDatabase &database, Project &project, std::shared_ptr<Client> connection, const String &address)
		: m_config(config)
		, m_manager(manager)
		, m_loginConnector(loginConnector)
		, m_worldManager(worldManager)
		, m_database(database)
		, m_project(project)
		, m_connection(std::move(connection))
		, m_address(address)
		, m_seed(0x3a6833cd)	//TODO: Randomize
        , m_authed(false)
		, m_characterId(std::numeric_limits<DatabaseId>::max())
		, m_instanceId(0x00)
		, m_getRace(std::bind(&RaceEntryManager::getById, &project.races, std::placeholders::_1))
		, m_getClass(std::bind(&ClassEntryManager::getById, &project.classes, std::placeholders::_1))
		, m_getLevel(std::bind(&LevelEntryManager::getById, &project.levels, std::placeholders::_1))
		, m_worldNode(nullptr)
	{
		assert(m_connection);

		m_connection->setListener(*this);
		m_social.reset(new PlayerSocial(m_manager, *this));
	}

	void Player::sendAuthChallenge()
	{
		sendPacket(
			std::bind(game::server_write::authChallenge, std::placeholders::_1, m_seed));
	}

	void Player::loginSucceeded(UInt32 accountId, const BigNumber &key, const BigNumber &v, const BigNumber &s)
	{
		// Check that Key and account name are the same on client and server
		Boost_SHA1HashSink sha;

		UInt32 t = 0;
		
		sha.write(m_accountName.c_str(), m_accountName.size());
		sha.write(reinterpret_cast<const char*>(&t), 4);
		sha.write(reinterpret_cast<const char*>(&m_clientSeed), 4);
		sha.write(reinterpret_cast<const char*>(&m_seed), 4);
		auto keyBuffer = key.asByteArray();
		sha.write(reinterpret_cast<const char*>(keyBuffer.data()), keyBuffer.size());
		SHA1Hash digest = sha.finalizeHash();

		if (digest != m_clientHash)
		{
			// AUTH_FAILED
			return;
		}

		m_accountId = accountId;
		m_sessionKey = key;
		m_v = v;
		m_s = s;

		//TODO Create session

		ILOG("Client " << m_accountName << " authenticated successfully from " << m_address);
		m_authed = true;

		// Notify login connector
		m_loginConnector.notifyPlayerLogin(m_accountId);

		// Initialize crypt
		game::Connection *cryptCon = static_cast<game::Connection*>(m_connection.get());
		game::Crypt &crypt = cryptCon->getCrypt();
		
		//For BC
		HMACHash cryptKey;
		crypt.generateKey(cryptKey, m_sessionKey);
		crypt.setKey(cryptKey.data(), cryptKey.size());
		crypt.init();

		//TODO: Load tutorials data

		// Send response code: AuthOk
		sendPacket(
			std::bind(game::server_write::authResponse, std::placeholders::_1, game::response_code::AuthOk, game::expansions::TheBurningCrusade));

		// Send addon proof packet
		sendPacket(
			std::bind(game::server_write::addonInfo, std::placeholders::_1, std::cref(m_addons)));
	}

	void Player::loginFailed()
	{
		// Log in process failed - disconnect the client
		destroy();
	}

	game::CharEntry * Player::getCharacterById(DatabaseId databaseId)
	{
		auto c = std::find_if(
			m_characters.begin(),
			m_characters.end(),
			[databaseId](const game::CharEntry &c)
		{
			return (databaseId == c.id);
		});

		if (c != m_characters.end())
		{
			return &(*c);
		}

		return nullptr;
	}

	void Player::worldNodeDisconnected()
	{
		// Disconnect the player client
		destroy();
	}

	void Player::destroy()
	{
		m_connection->resetListener();
		m_connection->close();
		m_connection.reset();

		m_manager.playerDisconnected(*this);
	}

	void Player::connectionLost()
	{
		ILOG("Client " << m_address << " disconnected");

		// If we are logged in, notify the world node about this
		if (m_gameCharacter)
		{
			// Send notification to friends
			game::SocialInfo info;
			info.flags = game::social_flag::Friend;
			info.status = game::friend_status::Offline;
			m_social->sendToFriends(
				std::bind(game::server_write::friendStatus, std::placeholders::_1, m_gameCharacter->getGuid(), game::friend_result::Offline, std::cref(info)));

			// Try to find the world node
			auto world = m_worldManager.getWorldByInstanceId(m_instanceId);
			if (world)
			{
				world->leaveWorldInstance(m_characterId, pp::world_realm::world_left_reason::Disconnect);
				ILOG("Sent notification about this to the world node.");
			}
			else
			{
				WLOG("Failed to find the world node - can't send disconnect notification.");
			}
		}

		destroy();
	}

	void Player::connectionMalformedPacket()
	{
		ILOG("Client " << m_address << " sent malformed packet");
		destroy();
	}

	bool Player::isSessionStatusValid(game::SessionStatus status, bool verbose /*= false*/) const
	{
		switch (status)
		{
			case game::session_status::Never:
			{
				if (verbose) WLOG("Packet isn't handled on the server side!");
				return false;
			}

			case game::session_status::Connected:
			{
				if (m_authed && verbose) WLOG("Packet is only handled if not yet authenticated!");
				return !m_authed;
			}

			case game::session_status::Authentificated:
			{
				if (!m_authed && verbose) WLOG("Packet is only handled if the player is authenticated!");
				return m_authed;
			}

			case game::session_status::LoggedIn:
			{
				if (!m_worldNode && verbose) WLOG("Packet is only handled if the player is logged in!");
				return (m_worldNode != nullptr);
			}

			case game::session_status::TransferPending:
			{
				if ((m_characterId == 0 || m_worldNode != nullptr) && verbose)
				{
					WLOG("Packet is only handled if a transfer is pending!");
				}

				return (m_characterId != 0 && m_worldNode == nullptr);
			}

			default:
			{
				// Includes game::session_status::Always
				return true;
			}
		}
	}

	void Player::connectionPacketReceived(game::IncomingPacket &packet)
	{
		// Decrypt position
		io::MemorySource *memorySource = static_cast<io::MemorySource*>(packet.getSource());

		const auto packetId = packet.getId();
		switch (packetId)
		{
#define WOWPP_HANDLE_PACKET(name, sessionStatus) \
			case wowpp::game::client_packet::name: \
			{ \
				if (!isSessionStatusValid(sessionStatus, true)) \
					break; \
				handle##name(packet); \
				break; \
			}

			WOWPP_HANDLE_PACKET(Ping, game::session_status::Always)
			WOWPP_HANDLE_PACKET(AuthSession, game::session_status::Connected)
			WOWPP_HANDLE_PACKET(CharEnum, game::session_status::Authentificated)
			WOWPP_HANDLE_PACKET(CharCreate, game::session_status::Authentificated)
			WOWPP_HANDLE_PACKET(CharDelete, game::session_status::Authentificated)
			WOWPP_HANDLE_PACKET(PlayerLogin, game::session_status::Authentificated)
			WOWPP_HANDLE_PACKET(MessageChat, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(NameQuery, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(ContactList, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(AddFriend, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(DeleteFriend, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(AddIgnore, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(DeleteIgnore, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(ItemQuerySingle, game::session_status::LoggedIn)

			WOWPP_HANDLE_PACKET(GroupInvite, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(GroupAccept, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(GroupDecline, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(GroupUninvite, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(GroupUninviteGUID, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(GroupSetLeader, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(LootMethod, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(GroupDisband, game::session_status::LoggedIn)
#undef WOWPP_HANDLE_PACKET

			default:
			{
				// Redirect to world server if attached
				if (m_gameCharacter)
				{
					// Redirect packet as proxy packet
					auto world = m_worldManager.getWorldByInstanceId(m_instanceId);
					if (world)
					{
						// Buffer
						const std::vector<char> packetBuffer(memorySource->getBegin(), memorySource->getEnd());
						world->sendProxyPacket(m_characterId, packetId, packetBuffer.size(), packetBuffer);
						break;
					}
				}

				// Log warning
				WLOG("Unknown packet received from " << m_address
					<< " - ID: " << static_cast<UInt32>(packetId)
					<< "; Size: " << packet.getSource()->size() << " bytes");
				break;
			}
		}
	}

	void Player::handlePing(game::IncomingPacket &packet)
	{
		UInt32 ping, latency;
		if (!game::client_read::ping(packet, ping, latency))
		{
			return;
		}

		// Send pong
		sendPacket(
			std::bind(game::server_write::pong, std::placeholders::_1, ping));
	}

	void Player::handleAuthSession(game::IncomingPacket &packet)
	{
		// Clear addon list
		m_addons.clear();

		UInt32 clientBuild;
		if (!game::client_read::authSession(packet, clientBuild, m_accountName, m_clientSeed, m_clientHash, m_addons))
		{
			return;
		}

		// Check if the client version is valid: At the moment, we only support
		// burning crusade (2.4.3)
		if (clientBuild != 8606)
		{
			//TODO Send error result
			WLOG("Client " << m_address << " tried to login with unsupported client build " << clientBuild);
			return;
		}

		// Ask the login server if this login is okay and also ask for session key etc.
		if (!m_loginConnector.playerLoginRequest(m_accountName))
		{
			// Could not send player login request
			return;
		}
	}

	void Player::handleCharEnum(game::IncomingPacket &packet)
	{
		if (!(game::client_read::charEnum(packet)))
		{
			return;
		}

		// TODO: Flood protection

		// Load characters
		m_characters.clear();
		if (!m_database.getCharacters(m_accountId, m_characters))
		{
			// Disconnect
			destroy();
			return;
		}

		for (auto &c : m_characters)
		{
			c.id = createRealmGUID(guidLowerPart(c.id), m_loginConnector.getRealmID(), guid_type::Player);
		}

		// Send character list
		sendPacket(
			std::bind(game::server_write::charEnum, std::placeholders::_1, std::cref(m_characters)));
	}

	void Player::handleCharCreate(game::IncomingPacket &packet)
	{
		game::CharEntry character;
		if (!(game::client_read::charCreate(packet, character)))
		{
			return;
		}

		// Empty character name?
		character.name = std::move(trim(character.name));
		if (character.name.empty())
		{
			sendPacket(
				std::bind(game::server_write::charCreate, std::placeholders::_1, game::response_code::CharCreateError));
			return;
		}

		// TODO: Check for invalid characters (numbers, white spaces etc.)

		// Capitalize the characters name
		capitalize(character.name);

		// Get number of characters on this account
		const UInt32 maxCharacters = 11;
		UInt32 numCharacters = m_database.getCharacterCount(m_accountId);
		if (numCharacters >= maxCharacters)
		{
			// No more free slots
			sendPacket(
				std::bind(game::server_write::charCreate, std::placeholders::_1, game::response_code::CharCreateServerLimit));
			return;
		}

		// Get the racial informations
		const auto *race = m_getRace(character.race);
		if (!race)
		{
			ELOG("Unable to find informations of race " << character.race);
			sendPacket(
				std::bind(game::server_write::charCreate, std::placeholders::_1, game::response_code::CharCreateError));
			return;
		}

		// Add initial spells
		const auto &initialSpellsEntry = race->initialSpells.find(character.class_);
		if (initialSpellsEntry == race->initialSpells.end())
		{
			ELOG("No initial spells set up for race " << race->name << " and class " << character.class_);
			sendPacket(
				std::bind(game::server_write::charCreate, std::placeholders::_1, game::response_code::CharCreateError));
			return;
		}

		// Item data
		UInt8 bagSlot = 0;
		std::vector<pp::world_realm::ItemData> items;

		// Add initial items
		auto it1 = race->initialItems.find(character.class_);
		if (it1 != race->initialItems.end())
		{
			auto it2 = it1->second.find(character.gender);
			if (it2 != it1->second.end())
			{
				for (const auto *item : it2->second)
				{
					// Get slot for this item
					UInt16 slot = 0xffff;
					switch (item->inventoryType)
					{
						case inventory_type::Head:
						{
							slot = player_equipment_slots::Head;
							break;
						}
						case inventory_type::Neck:
						{
							slot = player_equipment_slots::Neck;
							break;
						}
						case inventory_type::Shoulders:
						{
							slot = player_equipment_slots::Shoulders;
							break;
						}
						case inventory_type::Body:
						{
							slot = player_equipment_slots::Body;
							break;
						}
						case inventory_type::Chest:
						case inventory_type::Robe:
						{
							slot = player_equipment_slots::Chest;
							break;
						}
						case inventory_type::Waist:
						{
							slot = player_equipment_slots::Waist;
							break;
						}
						case inventory_type::Legs:
						{
							slot = player_equipment_slots::Legs;
							break;
						}
						case inventory_type::Feet:
						{
							slot = player_equipment_slots::Feet;
							break;
						}
						case inventory_type::Wrists:
						{
							slot = player_equipment_slots::Wrists;
							break;
						}
						case inventory_type::Hands:
						{
							slot = player_equipment_slots::Hands;
							break;
						}
						case inventory_type::Finger:
						{
							//TODO: Finger1/2
							slot = player_equipment_slots::Finger1;
							break;
						}
						case inventory_type::Trinket:
						{
							//TODO: Trinket1/2
							slot = player_equipment_slots::Trinket1;
							break;
						}
						case inventory_type::Weapon:
						case inventory_type::TwoHandWeapon:
						case inventory_type::WeaponMainHand:
						{
							slot = player_equipment_slots::Mainhand;
							break;
						}
						case inventory_type::Shield:
						case inventory_type::WeaponOffHand:
						case inventory_type::Holdable:
						{
							slot = player_equipment_slots::Offhand;
							break;
						}
						case inventory_type::Ranged:
						case inventory_type::Thrown:
						{
							slot = player_equipment_slots::Ranged;
							break;
						}
						case inventory_type::Cloak:
						{
							slot = player_equipment_slots::Back;
							break;
						}
						case inventory_type::Tabard:
						{
							slot = player_equipment_slots::Tabard;
							break;
						}

						default:
						{
							if (bagSlot < player_inventory_pack_slots::Count_)
							{
								slot = player_inventory_pack_slots::Start + (bagSlot++);
							}
							break;
						}
					}

					if (slot != 0xffff)
					{
						pp::world_realm::ItemData itemData;
						itemData.entry = item->id;
						itemData.durability = item->durability;
						itemData.slot = slot;
						itemData.stackCount = 1;
						items.emplace_back(std::move(itemData));
					}
				}
			}
		}

		// Update character location
		character.mapId = race->startMap;
		character.zoneId = race->startZone;
		character.x = race->startPosition[0];
		character.y = race->startPosition[1];
		character.z = race->startPosition[2];
		character.o = race->startRotation;

		// Create character
		game::ResponseCode result = m_database.createCharacter(m_accountId, initialSpellsEntry->second, items, character);
		if (result == game::response_code::CharCreateSuccess)
		{
			// Cache the character data
			m_characters.push_back(character);
		}

		// Send disabled message for now
		sendPacket(
			std::bind(game::server_write::charCreate, std::placeholders::_1, result));
	}

	void Player::handleCharDelete(game::IncomingPacket &packet)
	{
		// Read packet
		DatabaseId characterId;
		if (!(game::client_read::charDelete(packet, characterId)))
		{
			return;
		}

		// Try to remove character from the cache
		const auto c = std::find_if(
			m_characters.begin(),
			m_characters.end(),
			[characterId](const game::CharEntry &c)
		{
			return (characterId == c.id);
		});

		// Did we find him?
		if (c == m_characters.end())
		{
			// We didn't find him
			WLOG("Unable to delete character " << characterId << " of user " << m_accountName << ": Not found");
			sendPacket(
				std::bind(game::server_write::charDelete, std::placeholders::_1, game::response_code::CharDeleteFailed));
			return;
		}

		// Remove character from cache
		m_characters.erase(c);

		// Delete from database
		game::ResponseCode result = m_database.deleteCharacter(m_accountId, characterId);

		// Send disabled message for now
		sendPacket(
			std::bind(game::server_write::charDelete, std::placeholders::_1, result));
	}

	void Player::handlePlayerLogin(game::IncomingPacket &packet)
	{
		// Get the character id with which the player wants to enter the world
		DatabaseId characterId;
		if (!(game::client_read::playerLogin(packet, characterId)))
		{
			return;
		}

		// Check if the requested character belongs to our account
		game::CharEntry *charEntry = getCharacterById(characterId);
		if (!charEntry)
		{
			// It seems like we don't own the requested character
			WLOG("Requested character id " << characterId << " does not belong to account " << m_accountId << " or does not exist");
			sendPacket(
				std::bind(game::server_write::charLoginFailed, std::placeholders::_1, game::response_code::CharLoginNoCharacter));
			return;
		}

		// Store character id
		m_characterId = characterId;

		// Write something to the log just for informations
		ILOG("Player " << m_accountName << " tries to enter the world with character 0x" << std::hex << std::setw(16) << std::setfill('0') << std::uppercase << m_characterId);

		// Store character items
		std::vector<pp::world_realm::ItemData> items;

		// Load the player character data from the database
		std::unique_ptr<GameCharacter> character(new GameCharacter(m_manager.getTimers(), m_getRace, m_getClass, m_getLevel));
		character->initialize();
		character->setGuid(createRealmGUID(characterId, m_loginConnector.getRealmID(), guid_type::Player));
		if (!m_database.getGameCharacter(guidLowerPart(characterId), *character, items))
		{
			// Send error packet
			WLOG("Player login failed: Could not load character " << characterId);
			sendPacket(
				std::bind(game::server_write::charLoginFailed, std::placeholders::_1, game::response_code::CharLoginNoCharacter));
			return;
		}

		// We found the character - now we need to look for a world node
		// which is hosting a fitting world instance or is able to create
		// a new one

		m_worldNode = m_worldManager.getWorldByMapId(charEntry->mapId);
		if (!m_worldNode)
		{
			// World does not exist
			WLOG("Player login failed: Could not find world server for map " << charEntry->mapId);
			sendPacket(
				std::bind(game::server_write::charLoginFailed, std::placeholders::_1, game::response_code::CharLoginNoWorld));
			return;
		}

		// Use the new character
		m_gameCharacter = std::move(character);
		m_gameCharacter->setZone(charEntry->zoneId);

		// Load the social list
		m_social.reset(new PlayerSocial(m_manager, *this));
		m_database.getCharacterSocialList(m_characterId, *m_social);

		//TODO Map found - check if player is member of a group and if this instance
		// is valid on the world node and if not, transfer player

		// There should be an instance
		m_worldNode->enterWorldInstance(charEntry->id, *m_gameCharacter, items);
	}

	void Player::worldInstanceEntered(World &world, UInt32 instanceId, UInt64 worldObjectGuid, UInt32 mapId, UInt32 zoneId, float x, float y, float z, float o)
	{
		assert(m_gameCharacter);

		// Watch for world node disconnection
		m_worldDisconnected = world.onConnectionLost.connect(
			std::bind(&Player::worldNodeDisconnected, this));

		// Save instance id
		m_instanceId = instanceId;
		
		// Update character on the realm side with data received from the world server
		//m_gameCharacter->setGuid(createGUID(worldObjectGuid, 0, high_guid::Player));
		m_gameCharacter->relocate(x, y, z, o);
		m_gameCharacter->setMapId(mapId);

		// Clear mask
		m_gameCharacter->clearUpdateMask();

		sendPacket(
			std::bind(game::server_write::setDungeonDifficulty, std::placeholders::_1));

		// Send world verification packet to the client to proof world coordinates from
		// the character list
		sendPacket(
			std::bind(game::server_write::loginVerifyWorld, std::placeholders::_1, mapId, x, y, z, o));

		// Send account data times (TODO: Find out what this does)
		std::array<UInt32, 32> times;
		times.fill(0);
		sendPacket(
			std::bind(game::server_write::accountDataTimes, std::placeholders::_1, std::cref(times)));

		// SMSG_FEATURE_SYSTEM_STATUS 
		sendPacket(
			std::bind(game::server_write::featureSystemStatus, std::placeholders::_1));

		// SMSG_MOTD 
		sendPacket(
			std::bind(game::server_write::motd, std::placeholders::_1, m_config.messageOfTheDay));

		// Don't know what this packet does
		sendPacket(
			std::bind(game::server_write::setRestStart, std::placeholders::_1));

		// Notify about bind point for hearthstone (also used in case of corrupted location data)
		sendPacket(
			std::bind(game::server_write::bindPointUpdate, std::placeholders::_1, mapId, zoneId, x, y, z));
		
		// Send tutorial flags (which tutorials have been viewed etc.)
		sendPacket(
			std::bind(game::server_write::tutorialFlags, std::placeholders::_1));
		
		// Send spells
		const auto &spells = m_gameCharacter->getSpells();
		sendPacket(
			std::bind(game::server_write::initialSpells, std::placeholders::_1, std::cref(spells)));

		sendPacket(
			std::bind(game::server_write::unlearnSpells, std::placeholders::_1));

		auto raceEntry = m_gameCharacter->getRaceEntry();
		assert(raceEntry);

		auto cls = m_gameCharacter->getClass();
		auto classBtns = raceEntry->initialActionButtons.find(cls);
		if (classBtns != raceEntry->initialActionButtons.end())
		{
			sendPacket(
				std::bind(game::server_write::actionButtons, std::placeholders::_1, std::cref(classBtns->second)));
		}
		
		sendPacket(
			std::bind(game::server_write::initializeFactions, std::placeholders::_1));

		// Trigger intro cinematic based on the characters race
		game::CharEntry *charEntry = getCharacterById(m_characterId);
		if (raceEntry &&
			(charEntry && charEntry->cinematic))
		{
			sendPacket(
				std::bind(game::server_write::triggerCinematic, std::placeholders::_1, raceEntry->cinematic));
		}

		// Send notification to friends
		game::SocialInfo info;
		info.flags = game::social_flag::Friend;
		info.area = charEntry->zoneId;
		info.level = charEntry->level;
		info.class_ = charEntry->class_;
		info.status = game::friend_status::Online;
		m_social->sendToFriends(
			std::bind(game::server_write::friendStatus, std::placeholders::_1, m_gameCharacter->getGuid(), game::friend_result::Online, std::cref(info)));
	}

	void Player::worldInstanceLeft(World &world, UInt32 instanceId, pp::world_realm::WorldLeftReason reason)
	{
		// Display world instance left reason
		String reasonString = "UNKNOWN";
		switch (reason)
		{
			case pp::world_realm::world_left_reason::Logout:
			{
				reasonString = "LOGOUT";
				break;
			}

			case pp::world_realm::world_left_reason::Teleport:
			{
				reasonString = "TELEPORT";
				break;
			}

			default:
			{
				break;
			}
		}

		// Write something to the log just for informations
		ILOG("Player " << m_accountName << " left world instance " << m_instanceId << " - reason: " << reasonString);

		switch (reason)
		{
			case pp::world_realm::world_left_reason::Logout:
			{
				// We no longer care about the world node
				m_worldDisconnected.disconnect();

				// Send notification to friends
				game::SocialInfo info;
				info.flags = game::social_flag::Friend;
				info.status = game::friend_status::Offline;
				m_social->sendToFriends(
					std::bind(game::server_write::friendStatus, std::placeholders::_1, m_gameCharacter->getGuid(), game::friend_result::Offline, std::cref(info)));

				// Clear social list
				m_social.reset(new PlayerSocial(m_manager, *this));

				// Notify the client that the logout process is done
				sendPacket(
					std::bind(game::server_write::logoutComplete, std::placeholders::_1));

				// We are now longer signed int
				m_gameCharacter.reset();
				m_characterId = 0;
				m_instanceId = 0;
				m_worldNode = nullptr;

				break;
			}

			default:
			{
				// Unknown reason?
				WLOG("Player left world instance for unknown reason...")
				break;
			}
		}
	}

	void Player::sendProxyPacket(UInt16 opCode, const std::vector<char> &buffer)
	{
		// Write native packet
		wowpp::Buffer &sendBuffer = m_connection->getSendBuffer();
		io::StringSink sink(sendBuffer);

		// Get the end of the buffer (needed for encryption)
		size_t bufferPos = sink.position();

		game::Protocol::OutgoingPacket packet(sink, true);
		packet.start(opCode);
		packet
			<< io::write_range(buffer);
		packet.finish();

		// Crypt packet header
		game::Connection *cryptCon = static_cast<game::Connection*>(m_connection.get());
		cryptCon->getCrypt().encryptSend(reinterpret_cast<UInt8*>(&sendBuffer[bufferPos]), game::Crypt::CryptedSendLength);

		// Flush buffers
		m_connection->flush();
	}

	void Player::handleNameQuery(game::IncomingPacket &packet)
	{
		// Object guid
		UInt64 objectGuid;
		if (!game::client_read::nameQuery(packet, objectGuid))
		{
			// Could not read packet
			return;
		}

		// Get the realm ID out of this GUID and check if this is a player
		UInt32 realmID = guidRealmID(objectGuid);
		if (realmID != m_loginConnector.getRealmID())
		{
			// Redirect the request to the current world node
			auto world = m_worldManager.getWorldByInstanceId(m_instanceId);
			if (world)
			{
				// Decrypt position
				io::MemorySource *memorySource = static_cast<io::MemorySource*>(packet.getSource());

				// Buffer
				const std::vector<char> packetBuffer(memorySource->getBegin(), memorySource->getEnd());
				world->sendProxyPacket(m_characterId, game::client_packet::NameQuery, packetBuffer.size(), packetBuffer);
				return;
			}
			else
			{
				WLOG("Could not get world node to redirect name query request!");
			}
		}
		else
		{
			// Get the character db id
			UInt32 databaseID = guidLowerPart(objectGuid);

			// Look for the specified player
			game::CharEntry entry;
			if (!m_database.getCharacterById(databaseID, entry))
			{
				WLOG("Could not resolve name for player guid " << databaseID);
				return;
			}

			// Our realm name
			const String realmName("");

			// Send answer
			sendPacket(
				std::bind(game::server_write::nameQueryResponse, std::placeholders::_1, objectGuid, std::cref(entry.name), std::cref(realmName), entry.race, entry.gender, entry.class_));
		}
	}

	void Player::handleMessageChat(game::IncomingPacket &packet)
	{
		ChatMsg type;
		Language lang;
		String receiver, channel, message;
		if (!client_read::messageChat(packet, type, lang, receiver, channel, message))
		{
			// Error reading packet
			return;
		}

		if (!receiver.empty())
			capitalize(receiver);

		switch (type)
		{
			// Local chat modes
			case chat_msg::Say:
			case chat_msg::Yell:
			{
				// Redirect chat message to world node
				m_worldNode->sendChatMessage(
					m_gameCharacter->getGuid(),
					type,
					lang,
					receiver,
					channel,
					message);
				break;
			}
			// 
			case chat_msg::Whisper:
			{
				// Try to extract the realm name
				std::vector<String> nameElements;
				split(receiver, '-', nameElements);

				// Get the realm name (lower case)
				String realmName = m_config.internalName;
				boost::algorithm::to_lower(realmName);
			
				// Check if a realm name was provided
				if (nameElements.size() > 1)
				{
					String targetRealm = nameElements[1];
					boost::algorithm::to_lower(targetRealm);

					// There is a realm name - check if it is this realm
					if (targetRealm != realmName)
					{
						// It is another realm - redirect to the world node
						WLOG("TODO: Redirect whisper message to the world node");
						m_worldNode->sendChatMessage(
							m_gameCharacter->getGuid(),
							type,
							lang,
							receiver,
							channel,
							message);
						return;
					}
					else
					{
						receiver = nameElements[0];
						capitalize(receiver);
					}
				}

				// Get player guid by name
				game::CharEntry entry;
				if (!m_database.getCharacterByName(receiver, entry))
				{
					sendPacket(
						std::bind(game::server_write::chatPlayerNotFound, std::placeholders::_1, std::cref(receiver)));
					break;
				}

				// Make realm GUID
				UInt64 guid = createRealmGUID(entry.id, m_loginConnector.getRealmID(), guid_type::Player);

				// Check if that player is online right now
				Player *other = m_manager.getPlayerByCharacterGuid(guid);
				if (!other)
				{
					sendPacket(
						std::bind(game::server_write::chatPlayerNotFound, std::placeholders::_1, std::cref(receiver)));
					break;
				}

				// TODO: Check if that player is a GM and if he accepts whispers from us, eventually block

				// Change language if needed so that whispers are always readable
				if (lang != game::language::Addon) lang = game::language::Universal;

				// Send whisper message
				other->sendPacket(
					std::bind(game::server_write::messageChat, std::placeholders::_1, chat_msg::Whisper, lang, std::cref(channel), m_characterId, std::cref(message), m_gameCharacter.get()));

				// If not an addon message, send reply message
				if (lang != game::language::Addon)
				{
					sendPacket(
						std::bind(game::server_write::messageChat, std::placeholders::_1, chat_msg::Reply, lang, std::cref(channel), guid, std::cref(message), m_gameCharacter.get()));
				}

				break;
			}
			case chat_msg::Party:
			{
				// Get the players group
				if (!m_group)
				{
					WLOG("Player is not in group");
					return;
				}

				// Maybe we were just invited, but are not yet a member of that group
				if (!m_group->isMember(*m_gameCharacter))
				{
					WLOG("Player is not a member of the group, but was just invited.");
					return;
				}

				// Broadcast chat packet
				m_group->broadcastPacket(
					std::bind(game::server_write::messageChat, std::placeholders::_1, chat_msg::Party, lang, std::cref(channel), m_characterId, std::cref(message), m_gameCharacter.get()));

				break;
			}
			// Can be local or global chat mode
			case chat_msg::Channel:
			{
				WLOG("Channel Chat mode not yet implemented");
				break;
			}
			// Global chat modes / other
			default:
			{
				WLOG("Chat mode not yet implemented");
				break;
			}
		}
	}

	void Player::handleContactList(game::IncomingPacket &packet)
	{
		if (!client_read::contactList(packet))
		{
			// Error reading packet
			return;
		}

		// TODO: Only update the friend list after a specific time interval to prevent 
		// spamming of this command

		// Send the social list
		m_social->sendSocialList();
	}

	void Player::handleAddFriend(game::IncomingPacket &packet)
	{
		String name, note;
		if (!client_read::addFriend(packet, name, note))
		{
			// Error reading packet
			return;
		}

		if (name.empty())
		{
			WLOG("Received empty name in CMSG_ADD_FRIEND packet!");
			return;
		}

		// Convert name
		if (!name.empty())
			capitalize(name);

		// Find the character details
		game::CharEntry friendChar;
		if (!m_database.getCharacterByName(name, friendChar))
		{
			WLOG("Could not find that character");
			return;
		}

		// Create the characters guid value
		UInt64 characterGUID = createRealmGUID(friendChar.id, m_loginConnector.getRealmID(), guid_type::Player);
		
		// Fill friend info
		game::SocialInfo info;
		info.flags = game::social_flag::Friend;
		info.area = friendChar.zoneId;
		info.level = friendChar.level;
		info.class_ = friendChar.class_;
		info.note = std::move(note);

		// Result code
		game::FriendResult result = game::friend_result::AddedOffline;
		if (characterGUID == m_characterId)
		{
			result = game::friend_result::Self;
		}
		else
		{
			// Add to social list
			result = m_social->addToSocialList(characterGUID, false);
			if (result == game::friend_result::AddedOffline)
			{
				// Add to database
				const bool shouldUpdate = m_social->isIgnored(characterGUID);
				if (!m_database.addCharacterSocialContact(m_characterId, characterGUID, static_cast<game::SocialFlag>(info.flags), info.note))
				{
					result = game::friend_result::DatabaseError;
				}
			}
		}

		// Check if the player is online
		Player *friendPlayer = m_manager.getPlayerByCharacterGuid(characterGUID);
		info.status = friendPlayer ? game::friend_status::Online : game::friend_status::Offline;
		if (result == game::friend_result::AddedOffline &&
			friendPlayer != nullptr)
		{
			result = game::friend_result::AddedOnline;
		}

		sendPacket(
			std::bind(game::server_write::friendStatus, std::placeholders::_1, characterGUID, result, std::cref(info)));
	}

	void Player::handleDeleteFriend(game::IncomingPacket &packet)
	{
		UInt64 guid;
		if (!client_read::deleteFriend(packet, guid))
		{
			// Error reading packet
			return;
		}

		// Remove that friend from our social list
		auto result = m_social->removeFromSocialList(guid, false);
		if (result == game::friend_result::Removed)
		{
			if (!m_database.removeCharacterSocialContact(m_characterId, guid))
			{
				result = game::friend_result::DatabaseError;
			}
		}

		game::SocialInfo info;
		sendPacket(
			std::bind(game::server_write::friendStatus, std::placeholders::_1, guid, result, std::cref(info)));
	}

	void Player::handleAddIgnore(game::IncomingPacket &packet)
	{
		String name;
		if (!client_read::addIgnore(packet, name))
		{
			// Error reading packet
			return;
		}

		if (name.empty())
		{
			WLOG("Received empty name in CMSG_ADD_IGNORE packet!");
			return;
		}

		// Convert name
		if (!name.empty())
			capitalize(name);

		DLOG("TODO: Player " << m_accountName << " wants to add " << name << " to his ignore list");
	}

	void Player::handleDeleteIgnore(game::IncomingPacket &packet)
	{
		UInt64 guid;
		if (!client_read::deleteIgnore(packet, guid))
		{
			// Error reading packet
			return;
		}

		DLOG("TODO: Player " << m_accountName << " wants to delete " << guid << " from his ignore list");
	}

	void Player::handleItemQuerySingle(game::IncomingPacket &packet)
	{
		// Object guid
		UInt32 itemID;
		if (!game::client_read::itemQuerySingle(packet, itemID))
		{
			// Could not read packet
			return;
		}

		// Find item
		const ItemEntry *item = m_project.items.getById(itemID);
		if (item)
		{
			// Write answer
			ILOG("WORLD: CMSG_ITEM_QUERY_SINGLE '" << item->name << "' - Entry: " << itemID << ".");

			// TODO: Cache multiple query requests and send one, bigger response with multiple items

			// Write answer packet
			sendPacket(
				std::bind(game::server_write::itemQuerySingleResponse, std::placeholders::_1, std::cref(*item)));
		}
		else
		{
			WLOG("WORLD: CMSG_ITEM_QUERY_SINGLE - Entry: " << itemID << " NO ITEM INFO!");
		}
	}

	void Player::saveCharacter()
	{
		if (m_gameCharacter)
		{
			float x, y, z, o;
			m_gameCharacter->getLocation(x, y, z, o);

			DLOG("Saving character. Position: " << x << "," << y << "," << z << "," << o);
			m_database.saveGameCharacter(*m_gameCharacter);
		}
	}

	void Player::handleGroupInvite(game::IncomingPacket &packet)
	{
		String playerName;
		if (!game::client_read::groupInvite(packet, playerName))
		{
			// Could not read packet
			return;
		}

		// Capitalize player name
		capitalize(playerName);

		// Try to find that player
		auto *player = m_manager.getPlayerByCharacterName(playerName);
		if (!player)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, party_operation::Invite, std::cref(playerName), party_result::CantFindTarget));
			return;
		}

		// Check that players character faction
		auto *character = player->getGameCharacter();
		if (!character)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, party_operation::Invite, std::cref(playerName), party_result::CantFindTarget));
			return;
		}

		// Check team (no cross-faction groups)
		const bool isAllianceA = ((game::race::Alliance & (1 << (m_gameCharacter->getRace() - 1))) == (1 << (m_gameCharacter->getRace() - 1)));
		const bool isAllianceB = ((game::race::Alliance & (1 << (character->getRace() - 1))) == (1 << (character->getRace() - 1)));
		if (isAllianceA != isAllianceB)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, party_operation::Invite, std::cref(playerName), party_result::TargetUnfriendly));
			return;
		}

		// Check if target is already member of a group
		if (player->getGroup())
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, party_operation::Invite, std::cref(playerName), party_result::AlreadyInGroup));
			return;
		}

		DLOG("CMSG_GROUP_INVITE: Player " << m_gameCharacter->getName() << " invites player " << playerName);

		// Get players group or create a new one
		if (!m_group)
		{
			// Create the group
			m_group = std::make_shared<PlayerGroup>(m_manager);
			m_group->create(*m_gameCharacter);
		}

		// Check if we are the leader of that group
		if (m_group->getLeader() != m_gameCharacter->getGuid())
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, party_operation::Invite, "", party_result::YouNotLeader));
			return;
		}

		// Invite player to the group
		auto result = m_group->addInvite(character->getGuid());
		if (result != game::party_result::Ok)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, party_operation::Invite, "", result));
			return;
		}

		player->setGroup(m_group);
		player->sendPacket(
			std::bind(game::server_write::groupInvite, std::placeholders::_1, std::cref(m_gameCharacter->getName())));

		// Send result
		sendPacket(
			std::bind(game::server_write::partyCommandResult, std::placeholders::_1, party_operation::Invite, std::cref(playerName), party_result::Ok));
		m_group->sendUpdate();
	}

	void Player::handleGroupAccept(game::IncomingPacket &packet)
	{
		if (!game::client_read::groupAccept(packet))
		{
			// Could not read packet
			return;
		}

		if (!m_group)
		{
			WLOG("Player accepted group invitation, but is not in a group");
			return;
		}

		auto result = m_group->addMember(*m_gameCharacter);
		if (result != party_result::Ok)
		{
			// TODO...
			return;
		}

		DLOG("CMSG_GROUP_ACCEPT: Player " << m_gameCharacter->getName() << " accepts group invite");
		m_group->sendUpdate();
	}

	void Player::handleGroupDecline(game::IncomingPacket &packet)
	{
		if (!game::client_read::groupDecline(packet))
		{
			// Could not read packet
			return;
		}

		DLOG("CMSG_GROUP_DECLINE: Player " << m_gameCharacter->getName() << " declines group invite");
	}

	void Player::handleGroupUninvite(game::IncomingPacket &packet)
	{
		String memberName;
		if (!game::client_read::groupUninvite(packet, memberName))
		{
			// Could not read packet
			return;
		}

		// Capitalize player name
		capitalize(memberName);

		DLOG("CMSG_GROUP_UNINVITE: Player " << m_gameCharacter->getName() << " want's to uninvite member " << memberName);
	}

	void Player::handleGroupUninviteGUID(game::IncomingPacket &packet)
	{
		UInt64 memberGUID;
		if (!game::client_read::groupUninviteGUID(packet, memberGUID))
		{
			// Could not read packet
			return;
		}

		DLOG("CMSG_GROUP_UNINVITE_GUID: Player " << m_gameCharacter->getName() << " want's to uninvite member 0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << memberGUID);
	}

	void Player::handleGroupSetLeader(game::IncomingPacket &packet)
	{
		UInt64 leaderGUID;
		if (!game::client_read::groupSetLeader(packet, leaderGUID))
		{
			// Could not read packet
			return;
		}

		DLOG("CMSG_GROUP_SET_LEADER: Player " << m_gameCharacter->getName() << " want's to change leader to 0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << leaderGUID);
	}

	void Player::handleLootMethod(game::IncomingPacket &packet)
	{
		UInt32 lootMethod, lootTreshold;
		UInt64 lootMasterGUID;
		if (!game::client_read::lootMethod(packet, lootMethod, lootMasterGUID, lootTreshold))
		{
			// Could not read packet
			return;
		}

		DLOG("CMSG_LOOT_METHOD: Player " << m_gameCharacter->getName() << " want's to change loot method to: " << lootMethod);
		DLOG("Loot master: 0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << lootMasterGUID);
		DLOG("Loot treshold: " << lootTreshold);
	}

	void Player::handleGroupDisband(game::IncomingPacket &packet)
	{
		if (!game::client_read::groupDisband(packet))
		{
			// Could not read packet
			return;
		}

		DLOG("CMSG_GROUP_DISBAND: Player " << m_gameCharacter->getName() << " want's to disband his group");
	}

	void Player::setGroup(std::shared_ptr<PlayerGroup> group)
	{
		m_group = group;
	}

}
