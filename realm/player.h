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

#pragma once

#include "game_protocol/game_protocol.h"
#include "game_protocol/game_connection.h"
#include "game_protocol/game_crypted_connection.h"
#include "wowpp_protocol/wowpp_world_realm.h"
#include "data/data_load_context.h"
#include "common/big_number.h"
#include "game/game_character.h"
#include <boost/noncopyable.hpp>
#include <boost/signals2.hpp>
#include <log/default_log_levels.h>
#include "player_social.h"
#include <algorithm>
#include <utility>
#include <cassert>
#include <limits>

namespace wowpp
{
	// Forwards
	class PlayerManager;
	class LoginConnector;
	class WorldManager;
	struct IDatabase;
	class Project;
	class World;
	struct Configuration;

	/// Player connection class.
	class Player final
			: public game::IConnectionListener
			, public boost::noncopyable
	{
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
						PlayerManager &manager,
						LoginConnector &loginConnector,
						WorldManager &worldManager,
						IDatabase &database,
						Project &project,
		                std::shared_ptr<Client> connection,
						const String &address);

		/// Determines whether a session status is valid.
		/// @status The session status to validate.
		/// @verbose True, if outputs to the log should be made.
		bool isSessionStatusValid(game::SessionStatus status, bool verbose = false) const;
		/// This should be called right after a new player connected.
		void sendAuthChallenge();
		/// The login server notified us that the player login was successfull.
		/// @param accountId The account id of the player which is logging in.
		/// @param key Sesskion key (K) of the client which was calculated by the login server.
		/// @param v V value which was calculated by the login server.
		/// @param s S value which was calculated by the login server.
		void loginSucceeded(UInt32 accountId, const BigNumber &key, const BigNumber &v, const BigNumber &s);
		/// The login server notified us that the player login failed. This can have several
		/// different reasons (unknown account name, suspended account etc.),
		void loginFailed();
		/// A world server notified us that our character instance was successfully spawned in the
		/// world and will now be visible to other players.
		void worldInstanceEntered(World &world, UInt32 instanceId, UInt64 worldObjectGuid, UInt32 mapId, UInt32 zoneId, float x, float y, float z, float o);
		/// 
		void worldInstanceLeft(World &world, UInt32 instanceId, pp::world_realm::WorldLeftReason reason);
		
		/// Gets the player connection class used to send packets to the client.
		Client &getConnection() { assert(m_connection); return *m_connection; }
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

	private:

		Configuration &m_config;
		PlayerManager &m_manager;
		LoginConnector &m_loginConnector;
		WorldManager &m_worldManager;
		IDatabase &m_database;
		Project &m_project;
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
		std::unique_ptr<GameCharacter> m_gameCharacter;
		UInt32 m_instanceId;
		DataLoadContext::GetRace m_getRace;
		DataLoadContext::GetClass m_getClass;
		DataLoadContext::GetLevel m_getLevel;
		boost::signals2::scoped_connection m_worldDisconnected;
		UInt32 m_timeSyncCounter;
		World *m_worldNode;
		std::unique_ptr<PlayerSocial> m_social;

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
		void handleFriendList(game::IncomingPacket &packet);
		void handleAddFriend(game::IncomingPacket &packet);
		void handleDeleteFriend(game::IncomingPacket &packet);
		void handleAddIgnore(game::IncomingPacket &packet);
		void handleDeleteIgnore(game::IncomingPacket &packet);
	};
}
