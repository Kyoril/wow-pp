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
#include "proto_data/project.h"
#include "common/utilities.h"
#include "game/game_item.h"
#include "player_group.h"
#include "game/constants.h"

using namespace std;
using namespace wowpp::game;

namespace wowpp
{
	Player::Player(Configuration &config, IdGenerator<UInt64> &groupIdGenerator, PlayerManager &manager, LoginConnector &loginConnector, WorldManager &worldManager, IDatabase &database, proto::Project &project, std::shared_ptr<Client> connection, const String &address)
		: m_config(config)
		, m_groupIdGenerator(groupIdGenerator)
		, m_manager(manager)
		, m_loginConnector(loginConnector)
		, m_worldManager(worldManager)
		, m_database(database)
		, m_project(project)
		, m_connection(std::move(connection))
		, m_address(address)
        , m_authed(false)
		, m_characterId(std::numeric_limits<DatabaseId>::max())
		, m_instanceId(std::numeric_limits<UInt32>::max())
		, m_worldNode(nullptr)
		, m_transferMap(0)
		, m_transfer(math::Vector3(0.0f, 0.0f, 0.0f))
		, m_transferO(0.0f)
		, m_nextWhoRequest(0)
		, m_mailIdGenerator(1)
		, m_unreadMails(0)
	{
		// Randomize seed
		std::uniform_int_distribution<UInt32> dist;
		m_seed = dist(randomGenerator);

		ASSERT(m_connection);

		m_connection->setListener(*this);
		m_social.reset(new PlayerSocial(m_manager, *this));
		m_tutorialData.fill(0);
	}

	void Player::sendAuthChallenge()
	{
		sendPacket(
			std::bind(game::server_write::authChallenge, std::placeholders::_1, m_seed));
	}

	void Player::loginSucceeded(UInt32 accountId, auth::AuthLocale locale, const BigNumber &key, const BigNumber &v, const BigNumber &s, const std::array<UInt32, 8> &tutorialData)
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

		m_tutorialData = tutorialData; // std::move won't be faster here, cause of std::array
		m_accountId = accountId;
		m_sessionKey = key;
		m_v = v;
		m_s = s;
		m_locale = locale;

		//TODO Create session

		ILOG("Client " << m_accountName << " authenticated successfully from " << m_address);
		ILOG("Client locale: " << m_locale);
		m_authed = true;

		// Notify login connector
		m_loginConnector.notifyPlayerLogin(m_accountId);

		// Initialize crypt
		game::Connection *cryptCon = static_cast<game::Connection*>(m_connection.get());
		game::Crypt &crypt = cryptCon->getCrypt();
		
