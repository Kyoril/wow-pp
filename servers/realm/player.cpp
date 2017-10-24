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
#include "common/weak_ptr_function.h"

using namespace std;
using namespace wowpp::game;

namespace wowpp
{
	Player::Player(Configuration &config, 
		           IdGenerator<UInt64> &groupIdGenerator, 
		           PlayerManager &manager, 
		           LoginConnector &loginConnector, 
		           WorldManager &worldManager, 
		           IDatabase &database,
		           AsyncDatabase &asyncDatabase,
		           proto::Project &project, 
		           std::shared_ptr<Client> connection,
		           const String &address)
		: m_config(config)
		, m_groupIdGenerator(groupIdGenerator)
		, m_manager(manager)
		, m_loginConnector(loginConnector)
		, m_worldManager(worldManager)
		, m_database(database)
		, m_asyncDatabase(asyncDatabase)
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

	void Player::handleCharacterList(const boost::optional<game::CharEntries>& result)
	{
		if (!result)
		{
			destroy();
			return;
		}

		// Copy character list
		m_characters = result.get();
		for (auto &c : m_characters)
		{
			c.id = createRealmGUID(guidLowerPart(c.id), m_loginConnector.getRealmID(), guid_type::Player);
		}

		// Send character list
		sendPacket(
			std::bind(game::server_write::charEnum, std::placeholders::_1, std::cref(m_characters)));
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
			info.flags = game::Friend;
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
			info.flags = game::Friend;
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
				info.flags = game::Friend;
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

		// Callback handler
		auto handler = 
			std::bind<void>(
				bind_weak_ptr_1<Player>(
					shared_from_this(),
					std::bind(&Player::handleCharacterList, std::placeholders::_1, std::placeholders::_2)),		// Player Instance, Result
				std::placeholders::_1);	// Player instance

		// Start request
		m_asyncDatabase.asyncRequest(std::move(handler), &IDatabase::getCharacters, m_accountId);
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
}
