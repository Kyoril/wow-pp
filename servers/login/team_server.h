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

#include "common/typedefs.h"
#include "wowpp_protocol/wowpp_protocol.h"
#include "wowpp_protocol/wowpp_connection.h"

namespace wowpp
{
	class TeamServerManager;
	struct IDatabase;

	/// Player connection class.
	class TeamServer final
			: public pp::IConnectionListener
	{
	private:

		TeamServer(const TeamServer &Other) = delete;
		TeamServer &operator=(const TeamServer &Other) = delete;

	public:

		typedef AbstractConnection<pp::Protocol> Client;

	public:

		explicit TeamServer(TeamServerManager &manager,
						IDatabase &database,
		                std::shared_ptr<Client> connection,
						const String &address);

		/// Gets the player connection class used to send packets to the client.
		Client &getConnection() { ASSERT(m_connection); return *m_connection; }
		/// Gets the player manager which manages all connected players.
		TeamServerManager &getManager() const { return m_manager; }
		/// Determines whether this team server is authentificated.
		bool isAuthentificated() const { return m_authed; }
		/// Gets the internal team server name.
		const String &getInternalName() const { return m_name; }

	private:

		TeamServerManager &m_manager;
		IDatabase &m_database;
		std::shared_ptr<Client> m_connection;
		String m_address;						// IP address in string format
		String m_name;
		bool m_authed;							// True if the user has been successfully authentificated.

	private:

		/// Closes the connection if still connected.
		void destroy();

		/// @copydoc wow::auth::IConnectionListener::connectionLost()
		void connectionLost() override;
		/// @copydoc wow::auth::IConnectionListener::connectionMalformedPacket()
		void connectionMalformedPacket() override;
		/// @copydoc wow::auth::IConnectionListener::connectionPacketReceived()
		PacketParseResult connectionPacketReceived(pp::IncomingPacket &packet) override;

	private:

		void handleLogin(pp::IncomingPacket &packet);

		void handleEditorLoginRequest(pp::IncomingPacket &packet);
	};
}