		//For BC
		HMACHash cryptKey;
		crypt.generateKey(cryptKey, m_sessionKey);
#if SUPPORTED_CLIENT_BUILD >= 8606
		crypt.setKey(cryptKey.data(), cryptKey.size());
#else
		crypt.setKey(m_sessionKey.asByteArray().data(), 40);
#endif
		crypt.init();

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
		// If we are logged in, notify the world node about this
		if (m_gameCharacter)
		{
			// Decline pending group invite
			declineGroupInvite();

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
				ILOG("Sent leave world instance packet");
				world->leaveWorldInstance(m_characterId, pp::world_realm::world_left_reason::Disconnect);
				// We don't destroy this player instance yet, as we are still connected to a world node: This world node needs to
				// send the character's new data back to us, so that we can save it.
				return;
			}
			else
			{
				WLOG("Failed to find the world node - can't send disconnect notification.");
			}
		}

		if (m_authed)
		{
			// Notify login server about disconnection so that the session can be closed

		}

		destroy();
	}

	void Player::connectionMalformedPacket()
	{
		ILOG("Client " << m_address << " sent malformed packet");
		destroy();
	}

	bool Player::isSessionStatusValid(const String &name, game::SessionStatus status, bool verbose /*= false*/) const
	{
		switch (status)
		{
			case game::session_status::Never:
			{
				if (verbose) WLOG("Packet " << name << " isn't handled on the server side!");
				return false;
			}

			case game::session_status::Connected:
			{
				if (m_authed && verbose) WLOG("Packet " << name << " is only handled if not yet authenticated!");
				return !m_authed;
			}

			case game::session_status::Authentificated:
			{
				if (!m_authed && verbose) WLOG("Packet " << name << " is only handled if the player is authenticated!");
				return m_authed;
			}

			case game::session_status::LoggedIn:
			{
				if (!m_worldNode && verbose) WLOG("Packet " << name << " is only handled if the player is logged in!");
				return (m_worldNode != nullptr);
			}

			case game::session_status::TransferPending:
			{
				if ((m_characterId == 0 || m_worldNode != nullptr) && verbose)
				{
					WLOG("Packet " << name << " is only handled if a transfer is pending!");
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
#define QUOTE(str) #str
#define WOWPP_HANDLE_PACKET(name, sessionStatus) \
			case wowpp::game::client_packet::name: \
			{ \
				if (!isSessionStatusValid(#name, sessionStatus, true)) \
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
			WOWPP_HANDLE_PACKET(NameQuery, game::session_status::Authentificated)
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
			WOWPP_HANDLE_PACKET(RequestPartyMemberStats, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(MoveWorldPortAck, game::session_status::TransferPending)
			WOWPP_HANDLE_PACKET(SetActionButton, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(GameObjectQuery, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(TutorialFlag, game::session_status::Authentificated)
			WOWPP_HANDLE_PACKET(TutorialClear, game::session_status::Authentificated)
			WOWPP_HANDLE_PACKET(TutorialReset, game::session_status::Authentificated)
			WOWPP_HANDLE_PACKET(CompleteCinematic, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(RaidTargetUpdate, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(GroupRaidConvert, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(GroupAssistentLeader, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(RaidReadyCheck, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(RaidReadyCheckFinished, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(RealmSplit, game::session_status::Authentificated)
			WOWPP_HANDLE_PACKET(VoiceSessionEnable, game::session_status::Authentificated)
			WOWPP_HANDLE_PACKET(CharRename, game::session_status::Authentificated)
			WOWPP_HANDLE_PACKET(QuestQuery, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(Who, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(MinimapPing, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(ItemNameQuery, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(CreatureQuery, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(MailQueryNextTime, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(MailGetBody, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(MailTakeMoney, game::session_status::LoggedIn)
			WOWPP_HANDLE_PACKET(GetChannelMemberCount, game::session_status::LoggedIn)

#undef WOWPP_HANDLE_PACKET
#undef QUOTE

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

		// Check if the client version is valid (defined in CMake)
		if (clientBuild != SUPPORTED_CLIENT_BUILD)
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

		reloadCharacters();
	}

	void Player::handleCharCreate(game::IncomingPacket &packet)
	{
		game::CharEntry character;
		if (!(game::client_read::charCreate(packet, character)))
		{
			return;
		}

		// Empty character name?
		character.name = trim(character.name);
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
		const auto *race = m_project.races.getById(character.race);
		if (!race)
		{
			ELOG("Unable to find informations of race " << character.race);
			sendPacket(
				std::bind(game::server_write::charCreate, std::placeholders::_1, game::response_code::CharCreateError));
			return;
		}

		// Add initial spells
		const auto &initialSpellsEntry = race->initialspells().find(character.class_);
		if (initialSpellsEntry == race->initialspells().end())
		{
			ELOG("No initial spells set up for race " << race->name() << " and class " << character.class_);
			sendPacket(
				std::bind(game::server_write::charCreate, std::placeholders::_1, game::response_code::CharCreateError));
			return;
		}

		// Item data
		UInt8 bagSlot = 0;
		std::vector<ItemData> items;

		// Add initial items
		auto it1 = race->initialitems().find(character.class_);
		if (it1 != race->initialitems().end())
		{
			for (int i = 0; i < it1->second.items_size(); ++i)
			{
				const auto *item = m_project.items.getById(it1->second.items(i));
				if (!item)
					continue;

				// Get slot for this item
				UInt16 slot = 0xffff;
				switch (item->inventorytype())
				{
					case game::inventory_type::Head:
					{
						slot = player_equipment_slots::Head;
						break;
					}
					case game::inventory_type::Neck:
					{
						slot = player_equipment_slots::Neck;
						break;
					}
					case game::inventory_type::Shoulders:
					{
						slot = player_equipment_slots::Shoulders;
						break;
					}
					case game::inventory_type::Body:
					{
						slot = player_equipment_slots::Body;
						break;
					}
					case game::inventory_type::Chest:
					case game::inventory_type::Robe:
					{
						slot = player_equipment_slots::Chest;
						break;
					}
					case game::inventory_type::Waist:
					{
						slot = player_equipment_slots::Waist;
						break;
					}
					case game::inventory_type::Legs:
					{
						slot = player_equipment_slots::Legs;
						break;
					}
					case game::inventory_type::Feet:
					{
						slot = player_equipment_slots::Feet;
						break;
					}
					case game::inventory_type::Wrists:
					{
						slot = player_equipment_slots::Wrists;
						break;
					}
					case game::inventory_type::Hands:
					{
						slot = player_equipment_slots::Hands;
						break;
					}
					case game::inventory_type::Finger:
					{
						//TODO: Finger1/2
						slot = player_equipment_slots::Finger1;
						break;
					}
					case game::inventory_type::Trinket:
					{
						//TODO: Trinket1/2
						slot = player_equipment_slots::Trinket1;
						break;
					}
					case game::inventory_type::Weapon:
					case game::inventory_type::TwoHandedWeapon:
					case game::inventory_type::MainHandWeapon:
					{
						slot = player_equipment_slots::Mainhand;
						break;
					}
					case game::inventory_type::Shield:
					case game::inventory_type::OffHandWeapon:
					case game::inventory_type::Holdable:
					{
						slot = player_equipment_slots::Offhand;
						break;
					}
					case game::inventory_type::Ranged:
					case game::inventory_type::Thrown:
					{
						slot = player_equipment_slots::Ranged;
						break;
					}
					case game::inventory_type::Cloak:
					{
						slot = player_equipment_slots::Back;
						break;
					}
					case game::inventory_type::Tabard:
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
					ItemData itemData;
					itemData.entry = item->id();
					itemData.durability = item->durability();
					itemData.slot = slot | 0xFF00;
					itemData.stackCount = 1;
					items.push_back(std::move(itemData));
				}
			}
		}

		// Update character location
		character.mapId = race->startmap();
		character.zoneId = race->startzone();
		character.location.x = race->startposx();
		character.location.y = race->startposy();
		character.location.z = race->startposz();
		character.o = race->startrotation();

		std::vector<const proto::SpellEntry*> initialSpells;
		for (int i = 0; i < initialSpellsEntry->second.spells_size(); ++i)
		{
			const auto &spellid = initialSpellsEntry->second.spells(i);
			const auto *spell = m_project.spells.getById(spellid);
			if (spell)
			{
				initialSpells.push_back(spell);
			}
		}

		// Create character
		game::ResponseCode result = m_database.createCharacter(m_accountId, initialSpells, items, character);
		if (result == game::response_code::CharCreateSuccess)
		{
			// Cache the character data
			m_characters.push_back(character);

			// Add initial action buttons
			const auto classBtns = race->initialactionbuttons().find(character.class_);
			if (classBtns != race->initialactionbuttons().end())
			{
				wowpp::ActionButtons buttons;
				for (const auto &btn : classBtns->second.actionbuttons())
				{
					auto &entry = buttons[btn.first];
					entry.action = btn.second.action();
					entry.misc = btn.second.misc();
					entry.state = static_cast<ActionButtonUpdateState>(btn.second.state());
					entry.type = btn.second.type();
				}

				m_database.setCharacterActionButtons(character.id, buttons);
			}
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

		std::vector<char> buffer;
		io::VectorSink sink(buffer);
		game::OutgoingPacket outPacket(sink);
		game::server_write::friendStatus(outPacket, characterId, game::friend_result::Removed, game::SocialInfo());

		// Remove ourself from friend lists
		m_manager.foreachPlayer([characterId, &outPacket, &buffer](Player &player)
		{
			auto &social = player.getSocial();
			auto result = social.removeFromSocialList(characterId, false);
			if (result == game::friend_result::Removed)
			{
				player.sendProxyPacket(outPacket.getOpCode(), buffer);
			}
			social.removeFromSocialList(characterId, true);
		});

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

		// Make sure that a character, who is flagged for rename, is renamed first!
		if (charEntry->atLogin & game::atlogin_flags::Rename)
		{
			return;
		}

		// Write something to the log just for informations
		ILOG("Player " << m_accountName << " tries to enter the world with character 0x" << std::hex << std::setw(16) << std::setfill('0') << std::uppercase << characterId);

		// Load the player character data from the database
		std::shared_ptr<GameCharacter> character(new GameCharacter(m_project, m_manager.getTimers()));
		character->initialize();
		character->setGuid(createRealmGUID(characterId, m_loginConnector.getRealmID(), guid_type::Player));
		if (!m_database.getGameCharacter(guidLowerPart(characterId), *character))
		{
			// Send error packet
			WLOG("Player login failed: Could not load character " << characterId);
			sendPacket(
				std::bind(game::server_write::charLoginFailed, std::placeholders::_1, game::response_code::CharLoginNoCharacter));
			return;
		}

		// Restore character group if possible
		UInt32 groupInstanceId = std::numeric_limits<UInt32>::max();
		if (character->getGroupId() != 0)
		{
			auto groupIt = PlayerGroup::GroupsById.find(character->getGroupId());
			if (groupIt != PlayerGroup::GroupsById.end())
			{
				// Apply character group
				m_group = groupIt->second;
				groupInstanceId = m_group->instanceBindingForMap(charEntry->mapId);
			}
			else
			{
				WLOG("Failed to restore character group");
				character->setGroupId(0);
			}
		}

		// We found the character - now we need to look for a world node
		// which is hosting a fitting world instance or is able to create
		// a new one

		auto *worldNode = m_worldManager.getWorldByMapId(charEntry->mapId);
		if (!worldNode)
		{
			// World does not exist
			WLOG("Player login failed: Could not find world server for map " << charEntry->mapId);
			sendPacket(
				std::bind(game::server_write::charLoginFailed, std::placeholders::_1, game::response_code::CharLoginNoWorld));
			return;
		}

		// Store character id
		m_characterId = characterId;

		// Use the new character
		m_gameCharacter = std::move(character);
		m_gameCharacter->setZone(charEntry->zoneId);

		// TEST: If it is a hunter, set ammo
		if (m_gameCharacter->getClass() == char_class::Hunter)
		{
			m_gameCharacter->setUInt32Value(character_fields::AmmoId, 2512);
		}

		// Load the social list
		m_social.reset(new PlayerSocial(m_manager, *this));
		m_database.getCharacterSocialList(m_characterId, *m_social);

		// Load action buttons
		m_actionButtons.clear();
		m_database.getCharacterActionButtons(m_characterId, m_actionButtons);

		// When the world node disconnects while we try to log in, send error packet
		m_worldDisconnected = worldNode->onConnectionLost.connect([this]()
		{
			m_gameCharacter.reset();
			m_worldNode = nullptr;
			m_instanceId = std::numeric_limits<UInt32>::max();

			sendPacket(
				std::bind(game::server_write::charLoginFailed, std::placeholders::_1, game::response_code::CharLoginNoWorld));

			m_worldDisconnected.disconnect();
		});

		// There should be an instance
		worldNode->enterWorldInstance(charEntry->id, groupInstanceId, *m_gameCharacter);
	}

	void Player::worldInstanceEntered(World &world, UInt32 instanceId, UInt64 worldObjectGuid, UInt32 mapId, UInt32 zoneId, math::Vector3 location, float o)
	{
		ASSERT(m_gameCharacter);

		// Watch for world node disconnection
		m_worldNode = &world;
		m_worldDisconnected = world.onConnectionLost.connect(
			std::bind(&Player::worldNodeDisconnected, this));

		ILOG("Player " << m_gameCharacter->getName() << " entered world instance 0x" << std::hex << std::uppercase << instanceId);

		// If instance id is zero, this is the first time we enter a world since the login
		const bool isLoginEnter = (m_instanceId == std::numeric_limits<UInt32>::max());

		// Save instance id
		m_instanceId = instanceId;
		if (m_group &&
			m_group->isMember(m_gameCharacter->getGuid()))
		{
			m_group->addInstanceBinding(instanceId, mapId);
		}

		// Update character on the realm side with data received from the world server
		//m_gameCharacter->setGuid(createGUID(worldObjectGuid, 0, high_guid::Player));
		m_gameCharacter->relocate(location, o);
		m_gameCharacter->setMapId(mapId);
		
		UInt32 homeMap = 0;
		math::Vector3 homePos;
		float homeOri = 0;
		m_gameCharacter->getHome(homeMap, homePos, homeOri);

		// Clear mask
		m_gameCharacter->clearUpdateMask();

		sendPacket(
			std::bind(game::server_write::setDungeonDifficulty, std::placeholders::_1));

		// Send world verification packet to the client to proof world coordinates from
		// the character list
		sendPacket(
			std::bind(game::server_write::loginVerifyWorld, std::placeholders::_1, mapId, location, o));

		if (isLoginEnter)
		{
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
		}

		// Don't know what this packet does
		sendPacket(
			std::bind(game::server_write::setRestStart, std::placeholders::_1));

		// Notify about bind point for hearthstone (also used in case of corrupted location data)
		sendPacket(
			std::bind(game::server_write::bindPointUpdate, std::placeholders::_1, homeMap, zoneId, std::cref(homePos)));

		// TODO: Change this
		std::vector<const proto::SpellEntry*> spellBookList;
		for (const auto *spell : m_gameCharacter->getSpells())
		{
			if (spell->rank() != 0)
			{
				if (!canStackSpellRanksInSpellBook(*spell))
				{
					// Skip spells where lower ranked spell is not learned
					if (spell->prevspell())
					{
						if (!m_gameCharacter->hasSpell(spell->prevspell()))
							continue;
					}

					// Skip lower-ranked spells
					if (spell->nextspell())
					{
						if (m_gameCharacter->hasSpell(spell->nextspell()))
							continue;
					}
				}
			}

			spellBookList.push_back(spell);
		}

		// Send spells
		sendPacket(
			std::bind(game::server_write::initialSpells, std::placeholders::_1, std::cref(m_project), std::cref(spellBookList), std::cref(m_gameCharacter->getCooldowns())));

		if (isLoginEnter)
		{
			// Send tutorial flags (which tutorials have been viewed etc.)
			sendPacket(
				std::bind(game::server_write::tutorialFlags, std::placeholders::_1, std::cref(m_tutorialData)));

			/*
			sendPacket(
				std::bind(game::server_write::unlearnSpells, std::placeholders::_1));
			*/

			auto raceEntry = m_gameCharacter->getRaceEntry();
			ASSERT(raceEntry);

			sendPacket(
				std::bind(game::server_write::actionButtons, std::placeholders::_1, std::cref(m_actionButtons)));
		
			sendPacket(
				std::bind(game::server_write::initializeFactions, std::placeholders::_1, std::cref(*m_gameCharacter)));

			// Trigger intro cinematic based on the characters race
			game::CharEntry *charEntry = getCharacterById(m_characterId);
			if (raceEntry &&
				(charEntry && charEntry->cinematic))
			{
				sendPacket(
					std::bind(game::server_write::triggerCinematic, std::placeholders::_1, raceEntry->cinematic()));
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
		
		// Retrieve ignore list and send it to the new world node
		std::vector<UInt64> ignoreList;
		m_social->getIgnoreList(ignoreList);
		m_worldNode->characterIgnoreList(m_gameCharacter->getGuid(), ignoreList);
	}

	void Player::worldInstanceLeft(World &world, UInt32 instanceId, pp::world_realm::WorldLeftReason reason)
	{
		// We no longer care about the world node
		m_worldDisconnected.disconnect();
		m_worldNode = nullptr;

		// Save action buttons
		if (m_gameCharacter)
		{
			m_database.setCharacterActionButtons(m_gameCharacter->getGuid(), m_actionButtons);
		}

		switch (reason)
		{
			case pp::world_realm::world_left_reason::Logout:
			{
				// Decline pending group invite
				declineGroupInvite();

				auto guid = m_gameCharacter->getGuid();

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
				m_instanceId = std::numeric_limits<UInt32>::max();

				// If we are in a group, notify others
				if (m_group)
				{
					std::vector<UInt64> exclude;
					exclude.push_back(guid);

					m_group->broadcastPacket(
						std::bind(game::server_write::partyMemberStatsFullOffline, std::placeholders::_1, guid), &exclude);
					m_group.reset();
				}

				break;
			}

			case pp::world_realm::world_left_reason::Teleport:
			{
				// We were removed from the old world node - now we can move on to the new one
				commitTransfer();
				break;
			}

			case pp::world_realm::world_left_reason::Disconnect:
			{
				// If we are in a group, notify others
				if (m_group && m_gameCharacter)
				{
					std::vector<UInt64> exclude;
					exclude.push_back(m_gameCharacter->getGuid());

					// Broadcast disconnect
					m_group->broadcastPacket(
						std::bind(game::server_write::partyMemberStatsFullOffline, std::placeholders::_1, m_gameCharacter->getGuid()), &exclude);
					m_group.reset();
				}

				// Finally destroy this instance
				ILOG("Left world instance because of a disconnect");
				destroy();
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
		const size_t bufferSize = buffer.size();
		if (bufferSize == 0)
			return;

		// Write native packet
		wowpp::Buffer &sendBuffer = m_connection->getSendBuffer();
		io::StringSink sink(sendBuffer);

		// Get the end of the buffer (needed for encryption)
		size_t bufferPos = sendBuffer.size();

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

	AddItemResult Player::addItem(UInt32 itemId, UInt32 amount)
	{
		const auto *itemEntry = m_project.items.getById(itemId);
		if (!itemEntry)
		{
			return add_item_result::ItemNotFound;
		}

		if (!m_worldNode)
		{
			// Player is not in a world right now (maybe loading screen)
			return add_item_result::Unknown;
		}

		std::vector<ItemData> modifiedItems;

		ItemData data;
		data.contained = m_gameCharacter->getGuid();
		data.creator = 0;
		data.durability = itemEntry->durability();
		data.entry = itemId;
		data.randomPropertyIndex = 0;
		data.randomSuffixIndex = 0;
		data.slot = 0;
		data.stackCount = amount;
		modifiedItems.push_back(std::move(data));

		// Notify world node about this
		m_worldNode->itemData(m_gameCharacter->getGuid(), modifiedItems);
	
		// Save items
		return add_item_result::Success;
	}

	LearnSpellResult Player::learnSpell(const proto::SpellEntry & spell)
	{
		if (!m_gameCharacter)
		{
			return learn_spell_result::Unknown;
		}

		if (m_gameCharacter->hasSpell(spell.id()))
		{
			return learn_spell_result::AlreadyLearned;
		}

		if (!m_worldNode)
		{
			return learn_spell_result::Unknown;
		}

		m_worldNode->characterLearnedSpell(m_gameCharacter->getGuid(), spell.id());
		return learn_spell_result::Success;
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
			case chat_msg::Emote:
			case chat_msg::TextEmote:
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

				// Check faction
				const bool isAllianceA = ((game::race::Alliance & (1 << (m_gameCharacter->getRace() - 1))) == (1 << (m_gameCharacter->getRace() - 1)));
				const bool isAllianceB = ((game::race::Alliance & (1 << (entry.race - 1))) == (1 << (entry.race - 1)));
				if (isAllianceA != isAllianceB)
				{
					sendPacket(
						std::bind(game::server_write::chatWrongFaction, std::placeholders::_1));
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

				if (other->getSocial().isIgnored(m_gameCharacter->getGuid()))
				{
					DLOG("TODO: Other player ignores us - notify our client about this...");
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
			case chat_msg::Raid:
			case chat_msg::Party:
			case chat_msg::RaidLeader:
			case chat_msg::RaidWarning:
			{
				// Get the players group
				if (!m_group)
				{
					WLOG("Player is not in group");
					return;
				}

				auto groupType = m_group->getType();
				if (groupType == group_type::Raid)
				{
					if (type == chat_msg::Party)
					{
						// Only affect players subgroup
						DLOG("TODO: Player subgroup");
					}

					const bool isRaidLead = m_group->getLeader() == m_gameCharacter->getGuid();
					const bool isAssistant = m_group->isLeaderOrAssistant(m_gameCharacter->getGuid());
					if (type == chat_msg::RaidLeader &&
						!isRaidLead)
					{
						type = chat_msg::Raid;
					}
					else if (type == chat_msg::RaidWarning &&
						!isRaidLead && 
						!isAssistant)
					{
						WLOG("Raid warning can only be done by raid leader or assistants");
						return;
					}
				}
				else
				{
					if (type == chat_msg::Raid)
					{
						DLOG("Not a raid group!");
						return;
					}
				}

				// Maybe we were just invited, but are not yet a member of that group
				if (!m_group->isMember(m_gameCharacter->getGuid()))
				{
					WLOG("Player is not a member of the group, but was just invited.");
					return;
				}

				// Broadcast chat packet
				m_group->broadcastPacket(
					std::bind(game::server_write::messageChat, std::placeholders::_1, type, lang, std::cref(channel), m_characterId, std::cref(message), m_gameCharacter.get()), nullptr, m_gameCharacter->getGuid());

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

		// Check faction
		const bool isAllianceA = ((game::race::Alliance & (1 << (m_gameCharacter->getRace() - 1))) == (1 << (m_gameCharacter->getRace() - 1)));
		const bool isAllianceB = ((game::race::Alliance & (1 << (friendChar.race - 1))) == (1 << (friendChar.race - 1)));
		
		// Result code
		game::FriendResult result = game::friend_result::AddedOffline;
		if (characterGUID == m_characterId)
		{
			result = game::friend_result::Self;
		}
		else if (isAllianceA != isAllianceB)
		{
			result = game::friend_result::Enemy;
		}
		else
		{
			// Add to social list
			result = m_social->addToSocialList(characterGUID, false);
			if (result == game::friend_result::AddedOffline)
			{
				// Add to database
				const bool shouldUpdate = m_social->isIgnored(characterGUID);
				if (!shouldUpdate)
				{
					if (!m_database.addCharacterSocialContact(m_characterId, characterGUID, static_cast<game::SocialFlag>(info.flags), info.note))
					{
						result = game::friend_result::DatabaseError;
					}
				}
				else
				{
					if (!m_database.updateCharacterSocialContact(m_characterId, characterGUID, static_cast<SocialFlag>(game::social_flag::Friend | game::social_flag::Ignored)))
					{
						result = game::friend_result::DatabaseError;
					}
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
			if (m_social->isIgnored(guid))
			{
				// Old friend is still ignored - update flags
				if (!m_database.updateCharacterSocialContact(m_characterId, guid, game::social_flag::Ignored, ""))
				{
					result = game::friend_result::DatabaseError;
				}
			}
			else
			{
				// Completely remove contact, as he is neither ignored nor a friend
				if (!m_database.removeCharacterSocialContact(m_characterId, guid))
				{
					result = game::friend_result::DatabaseError;
				}
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

        // Find the character details
        game::CharEntry ignoredChar;
        if (!m_database.getCharacterByName(name, ignoredChar))
        {
            WLOG("Could not find that character");
            return;
        }

        // Create the characters guid value
        UInt64 characterGUID = createRealmGUID(ignoredChar.id, m_loginConnector.getRealmID(), guid_type::Player);

        // Fill ignored info
        game::SocialInfo info;
        info.flags = game::social_flag::Ignored;
        info.area = ignoredChar.zoneId;
        info.level = ignoredChar.level;
        info.class_ = ignoredChar.class_;

        //result
        game::FriendResult result = m_social->addToSocialList(characterGUID, true);
		if (result == game::friend_result::IgnoreAdded)
		{
			const bool shouldUpdate = m_social->isFriend(characterGUID);
			if (!shouldUpdate)
			{
				if (!m_database.addCharacterSocialContact(m_characterId, characterGUID, static_cast<game::SocialFlag>(info.flags), ""))
				{
					result = game::friend_result::DatabaseError;
				}
			}
			else
			{
				if (!m_database.updateCharacterSocialContact(m_characterId, characterGUID, static_cast<SocialFlag>(game::social_flag::Friend | game::social_flag::Ignored)))
				{
					result = game::friend_result::DatabaseError;
				}
			}

			m_worldNode->characterAddIgnore(m_characterId, characterGUID);
		}

        sendPacket(
            std::bind(game::server_write::friendStatus, std::placeholders::_1, characterGUID, result, std::cref(info)));
	}

	void Player::handleDeleteIgnore(game::IncomingPacket &packet)
	{
		UInt64 guid;
		if (!client_read::deleteIgnore(packet, guid))
		{
			// Error reading packet
			return;
		}

		// Find the character details
		game::CharEntry ignoredChar;

		// Fill ignored info
		game::SocialInfo info;

		//result
		auto result = m_social->removeFromSocialList(guid, true);
		if (result == game::friend_result::IgnoreRemoved)
		{
			if (m_social->isFriend(guid))
			{
				if (!m_database.updateCharacterSocialContact(m_characterId, guid, game::social_flag::Friend))
				{
					result = game::friend_result::DatabaseError;
				}
			}
			else
			{
				if (!m_database.removeCharacterSocialContact(m_characterId, guid))
				{
					result = game::friend_result::DatabaseError;
				}
			}

			m_worldNode->characterRemoveIgnore(m_characterId, guid);
		}
		else
		{
			WLOG("handleDeleteIgnore: result " << result);
		}

		sendPacket(
			std::bind(game::server_write::friendStatus, std::placeholders::_1, guid, result, std::cref(info)));
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
		const auto *item = m_project.items.getById(itemID);
		if (item)
		{
			// TODO: Cache multiple query requests and send one, bigger response with multiple items

			// Write answer packet
			sendPacket(
				std::bind(game::server_write::itemQuerySingleResponse, std::placeholders::_1, m_locale, std::cref(*item)));
		}
	}

	void Player::handleCreatureQuery(game::IncomingPacket &packet)
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

		// Find creature info by entry
		const auto *unit = m_project.units.getById(creatureEntry);
		if (unit)
		{
			// Write answer packet
			sendPacket(
				std::bind(game::server_write::creatureQueryResponse, std::placeholders::_1, m_locale, std::cref(*unit)));
		}
		else
		{
			//TODO: Send resulting packet SMSG_CREATURE_QUERY_RESPONSE with only one uin32 value
			//which is creatureEntry | 0x80000000
		}
	}

	void Player::handleMailQueryNextTime(game::IncomingPacket & packet)
	{
		if (!game::client_read::mailQueryNextTime(packet))
		{
			return;
		}

		sendPacket(
			std::bind(game::server_write::mailQueryNextTime, std::placeholders::_1, m_unreadMails, getMails()));
	}

	void Player::handleMailGetBody(game::IncomingPacket & packet)
	{
		UInt32 mailTextId, mailId;

		if (!game::client_read::mailGetBody(packet, mailTextId, mailId))
		{
			return;
		}

		String body = getMail(mailId)->getBody();

		sendPacket(
			std::bind(game::server_write::mailSendBody, std::placeholders::_1, /*TODO: change to mailTextId*/mailId, body));
	}

	void Player::handleMailTakeMoney(game::IncomingPacket & packet)
	{
		ObjectGuid mailboxGuid;
		UInt32 mailId;

		if (!game::client_read::mailTakeMoney(packet, mailboxGuid, mailId))
		{
			return;
		}

		// TODO check distance to mailbox, etc

		// TODO check state + delivery time
		auto mail = getMail(mailId);
		if (!mail)
		{
			sendPacket(
				std::bind(game::server_write::mailSendResult, std::placeholders::_1,
					MailResult(mailId, mail::response_type::MoneyTaken, mail::response_result::Internal)));
			return;
		}

		sendPacket(
			std::bind(game::server_write::mailSendResult, std::placeholders::_1,
				MailResult(mailId, mail::response_type::MoneyTaken, mail::response_result::Ok)));

		m_worldNode->changeMoney(m_characterId, mail->getMoney(), false);
		mail->setMoney(0);
		// TODO change mail state
	}

	void Player::handleMailDelete(game::IncomingPacket & packet)
	{
		ObjectGuid mailboxGuid;
		UInt32 mailId;

		if (!game::client_read::mailDelete(packet, mailboxGuid, mailId))
		{
			return;
		}

		// TODO check distance to mailbox, etc

		auto mail = getMail(mailId);
		if (mail && mail->getCOD() > 0)
		{
			sendPacket(
				std::bind(game::server_write::mailSendResult, std::placeholders::_1,
					MailResult(mailId, mail::response_type::Deleted, mail::response_result::Ok)));

			// TODO change mail state (delete it maybe ?)
		}
		else
		{
			sendPacket(
				std::bind(game::server_write::mailSendResult, std::placeholders::_1,
					MailResult(mailId, mail::response_type::Deleted, mail::response_result::Internal)));
		}
	}

	void Player::handleGetChannelMemberCount(game::IncomingPacket & packet)
	{
		// TODO
	}

	/*
	void Player::saveCharacter()
	{
		if (m_gameCharacter)
		{
			//m_database.saveGameCharacter(*m_gameCharacter, m_itemData);
			m_database.setCharacterActionButtons(m_gameCharacter->getGuid(), m_actionButtons);
		}
	}
	*/
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

		if (player->getSocial().isIgnored(m_gameCharacter->getGuid()))
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, party_operation::Invite, std::cref(playerName), party_result::TargetIgnoreYou));
			return;
		}

		// Get players group or create a new one
		if (!m_group)
		{
			// Create the group
			m_group = std::make_shared<PlayerGroup>(m_groupIdGenerator.generateId(), m_manager, m_database);
			m_group->create(*m_gameCharacter);

			// Save character group id
			m_gameCharacter->setGroupId(m_group->getId());

			// Send to world node
			m_worldNode->characterGroupChanged(m_gameCharacter->getGuid(), m_group->getId());
		}

		// Check if we are the leader of that group
		if (!m_group->isLeaderOrAssistant(m_gameCharacter->getGuid()))
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

		m_gameCharacter->setGroupId(m_group->getId());

		// Send to world node
		m_worldNode->characterGroupChanged(m_gameCharacter->getGuid(), m_group->getId());
	}
	
	void Player::handleGroupDecline(game::IncomingPacket &packet)
	{
		if (!game::client_read::groupDecline(packet))
		{
			// Could not read packet
			return;
		}

		declineGroupInvite();
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

		if (!m_group)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Leave, "", game::party_result::YouNotInGroup));
			return;
		}

		if (!m_group->isLeaderOrAssistant(m_gameCharacter->getGuid()))
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Leave, "", game::party_result::YouNotLeader));
			return;
		}

		UInt64 guid = m_group->getMemberGuid(memberName);
		if (guid == 0)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Leave, std::cref(memberName), game::party_result::NotInYourParty));
			return;
		}

		// Raid assistants may not kick the leader
		if (m_gameCharacter->getGuid() != m_group->getLeader() &&
			m_group->getLeader() == guid)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Leave, "", game::party_result::YouNotLeader));
			return;
		}

		m_group->removeMember(guid);
	}

	void Player::handleGroupUninviteGUID(game::IncomingPacket &packet)
	{
		UInt64 memberGUID;
		if (!game::client_read::groupUninviteGUID(packet, memberGUID))
		{
			// Could not read packet
			return;
		}

		if (!m_group)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Leave, "", game::party_result::YouNotInGroup));
			return;
		}

		if (!m_group->isLeaderOrAssistant(m_gameCharacter->getGuid()))
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Leave, "", game::party_result::YouNotLeader));
			return;
		}

		if (!m_group->isMember(memberGUID))
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Leave, "", game::party_result::NotInYourParty));
			return;
		}

		// Raid assistants may not kick the leader
		if (m_gameCharacter->getGuid() != m_group->getLeader() &&
			m_group->getLeader() == memberGUID)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Leave, "", game::party_result::YouNotLeader));
			return;
		}

		m_group->removeMember(memberGUID);
	}

	void Player::handleGroupSetLeader(game::IncomingPacket &packet)
	{
		UInt64 leaderGUID;
		if (!game::client_read::groupSetLeader(packet, leaderGUID))
		{
			return;
		}

		if (!m_group)
		{
			WLOG("Player is not a member of a group!");
			return;
		}
		
		if (m_group->getLeader() == leaderGUID ||
			m_group->getLeader() != m_gameCharacter->getGuid())
		{
			WLOG("Player is not the group leader or no leader change");
			return;
		}
		
		m_group->setLeader(leaderGUID);
		m_group->sendUpdate();
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

		if (!m_group)
		{
			return;
		}
		if (m_group->getLeader() != m_gameCharacter->getGuid())
		{
			return;
		}
		if (lootMethod > loot_method::NeedBeforeGreed)
		{
			return;
		}
		if (lootTreshold < 2 || lootTreshold > 6)
		{
			return;
		}
		if (lootMethod == loot_method::MasterLoot &&
			!m_group->isMember(lootMasterGUID))
		{
			return;
		}

		m_group->setLootMethod(static_cast<LootMethod>(lootMethod), lootMasterGUID, lootTreshold);
		m_group->sendUpdate();
	}

	void Player::handleGroupDisband(game::IncomingPacket &packet)
	{
		if (!game::client_read::groupDisband(packet))
		{
			// Could not read packet
			return;
		}

		if (!m_group)
		{
			return;
		}

		// Remove this group member
		// Note: This will make m_group invalid
		m_group->removeMember(m_gameCharacter->getGuid());
	}

	void Player::setGroup(std::shared_ptr<PlayerGroup> group)
	{
		m_group = group;
	}

	Mail * Player::getMail(UInt32 mailId)
	{
		for (auto &mail : m_mails)
		{
			if (mail.getMailId() == mailId)
			{
				return &mail;
			}
		}

		return nullptr;
	}

	void Player::declineGroupInvite()
	{
		if (!m_group || !m_gameCharacter)
		{
			return;
		}

		// Find the group leader
		UInt64 leader = m_group->getLeader();
		if (!m_group->removeInvite(m_gameCharacter->getGuid()))
		{
			return;
		}

		// We are no longer a member of this group
		m_group.reset();

		if (leader != 0)
		{
			auto *player = m_manager.getPlayerByCharacterGuid(leader);
			if (player)
			{
				player->sendPacket(
					std::bind(game::server_write::groupDecline, std::placeholders::_1, std::cref(m_gameCharacter->getName())));
			}
		}
	}

	void Player::reloadCharacters()
	{
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

	void Player::spawnedNotification()
	{
		if (m_group)
		{
			m_group->sendUpdate();

			std::vector<UInt64> exclude;
			exclude.push_back(m_gameCharacter->getGuid());
			m_group->broadcastPacket(
				std::bind(game::server_write::partyMemberStatsFull, std::placeholders::_1, std::cref(*m_gameCharacter)), &exclude);
		}
	}

	void Player::mailReceived(Mail mail)
	{
		mail.setMailId(m_mailIdGenerator.generateId());
		// TODO save to db
		m_mails.push_front(mail);
		m_unreadMails++;

		DLOG("Mail stored");

		sendPacket(
			std::bind(game::server_write::mailReceived, std::placeholders::_1, mail.getMailId()));
	}

	void Player::readMail()
	{
		if (m_unreadMails > 0)
		{
			m_unreadMails--;
		}
	}

	void Player::handleRequestPartyMemberStats(game::IncomingPacket &packet)
	{
		UInt64 guid = 0;
		if (!game::client_read::requestPartyMemberStats(packet, guid))
		{
			// Could not read packet
			return;
		}

		// Try to find that player
		auto *player = m_manager.getPlayerByCharacterGuid(guid);
		if (!player)
		{
			DLOG("Could not find player with character guid - send offline packet");
			sendPacket(
				std::bind(game::server_write::partyMemberStatsFullOffline, std::placeholders::_1, guid));
			return;
		}

		// Send result
		sendPacket(
			std::bind(game::server_write::partyMemberStatsFull, std::placeholders::_1, std::cref(*m_gameCharacter)));
	}

	bool Player::initializeTransfer(UInt32 map, math::Vector3 location, float o, bool shouldLeaveNode/* = false*/)
	{
		auto *world = m_worldManager.getWorldByInstanceId(m_instanceId);
		if (shouldLeaveNode && !world)
		{
			return false;
		}

		m_transferMap = map;
		m_transfer = location;
		m_transferO = o;

		if (shouldLeaveNode)
		{
			// Send transfer pending state. This will show up the loading screen at the client side
			sendPacket(
				std::bind(game::server_write::transferPending, std::placeholders::_1, map, 0, 0));
			world->leaveWorldInstance(m_characterId, pp::world_realm::world_left_reason::Teleport);
		}

		return true;
	}

	void Player::commitTransfer()
	{
		if (m_transferMap == 0 && m_transfer.x == 0.0f && m_transfer.y == 0.0f && m_transfer.z == 0.0f && m_transferO == 0.0f)
		{
			WLOG("No transfer pending - commit will be ignored.");
			return;
		}

		// Load new world
		sendPacket(
			std::bind(game::server_write::newWorld, std::placeholders::_1, m_transferMap, m_transfer, m_transferO));
	}
	
	void Player::handleMoveWorldPortAck(game::IncomingPacket &packet)
	{
		if (m_transferMap == 0 && m_transfer.x == 0.0f && m_transfer.y == 0.0f && m_transfer.z == 0.0f && m_transferO == 0.0f)
		{
			WLOG("No transfer pending - commit will be ignored.");
			return;
		}

		// Update character location
		m_gameCharacter->setMapId(m_transferMap);
		m_gameCharacter->relocate(m_transfer, m_transferO);
		
		// We found the character - now we need to look for a world node
		// which is hosting a fitting world instance or is able to create
		// a new one

		UInt32 groupInstanceId = std::numeric_limits<UInt32>::max();

		// Determine group instance to join
		auto *player = m_manager.getPlayerByCharacterGuid(m_gameCharacter->getGuid());
		if (player)
		{
			if (auto *group = player->getGroup())
			{
				groupInstanceId = group->instanceBindingForMap(m_transferMap);
			}
		}

		// Find a new world node
		auto *world = m_worldManager.getWorldByMapId(m_transferMap);
		if (!world)
		{
			// World does not exist
			WLOG("Player login failed: Could not find world server for map " << m_transferMap);
			sendPacket(
				std::bind(game::server_write::transferAborted, std::placeholders::_1, m_transferMap, game::transfer_abort_reason::NotFound));
			return;
		}

		//TODO Map found - check if player is member of a group and if this instance
		// is valid on the world node and if not, transfer player

		// There should be an instance
		m_worldNode = world;
		m_worldNode->enterWorldInstance(m_characterId, groupInstanceId, *m_gameCharacter);

		// Reset transfer data
		m_transferMap = 0;
		m_transfer = math::Vector3(0.0f, 0.0f, 0.0f);
		m_transferO = 0.0f;
	}

	void Player::handleSetActionButton(game::IncomingPacket &packet)
	{
		ActionButton button;
		UInt8 slot = 0;
		if (!game::client_read::setActionButton(packet, slot, button.misc, button.type, button.action))
		{
			// Could not read packet
			return;
		}

		// Validate button
		if (slot > constants::ActionButtonLimit)		// TODO: Maximum number of action buttons
		{
			WLOG("Client sent invalid action button number");
			return;
		}

		// Check if we want to remove that button or add a new one
		if (button.action == 0)
		{
			auto it = m_actionButtons.find(slot);
			if (it == m_actionButtons.end())
			{
				WLOG("Could not find action button to remove - button seems to be empty already!");
				return;
			}

			// Clear button
			m_actionButtons.erase(it);
		}
		else
		{
			m_actionButtons[slot] = button;
		}
	}

	void Player::handleGameObjectQuery(game::IncomingPacket &packet)
	{
		UInt32 entry = 0;
		UInt64 guid = 0;
		if (!game::client_read::gameObjectQuery(packet, entry, guid))
		{
			// Could not read packet
			return;
		}

		const auto *objectEntry = m_project.objects.getById(entry);
		if (!objectEntry)
		{
			WLOG("Could not find game object by entry " << entry);
			sendPacket(
				std::bind(game::server_write::gameObjectQueryResponseEmpty, std::placeholders::_1, entry));
			return;
		}

		// Send response
		sendPacket(
			std::bind(game::server_write::gameObjectQueryResponse, std::placeholders::_1, m_locale, std::cref(*objectEntry)));
	}

	void Player::handleTutorialFlag(game::IncomingPacket &packet)
	{
		UInt32 flag = 0;
		if (!game::client_read::tutorialFlag(packet, flag))
		{
			// Could not read packet
			return;
		}

		UInt32 wInt = (flag / 32);
		if (wInt >= 8)
		{
			WLOG("Wrong tutorial flag sent");
			return;
		}

		UInt32 rInt = (flag % 32);

		UInt32 &tutflag = m_tutorialData[wInt];
		tutflag |= (1 << rInt);

		m_loginConnector.sendTutorialData(m_accountId, m_tutorialData);
	}

	void Player::handleTutorialClear(game::IncomingPacket &packet)
	{
		if (!game::client_read::tutorialClear(packet))
		{
			// Could not read packet
			return;
		}

		m_tutorialData.fill(0xFFFFFFFF);
		m_loginConnector.sendTutorialData(m_accountId, m_tutorialData);
	}

	void Player::handleTutorialReset(game::IncomingPacket &packet)
	{
		if (!game::client_read::tutorialReset(packet))
		{
			// Could not read packet
			return;
		}

		m_tutorialData.fill(0);
		m_loginConnector.sendTutorialData(m_accountId, m_tutorialData);
	}

	void Player::handleCompleteCinematic(game::IncomingPacket &packet)
	{
		if (!game::client_read::completeCinematic(packet))
		{
			// Could not read packet
			return;
		}

		m_database.setCinematicState(m_characterId, false);
	}

	void Player::handleRaidTargetUpdate(game::IncomingPacket &packet)
	{
		UInt8 mode = 0;
		UInt64 guid = 0;
		if (!(game::client_read::raidTargetUpdate(packet, mode, guid)))
		{
			return;
		}

		if (!m_group)
		{
			return;
		}
		if (!m_group->isMember(m_gameCharacter->getGuid()))
		{
			return;
		}

		if (mode == 0xFF)
		{
			m_group->sendTargetList(*this);
		}
		else
		{
			if (!m_group->isLeaderOrAssistant(m_gameCharacter->getGuid()))
			{
				WLOG("Only the group leader is allowed to update raid target icons!");
				return;
			}

			m_group->setTargetIcon(mode, guid);
		}
	}

	void Player::handleGroupRaidConvert(game::IncomingPacket &packet)
	{
		if (!(game::client_read::groupRaidConvert(packet)))
		{
			return;
		}

		const bool isLeader = (m_group && (m_group->getLeader() == m_gameCharacter->getGuid()));
		if (!isLeader)
		{
			return;
		}

		sendPacket(
			std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Invite, "", game::party_result::Ok));
		m_group->convertToRaidGroup();
	}

	void Player::handleGroupAssistentLeader(game::IncomingPacket &packet)
	{
		UInt64 guid;
		UInt8 flags;
		if (!(game::client_read::groupAssistentLeader(packet, guid, flags)))
		{
			return;
		}

		if (!m_group)
		{
			return;
		}
		if (m_group->getType() != group_type::Raid)
		{
			return;
		}
		if (!m_group->isLeaderOrAssistant(m_gameCharacter->getGuid()))
		{
			return;
		}

		m_group->setAssistant(guid, flags);
	}

	void Player::handleRaidReadyCheck(game::IncomingPacket &packet)
	{
		bool hasState = false;
		UInt8 state = 0;
		if (!(game::client_read::raidReadyCheck(packet, hasState, state)))
		{
			return;
		}

		if (!m_group)
		{
			return;
		}
		if (!m_group->isMember(m_gameCharacter->getGuid()))
		{
			return;
		}

		if (!hasState)
		{
			if (!m_group->isLeaderOrAssistant(m_gameCharacter->getGuid()))
			{
				return;
			}

			// Ready check request
			m_group->broadcastPacket(
				std::bind(game::server_write::raidReadyCheck, std::placeholders::_1, m_gameCharacter->getGuid()));
		}
		else
		{
			// Ready check request
			m_group->broadcastPacket(
				std::bind(game::server_write::raidReadyCheckConfirm, std::placeholders::_1, m_gameCharacter->getGuid(), state));
		}
	}

	void Player::handleRaidReadyCheckFinished(game::IncomingPacket &packet)
	{
		if (!m_group)
		{
			return;
		}
		if (!m_group->isLeaderOrAssistant(m_gameCharacter->getGuid()))
		{
			return;
		}

		// Ready check request
		m_group->broadcastPacket(
			std::bind(game::server_write::raidReadyCheckFinished, std::placeholders::_1));
	}

	void Player::handleRealmSplit(game::IncomingPacket & packet)
	{
		UInt32 preferred = 0;
		if (!(game::client_read::realmSplit(packet, preferred)))
		{
			return;
		}

		if (preferred == 0xFFFFFFFF)
		{
			// The player requests his preferred realm id from the server
			// TODO: Determine if a realm split is happening and if so, send SMSG_REALM_SPLIT message
		}
		else
		{
			DLOG("TODO...");
		}
	}

	void Player::handleVoiceSessionEnable(game::IncomingPacket & packet)
	{
		UInt16 unknown = 0;
		if (!(game::client_read::voiceSessionEnable(packet, unknown)))
		{
			return;
		}

		// TODO
	}

	void Player::handleCharRename(game::IncomingPacket & packet)
	{
		UInt64 characterId = 0;
		String newName;
		if (!(game::client_read::charRename(packet, characterId, newName)))
		{
			return;
		}

		// Rename character
		game::ResponseCode response = game::response_code::CharCreateError;
		for (auto &entry : m_characters)
		{
			if (entry.id == characterId)
			{
				// Capitalize the characters name
				capitalize(newName);

				response = m_database.renameCharacter(characterId, newName);
				if (response == game::response_code::Success)
				{
					// Fix entry
					entry.name = newName;
					entry.atLogin = static_cast<game::AtLoginFlags>(entry.atLogin & ~game::atlogin_flags::Rename);
				}

				break;
			}
		}

		// Send response
		sendPacket(
			std::bind(game::server_write::charRename, std::placeholders::_1, response, characterId, std::cref(newName)));
	}

	void Player::handleQuestQuery(game::IncomingPacket & packet)
	{
		UInt32 questId = 0;
		if (!(game::client_read::questQuery(packet, questId)))
		{
			return;
		}

		const auto *quest = m_project.quests.getById(questId);
		if (!quest)
		{
			return;
		}

		// Send response
		sendPacket(
			std::bind(game::server_write::questQueryResponse, std::placeholders::_1, std::cref(*quest)));
	}

	void Player::handleWho(game::IncomingPacket & packet)
	{
		// Timer protection
		GameTime now = getCurrentTime();
		if (now < m_nextWhoRequest)
		{
			// Don't do that yet
			return;
		}

		// Allow one request every 6 seconds
		m_nextWhoRequest = now + constants::OneSecond * 6;

		// Read request packet
		game::WhoListRequest request;
		if (!game::client_read::who(packet, request))
		{
			ILOG("Who Request does not match!");
			return;
		}

		// Check if the packet can be valid, as WoW allows a maximum of 10 zones and 4 strings
		// per request.
		if (request.zoneids.size() > 10 || request.strings.size() > 4)
		{
			return;
		}

		if (!m_gameCharacter)
			return;

		// Used for response packet
		game::WhoResponse response;
		const bool is_this_Alliance = ((game::race::Alliance & (1 << (m_gameCharacter->getRace() - 1))) == (1 << (m_gameCharacter->getRace() - 1)));

		// Iterate through all connected players
		for (auto &player : m_manager.getPlayers())
		{
			// Maximum of 50 allowed characters
			if (response.entries.size() >= 50)
			{
				// STOP the loop
				break;
			}

			// Check if the player has a valid game character
			auto *character = player->getGameCharacter();
			if (!character)
			{
				continue;
			}

			//is this player we are looking for alliance??
			const bool isAlliance = ((game::race::Alliance & (1 << (character->getRace() - 1))) == (1 << (character->getRace() - 1)));
			if(is_this_Alliance != isAlliance)
			{
				//cant find players that are not same faction
				continue;
			}

			// Check if that game character is currently in a world
			if (!player->getWorldNode())
			{
				continue;
			}

			// Check if levels match, first, as this is the least expensive thing to check
			const UInt32 level = character->getLevel();
			if (level < request.level_min || level > request.level_max)
			{
				continue;
			}

			//search by racemask
			if (0 != request.racemask)
			{
				UInt32 race = character->getRace();
				if (!(request.racemask & (1 << race)))
				{
					continue;
				}
			}

			//search by classmask
			if (0 != request.classmask)
			{
				UInt32 _class = character->getClass();
				if (!(request.classmask & (1 << _class)))
				{
					continue;
				}
			}

			//search by zoneID
			if(!request.zoneids.empty())
			{
				bool found_by_zoneid = false;

				for (auto zone : request.zoneids)
				{
					if(zone == character->getZone())
					{
						found_by_zoneid = true;
						break;
					}
				}

				if(found_by_zoneid == false)
				{
					continue;
				}
			}

			// Convert character name to lower case string
			String charNameLowered = character->getName();
			std::transform(charNameLowered.begin(), charNameLowered.end(), charNameLowered.begin(), ::tolower);

			// Validate character name
			if (!request.player_name.empty())
			{
				String searchLowered = request.player_name;
				std::transform(searchLowered.begin(), searchLowered.end(), searchLowered.begin(), ::tolower);

				// Part not found
				if (charNameLowered.find(searchLowered) == String::npos)
				{
					continue;
				}
			}

			// Validate strings
			bool passedStringTest = request.strings.empty();
			for (const auto &string : request.strings)
			{
				String searchLowered = string;
				std::transform(searchLowered.begin(), searchLowered.end(), searchLowered.begin(), ::tolower);
				
				// TODO: These strings also have to be validated against the guild name.
				// the players name

				//get zone name
				auto zone = m_project.zones.getById(character->getZone());
				auto zone_name = zone->name();
				std::transform(zone_name.begin(), zone_name.end(), zone_name.begin(), ::tolower);

				if ((charNameLowered.find(searchLowered) != String::npos) || (zone_name.find(searchLowered) != String::npos))
				{
					passedStringTest = true;
					break;
				}


			}

			// Skip this character if not passed string test
			if (!passedStringTest)
			{
				continue;
			}

			// Add new response entry, as all checks above were fulfilled
			WhoResponseEntry entry(*character);
			response.entries.push_back(std::move(entry));
		}

		// Match Count is request count right now (TODO)
		sendPacket(
			std::bind(game::server_write::whoRequestResponse, std::placeholders::_1, response, response.entries.size()));
	}

	void Player::handleMinimapPing(game::IncomingPacket & packet)
	{
		float x = 0.0f, y = 0.0f;
		if (!(game::client_read::minimapPing(packet, x, y)))
		{
			WLOG("Could not read minimap ping packet");
			return;
		}

		// Only accepted while in group
		if (!m_group)
		{
			WLOG("Player is not in a group");
			return;
		}

		// TODO: Only send to group members in the same map instance?

		std::vector<UInt64> excludeGuids;
		excludeGuids.push_back(m_gameCharacter->getGuid());

		// Broadcast packet to group
		auto generator = std::bind(game::server_write::minimapPing, std::placeholders::_1, m_gameCharacter->getGuid(), x, y);
		m_group->broadcastPacket(generator, &excludeGuids);
	}

	void Player::handleItemNameQuery(game::IncomingPacket & packet)
	{
		UInt32 itemEntry = 0;
		UInt64 itemGuid = 0;
		if (!(game::client_read::itemNameQuery(packet, itemEntry, itemGuid)))
		{
			return;
		}

		const auto *entry = m_project.items.getById(itemEntry);
		if (!entry)
		{
			return;
		}

		String name =
			entry->name_loc_size() >= static_cast<Int32>(m_locale) ?
			entry->name_loc(static_cast<Int32>(m_locale) - 1) : entry->name();
		if (name.empty()) name = entry->name();

		// Match Count is request count right now
		sendPacket(
			std::bind(game::server_write::itemNameQueryResponse, std::placeholders::_1, itemEntry, std::cref(name), entry->inventorytype()));
	}
}
