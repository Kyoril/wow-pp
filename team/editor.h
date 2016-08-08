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
#include "wowpp_protocol/wowpp_editor_team.h"
#include "proto_data/project.h"

namespace wowpp
{
	class EditorManager;
	class LoginConnector;

	/// Editor connection class.
	class Editor final
		: public pp::IConnectionListener
		, public boost::noncopyable
		, public std::enable_shared_from_this<Editor>
	{
	public:

		typedef AbstractConnection<pp::Protocol> Client;

	public:

		explicit Editor(EditorManager &manager,
			LoginConnector &loginConnector,
			proto::Project &project,
			std::shared_ptr<Client> connection,
			const String &address);

		/// Gets the editor connection class used to send packets to the client.
		Client &getConnection() { assert(m_connection); return *m_connection; }
		/// Gets the editor manager which manages all connected editors.
		EditorManager &getManager() const { return m_manager; }
		/// Determines whether this realm is authentificated.
		bool isAuthentificated() const { return m_authed; }
		/// Determines the editors account name which was used to sign in if authentificated.
		/// This returns an empty string if not yet authentificated.
		const String &getName() const { return m_name; }
		/// Called when the authentification request was confirmed by the login server.
		void onAuthResult(pp::editor_team::LoginResult result);

		/// Sends a packet to the editor.
		/// @param generator Packet writer function pointer.
		template<class F>
		void sendPacket(F generator)
		{
			// Write native packet
			wowpp::Buffer &sendBuffer = m_connection->getSendBuffer();
			io::StringSink sink(sendBuffer);

			// Get the end of the buffer (needed for encryption)
			size_t bufferPos = sink.position();

			typename pp::OutgoingPacket packet(sink);
			generator(packet);

			// Flush buffers
			m_connection->flush();
		}

	private:

		EditorManager &m_manager;
		LoginConnector &m_loginConnector;
		proto::Project &m_project;
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
		void connectionPacketReceived(pp::IncomingPacket &packet) override;

	private:

		void handleLogin(pp::IncomingPacket &packet);
		void handleProjectHashMap(pp::IncomingPacket &packet);
	};
}
