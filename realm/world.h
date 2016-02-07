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

#include "wowpp_protocol/wowpp_protocol.h"
#include "wowpp_protocol/wowpp_connection.h"
#include "wowpp_protocol/wowpp_world_realm.h"
#include "game/game_character.h"
#include <boost/noncopyable.hpp>
#include <boost/signals2.hpp>
#include "common/linear_set.h"
#include <algorithm>
#include <utility>
#include <cassert>

namespace wowpp
{
	// Forwards
	class WorldManager;
	class PlayerManager;
	struct IDatabase;
	namespace proto
	{
		class Project;
	}

	/// World connection class.
	class World final
			: public pp::IConnectionListener
			, public boost::noncopyable
	{
	public:

		typedef AbstractConnection<pp::Protocol> Client;
		typedef boost::signals2::signal<void()> DisconnectedSignal;

		typedef std::vector<UInt32> MapList;
		typedef LinearSet<UInt32> InstanceList;

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
						proto::Project &project,
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
		/// Returns an array of supported map id's of this world node.
		const MapList &getMapList() const { return m_mapIds; }
		/// Returns an array of opened instance id's of this world node.
		const InstanceList &getInstanceList() const { return m_instances; }
		
		// Called by player
		void enterWorldInstance(UInt64 characterDbId, UInt32 instanceId, const GameCharacter &character, const std::vector<ItemData> &out_items);
		void leaveWorldInstance(UInt64 characterDbId, pp::world_realm::WorldLeftReason reason);
		void sendProxyPacket(UInt64 characterGuid, UInt16 opCode, UInt32 size, const std::vector<char> &buffer);
		void sendChatMessage(UInt64 characterGuid, game::ChatMsg type, game::Language lang, const String &receiver, const String &channel, const String &message);
		void characterGroupChanged(UInt64 characterGuid, UInt64 groupId);
		void characterIgnoreList(UInt64 characterGuid, const std::vector<UInt64> &list);
		void characterAddIgnore(UInt64 characterGuid, UInt64 ignoreGuid);
		void characterRemoveIgnore(UInt64 characterGuid, UInt64 removeGuid);
		void itemData(UInt64 characterGuid, const std::vector<ItemData> &items);

	private:

		// Variables
		WorldManager &m_manager;
		PlayerManager &m_playerManager;
		proto::Project &m_project;
		IDatabase &m_database;
		std::shared_ptr<Client> m_connection;
		String m_address;						// IP address in string format
		bool m_authed;							// True if the user has been successfully authentificated.
		MapList m_mapIds;			// A vector of map id's which this node does support
		InstanceList m_instances;		// A vector of running instances on this server
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
		void handleTeleportRequest(pp::IncomingPacket &packet);
		void handleCharacterGroupUpdate(pp::IncomingPacket &packet);
		void handleQuestUpdate(pp::IncomingPacket &packet);
	};
}
