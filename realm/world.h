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

#include "wowpp_protocol/wowpp_protocol.h"
#include "wowpp_protocol/wowpp_connection.h"
#include "wowpp_protocol/wowpp_world_realm.h"
#include "game/game_character.h"
#include <boost/noncopyable.hpp>
#include <boost/signals2.hpp>
#include <algorithm>
#include <utility>
#include <cassert>

namespace wowpp
{
	// Forwards
	class WorldManager;
	class PlayerManager;
	class Project;
	struct IDatabase;

	/// World connection class.
	class World final
			: public pp::IConnectionListener
			, public boost::noncopyable
	{
	public:

		typedef AbstractConnection<pp::Protocol> Client;
		typedef boost::signals2::signal<void()> DisconnectedSignal;

	public:

		/// Triggered if the connection was lost or closed somehow.
		DisconnectedSignal onConnectionLost;

	public:

		/// 
		/// @param manager
		/// @param playerManager
		/// @param project
		/// @param connection
		/// @param address
		explicit World(WorldManager &manager,
						PlayerManager &playerManager,
						Project &project,
						IDatabase &database,
						std::shared_ptr<Client> connection,
						String address,
						String realmName);

		/// Gets the player connection class used to send packets to the client.
		Client &getConnection() { assert(m_connection); return *m_connection; }
		/// Gets the player manager which manages all connected worlds.
		WorldManager &getManager() const { return m_manager; }
		/// Determines whether a specific map id is supported by this world node.
		bool isMapSupported(UInt32 mapId) const { return std::find(m_mapIds.begin(), m_mapIds.end(), mapId) != m_mapIds.end(); }
		/// Determines whether a specific map instance is available on this world node.
		bool hasInstance(UInt32 instanceId) const { return std::find(m_instances.begin(), m_instances.end(), instanceId) != m_instances.end(); }
		/// Determines whether there are instances running on this world server.
		bool hasInstances() const { return !m_instances.empty(); }

		// Called by player
		void enterWorldInstance(DatabaseId characterDbId, const GameCharacter &character, const std::vector<pp::world_realm::ItemData> &out_items);
		void leaveWorldInstance(DatabaseId characterDbId, pp::world_realm::WorldLeftReason reason);
		void sendProxyPacket(DatabaseId characterId, UInt16 opCode, UInt32 size, const std::vector<char> &buffer);
		void sendChatMessage(NetUInt64 characterGuid, game::ChatMsg type, game::Language lang, const String &receiver, const String &channel, const String &message);

	private:

		// Variables
		WorldManager &m_manager;
		PlayerManager &m_playerManager;
		Project &m_project;
		IDatabase &m_database;
		std::shared_ptr<Client> m_connection;
		String m_address;						// IP address in string format
		bool m_authed;							// True if the user has been successfully authentificated.
		std::vector<UInt32> m_mapIds;			// A vector of map id's which this node does support
		std::vector<UInt32> m_instances;		// A vector of running instances on this server
		String m_realmName;

	private:

		/// Closes the connection if still connected.
		void destroy();
		
		/// @copydoc wow::game::IConnectionListener::connectionLost()
		void connectionLost() override;
		/// @copydoc wow::game::IConnectionListener::connectionMalformedPacket()
		void connectionMalformedPacket() override;
		/// @copydoc wow::game::IConnectionListener::connectionPacketReceived()
		void connectionPacketReceived(pp::IncomingPacket &packet) override;

		// Packet handlers
		void handleLogin(pp::IncomingPacket &packet);
		void handleWorldInstanceEntered(pp::IncomingPacket &packet);
		void handleWorldInstanceError(pp::IncomingPacket &packet);
		void handleWorldInstanceLeft(pp::IncomingPacket &packet);
		void handleClientProxyPacket(pp::IncomingPacket &packet);
		void handleCharacterData(pp::IncomingPacket &packet);
	};
}
