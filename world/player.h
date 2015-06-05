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
#include "data/data_load_context.h"
#include "common/big_number.h"
#include "common/countdown.h"
#include "game/game_character.h"
#include "binary_io/vector_sink.h"
#include "realm_connector.h"
#include <boost/noncopyable.hpp>
#include <boost/signals2.hpp>
#include <algorithm>
#include <utility>
#include <cassert>
#include <limits>

namespace wowpp
{
	// Forwards
	class PlayerManager;
	class WorldInstanceManager;
	class WorldInstance;

	/// Player connection class.
	class Player final : public boost::noncopyable
	{
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
		explicit Player(PlayerManager &manager,
						RealmConnector &realmConnector,
						WorldInstanceManager &worldInstanceManager,
						DatabaseId characterId,
						std::shared_ptr<GameCharacter> character,
						WorldInstance &instance);

		/// Gets the player manager which manages all connected players.
		PlayerManager &getManager() const { return m_manager; }
		/// Gets the database identifier of the character (which is provided by the realm).
		const DatabaseId &getCharacterId() const { return m_characterId; }
		/// Gets the guid of the character in the world (which is provided by this world server).
		const UInt64 getCharacterGuid() const { return m_character->getGuid(); }
		/// 
		std::shared_ptr<GameCharacter> getCharacter() { return m_character; }
		/// 
		WorldInstance &getWorldInstance() { return m_instance; }

		/// Starts the 20 second logout timer.
		void logoutRequest();
		/// Cancels the 20 second logout timer if it has been set up.
		void cancelLogoutRequest();

		/// Sends an proxy packet to the realm which will then be redirected to the game client.
		/// @param generator Packet writer function pointer.
		template<class F>
		void sendProxyPacket(F generator) const
		{
			std::vector<char> buffer;
			io::VectorSink sink(buffer);

			typename game::Protocol::OutgoingPacket packet(sink);
			generator(packet);

			// Send the proxy packet to the realm server
			m_realmConnector.sendProxyPacket(m_characterId, packet.getOpCode(), packet.getSize(), buffer);
		}

	private:

		void onLogout();

	private:

		PlayerManager &m_manager;
		RealmConnector &m_realmConnector;
		WorldInstanceManager &m_worldInstanceManager;
		DatabaseId m_characterId;
		std::shared_ptr<GameCharacter> m_character;
		Countdown m_logoutCountdown;
		WorldInstance &m_instance;
	};
}
