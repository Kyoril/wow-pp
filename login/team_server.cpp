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
#include "team_server.h"
#include "team_server_manager.h"
#include "wowpp_protocol/wowpp_team_login.h"
#include "common/sha1.h"
#include "log/default_log_levels.h"
#include "database.h"

using namespace std;

namespace wowpp
{
	TeamServer::TeamServer(TeamServerManager &manager, IDatabase &database, std::shared_ptr<Client> connection, const String &address)
		: m_manager(manager)
		, m_database(database)
		, m_connection(std::move(connection))
		, m_address(address)
		, m_authed(false)
	{
		assert(m_connection);

		m_connection->setListener(*this);
	}

	void TeamServer::connectionLost()
	{
		WLOG("Team server " << (m_authed ? m_name : m_address) << " disconnected");
		destroy();
	}

	void TeamServer::destroy()
	{
		m_connection->resetListener();
		m_connection.reset();

		m_manager.teamServerDisconnected(*this);
	}

	void TeamServer::connectionMalformedPacket()
	{
		WLOG("Team server " << m_address << " sent malformed packet");
		destroy();
	}

	void TeamServer::connectionPacketReceived(pp::IncomingPacket &packet)
	{
		const auto packetId = packet.getId();

		switch (packetId)
		{
			case pp::team_login::team_packet::Login:
			{
				handleLogin(packet);
				break;
			}

			case pp::team_login::team_packet::EditorLoginRequest:
			{
				handleEditorLoginRequest(packet);
				break;
			}

			default:
			{
				// Log unknown or unhandled packet
				WLOG("Received unknown packet " << static_cast<UInt32>(packetId) 
					<< " from team server at " << m_address);
				break;
			}
		}
	}

	void TeamServer::handleLogin(pp::IncomingPacket &packet)
	{
		if (m_authed)
		{
			WLOG("Team server is already logged in");
			return;
		}

		using namespace pp::team_login;

		String internalName, password;
		if (!team_read::login(packet,
			internalName,
			std::numeric_limits<UInt8>::max(),
			password,
			std::numeric_limits<UInt8>::max()))
		{
			return;
		}

		m_authed = false;

		// Check for a realm using this name already
		LoginResult result = login_result::AlreadyLoggedIn;
		if (!m_manager.getTeamServerByInternalName(internalName))
		{
			// Try to log in
			UInt32 teamId = 0;
			result = m_database.teamServerLogIn(teamId, internalName, password);
			if (result == login_result::Success)
			{
				// Save internal name
				m_name = internalName;

				ILOG("Team server " << m_name << " successfully authenticated");

				// Team server is authentificated
				m_authed = true;
			}
		}

		// Send realm answer
		m_connection->sendSinglePacket(
			std::bind(
				login_write::loginResult, std::placeholders::_1, result));
	}

	void TeamServer::handleEditorLoginRequest(pp::IncomingPacket & packet)
	{
		if (!m_authed)
			return;

		using namespace pp::team_login;

		String internalName;
		SHA1Hash password;
		if (!team_read::editorLoginRequest(packet,
			internalName,
			std::numeric_limits<UInt8>::max(),
			password))
		{
			return;
		}

		pp::team_login::EditorLoginResult result = pp::team_login::editor_login_result::ServerError;

		// Read account data from the database
		UInt32 id = 0;
		String hash;
		if (!m_database.getPlayerPassword(internalName, id, hash))
		{
			result = pp::team_login::editor_login_result::UnknownUserName;
		}
		else
		{
			// Uppercase password hash
			std::transform(hash.begin(), hash.end(), hash.begin(), ::toupper);

			// Found data
			if (id != 0 && !hash.empty())
			{
				// Check if the passwords match
				std::stringstream strm;
				sha1PrintHex(strm, password);

				String passString = strm.str();
				std::transform(passString.begin(), passString.end(), passString.begin(), ::toupper);

				if (passString == hash)
				{
					result = pp::team_login::editor_login_result::Success;
				}
				else
				{
					result = pp::team_login::editor_login_result::WrongPassword;
				}
			}
			else
			{
				result = pp::team_login::editor_login_result::UnknownUserName;
			}
		}

		// Send answer
		m_connection->sendSinglePacket(
			std::bind(
				login_write::editorLoginResult, std::placeholders::_1, result));
	}
}
