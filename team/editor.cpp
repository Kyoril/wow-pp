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
#include "editor.h"
#include "editor_manager.h"
#include "wowpp_protocol/wowpp_realm_login.h"
#include "log/default_log_levels.h"

using namespace std;

namespace wowpp
{
	Editor::Editor(EditorManager &manager, std::shared_ptr<Client> connection, const String &address)
		: m_manager(manager)
		, m_connection(std::move(connection))
		, m_address(address)
		, m_authed(false)
	{
		assert(m_connection);

		m_connection->setListener(*this);
	}

	void Editor::connectionLost()
	{
		WLOG("Editor " << (m_authed ? m_name : m_address) << " disconnected");
		destroy();
	}

	void Editor::destroy()
	{
		m_connection->resetListener();
		m_connection.reset();

		m_manager.editorDisconnected(*this);
	}

	void Editor::connectionMalformedPacket()
	{
		WLOG("Editor " << m_address << " sent malformed packet");
		destroy();
	}

	void Editor::connectionPacketReceived(pp::IncomingPacket &packet)
	{
		const auto packetId = packet.getId();

		switch (packetId)
		{
			case pp::realm_login::realm_packet::Login:
			{
				handleLogin(packet);
				break;
			}

			default:
			{
				// Log unknown or unhandled packet
				WLOG("Received unknown packet " << static_cast<UInt32>(packetId)
					<< " from editor at " << m_address);
				break;
			}
		}
	}

	void Editor::handleLogin(pp::IncomingPacket &packet)
	{
		using namespace pp::realm_login;
	}
}
