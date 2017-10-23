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

#pragma once

#include "game_protocol/game_protocol.h"
#include "game_protocol/game_connection.h"
#include "game_protocol/game_crypted_connection.h"
#include "auth_protocol/auth_protocol.h"
#include "wowpp_protocol/wowpp_world_realm.h"
#include "common/big_number.h"
#include "common/id_generator.h"
#include "game/game_character.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	// Forwards
	class PlayerManager;
	class LoginConnector;
	class WorldManager;
	struct IDatabase;
	class World;
	struct Configuration;
	class PlayerSocial;
	class PlayerGroup;
	namespace proto
	{
		class Project;
	}

	namespace add_item_result
	{
		enum Type
		{
			/// Successfully added item.
			Success,
			/// Item could not be found.
			ItemNotFound,
			/// Players bag is full.
			BagIsFull,
			/// Player has too many instances of that item.
			TooManyItems,
			/// Unknown error.
			Unknown
		};
	}

	typedef add_item_result::Type AddItemResult;

	namespace learn_spell_result
	{
		enum Type
		{
			/// Successfully added item.
			Success,
			/// Player already knows that spell.
			AlreadyLearned,
			/// Unknown error.
			Unknown
		};
	}

	typedef learn_spell_result::Type LearnSpellResult;

	/// Player connection class.
	class Player final
			: public game::IConnectionListener
			, public std::enable_shared_from_this<Player>
	{
	private:

		Player(const Player &Other) = delete;
		Player &operator=(const Player &Other) = delete;

	public:

		typedef AbstractConnection<game::Protocol> Client;

	public:

		/// Creates a new instance of the player class and registers itself as the
		/// active connection listener.
		/// @param manager Reference to the player manager which manages all connected players on this realm.
		/// @param loginConnector Reference to the login connector for communication with the login server.
		/// @param worldManager Reference to the world manager which manages all connected world nodes.
		/// @param database Reference to the database system.
		/// @param project Reference to the data project.
		/// @param connection The connection instance as a shared pointer.
		/// @param address The remote address of the player (ip address) as string.
		explicit Player(Configuration &config,
						IdGenerator<UInt64> &groupIdGenerator,
						PlayerManager &manager,
						LoginConnector &loginConnector,
						WorldManager &worldManager,
						IDatabase &database,
						proto::Project &project,
		                std::shared_ptr<Client> connection,
						const String &address);

		/// Determines whether a session status is valid.
		/// @name Name of the packet.
		/// @status The session status to validate.
		/// @verbose True, if outputs to the log should be made.
		bool isSessionStatusValid(const String &name, game::SessionStatus status, bool verbose = false) const;
		/// This should be called right after a new player connected.
		void sendAuthChallenge();
		/// The login server notified us that the player login was successfull.
		/// @param accountId The account id of the player which is logging in.
		/// @param key Sesskion key (K) of the client which was calculated by the login server.
		/// @param v V value which was calculated by the login server.
		/// @param s S value which was calculated by the login server.
		void loginSucceeded(UInt32 accountId, auth::AuthLocale locale, const BigNumber &key, const BigNumber &v, const BigNumber &s, const std::array<UInt32, 8> &tutorialData);
		/// The login server notified us that the player login failed. This can have several
		/// different reasons (unknown account name, suspended account etc.),
		void loginFailed();
		/// A world server notified us that our character instance was successfully spawned in the
		/// world and will now be visible to other players.
		void worldInstanceEntered(World &world, UInt32 instanceId, UInt64 worldObjectGuid, UInt32 mapId, UInt32 zoneId, math::Vector3 location, float o);
		/// 
		void worldInstanceLeft(World &world, UInt32 instanceId, pp::world_realm::WorldLeftReason reason);
		/// Saves the current character (if any).
		//void saveCharacter();
		/// Inititalizes a character transfer to a new map.
		bool initializeTransfer(UInt32 map, math::Vector3 location, float o, bool shouldLeaveNode = false);
		/// Commits an initialized transfer (if any).
		void commitTransfer();

		/// Gets the player connection class used to send packets to the client.
		Client &getConnection() { ASSERT(m_connection); return *m_connection; }
		/// Gets the player manager which manages all connected players.
		PlayerManager &getManager() const { return m_manager; }
		/// Gets the user name sent by the client.
		const String &getAccountName() const { return m_accountName; }
		/// Gets the character id.
		DatabaseId getCharacterId() const { return m_characterId; }
		/// Gets the world object id of this character.
		UInt64 getWorldObjectId() const { return (m_gameCharacter ? m_gameCharacter->getGuid() : 0); }
		/// 
		GameCharacter *getGameCharacter() const { return m_gameCharacter.get(); }
		/// 
		PlayerSocial &getSocial() { return *m_social; }
		/// Returns the players group (or nullptr, if the player is not in a group).
		PlayerGroup *getGroup() { return m_group.get(); }
		/// Sets the players new group.
		void setGroup(std::shared_ptr<PlayerGroup> group);
		/// 
		UInt32 getWorldInstanceId() const { return m_instanceId; }
		/// Gets the connected world node
		World *getWorldNode() { return m_worldNode; }
		///
		std::list<Mail> &getMails() { return m_mails; }
		///
		Mail *getMail(UInt32 mailId);

		/// Declines a pending group invite (if available).
		void declineGroupInvite();
		/// 
		void reloadCharacters();
		/// 
		void spawnedNotification();
		/// Called when character receives a (valid) mail
		void mailReceived(Mail mail);
		/// Called when character reads a mail
		void readMail();

		/// Sends an encrypted packet to the game client
		/// @param generator Packet writer function pointer.
		template<class F>
		void sendPacket(F generator)
		{
			// Write native packet
			wowpp::Buffer &sendBuffer = m_connection->getSendBuffer();
			io::StringSink sink(sendBuffer);

			// Get the end of the buffer (needed for encryption)
			size_t bufferPos = sink.position();

			typename game::Protocol::OutgoingPacket packet(sink);
			generator(packet);

			// Crypt packet header
			game::Connection *cryptCon = static_cast<game::Connection*>(m_connection.get());
			cryptCon->getCrypt().encryptSend(reinterpret_cast<UInt8*>(&sendBuffer[bufferPos]), game::Crypt::CryptedSendLength);

			// Flush buffers
			m_connection->flush();
		}

		/// Redirects a packet which was received by a world server to the client and also
		/// encrypts it's header (which can only be done here since only we have the crypt 
		/// data, not the world server).
		/// @param opCode The packet's identifier.
		/// @param buffer The packet content buffer which also includes the op code and the packet size.
		void sendProxyPacket(UInt16 opCode, const std::vector<char> &buffer);

	public:

		/// Adds a new item to the players inventory if logged in.
		AddItemResult addItem(UInt32 itemId, UInt32 amount);
		/// Learns a new spell and notifies the world node about this.
		LearnSpellResult learnSpell(const proto::SpellEntry &spell);

	private:

		Configuration &m_config;
		IdGenerator<UInt64> &m_groupIdGenerator;
		PlayerManager &m_manager;
		LoginConnector &m_loginConnector;
		WorldManager &m_worldManager;
		IDatabase &m_database;
		proto::Project &m_project;
		std::shared_ptr<Client> m_connection;
		String m_address;								// IP address in string format
		String m_accountName;
		UInt32 m_seed;
		UInt32 m_clientSeed;
		SHA1Hash m_clientHash;
		game::AddonEntries m_addons;
		game::CharEntries m_characters;
		bool m_authed;									// True if the user has been successfully authentificated.
		UInt32 m_accountId;
		BigNumber m_sessionKey;
		BigNumber m_v;
		BigNumber m_s;
		DatabaseId m_characterId;
		std::shared_ptr<GameCharacter> m_gameCharacter;
		UInt32 m_instanceId;
		simple::scoped_connection m_worldDisconnected;
		UInt32 m_timeSyncCounter;
		World *m_worldNode;
		std::unique_ptr<PlayerSocial> m_social;
		std::shared_ptr<PlayerGroup> m_group;
		UInt32 m_transferMap;
		math::Vector3 m_transfer;
		float m_transferO;
		ActionButtons m_actionButtons;
		std::array<UInt32, 8> m_tutorialData;
		GameTime m_nextWhoRequest;
		auth::AuthLocale m_locale;
		std::list<Mail> m_mails;
		IdGenerator<UInt32> m_mailIdGenerator;
		UInt8 m_unreadMails;

	private:

		/// Gets a character of this account by it's id.
		game::CharEntry *getCharacterById(DatabaseId databaseId);
		/// Called if the world node where the character is logged in with disconnected.
		void worldNodeDisconnected();

	private:

		/// Closes the connection if still connected.
		void destroy();
		
		/// @copydoc wow::game::IConnectionListener::connectionLost()
		void connectionLost() override;
		/// @copydoc wow::game::IConnectionListener::connectionMalformedPacket()
		void connectionMalformedPacket() override;
		/// @copydoc wow::game::IConnectionListener::connectionPacketReceived()
		void connectionPacketReceived(game::IncomingPacket &packet) override;

		// Packet handlers
		void handlePing(game::IncomingPacket &packet);
		void handleAuthSession(game::IncomingPacket &packet);
		void handleCharEnum(game::IncomingPacket &packet);
		void handleCharCreate(game::IncomingPacket &packet);
		void handleCharDelete(game::IncomingPacket &packet);
		void handlePlayerLogin(game::IncomingPacket &packet);
		void handleMessageChat(game::IncomingPacket &packet);
		void handleNameQuery(game::IncomingPacket &packet);
		void handleContactList(game::IncomingPacket &packet);
		void handleAddFriend(game::IncomingPacket &packet);
		void handleDeleteFriend(game::IncomingPacket &packet);
		void handleAddIgnore(game::IncomingPacket &packet);
		void handleDeleteIgnore(game::IncomingPacket &packet);
		void handleItemQuerySingle(game::IncomingPacket &packet);
		void handleGroupInvite(game::IncomingPacket &packet);
		void handleGroupAccept(game::IncomingPacket &packet);
		void handleGroupDecline(game::IncomingPacket &packet);
		void handleGroupUninvite(game::IncomingPacket &packet);
		void handleGroupUninviteGUID(game::IncomingPacket &packet);
		void handleGroupSetLeader(game::IncomingPacket &packet);
		void handleLootMethod(game::IncomingPacket &packet);
		void handleGroupDisband(game::IncomingPacket &packet);
		void handleRequestPartyMemberStats(game::IncomingPacket &packet);
		void handleMoveWorldPortAck(game::IncomingPacket &packet);
		void handleSetActionButton(game::IncomingPacket &packet);
		void handleGameObjectQuery(game::IncomingPacket &packet);
		void handleTutorialFlag(game::IncomingPacket &packet);
		void handleTutorialClear(game::IncomingPacket &packet);
		void handleTutorialReset(game::IncomingPacket &packet);
		void handleCompleteCinematic(game::IncomingPacket &packet);
		void handleRaidTargetUpdate(game::IncomingPacket &packet);
		void handleGroupRaidConvert(game::IncomingPacket &packet);
		void handleGroupAssistentLeader(game::IncomingPacket &packet);
		void handleRaidReadyCheck(game::IncomingPacket &packet);
		void handleRaidReadyCheckFinished(game::IncomingPacket &packet);
		void handleRealmSplit(game::IncomingPacket &packet);
		void handleVoiceSessionEnable(game::IncomingPacket &packet);
		void handleCharRename(game::IncomingPacket &packet);
		void handleQuestQuery(game::IncomingPacket &packet);
		void handleWho(game::IncomingPacket &packet);
		void handleMinimapPing(game::IncomingPacket &packet);
		void handleItemNameQuery(game::IncomingPacket &packet);
		void handleCreatureQuery(game::IncomingPacket &packet);
		void handleMailQueryNextTime(game::IncomingPacket &packet);
		void handleMailGetBody(game::IncomingPacket &packet);
		void handleMailTakeMoney(game::IncomingPacket &packet);
		void handleMailDelete(game::IncomingPacket &packet);
		void handleGetChannelMemberCount(game::IncomingPacket &packet);
	};
}
