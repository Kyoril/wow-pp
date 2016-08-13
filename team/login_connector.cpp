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
#include "login_connector.h"
#include "editor.h"
#include "wowpp_protocol/wowpp_team_login.h"
#include "wowpp_protocol/wowpp_editor_team.h"
#include "configuration.h"
#include "common/clock.h"
#include "common/make_unique.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	static const auto ReconnectDelay = (constants::OneSecond * 4);
	static const auto KeepAliveDelay = (constants::OneMinute / 2);

	LoginConnector::LoginConnector(boost::asio::io_service &ioService, const Configuration &config, TimerQueue &timer)
		: m_ioService(ioService)
		, m_config(config)
		, m_timer(timer)
		, m_host(m_config.loginAddress)
		, m_port(m_config.loginPort)
	{
		tryConnect();
	}

	LoginConnector::~LoginConnector()
	{
	}

	void LoginConnector::connectionLost()
	{
		WLOG("Lost connection with the login server at " << m_host << ":" << m_port);
		m_connection->resetListener();
		m_connection.reset();
		scheduleConnect();
	}

	void LoginConnector::connectionMalformedPacket()
	{
		WLOG("Received malformed packet from the login server");
		m_connection->resetListener();
		m_connection.reset();
	}

	void LoginConnector::connectionPacketReceived(pp::Protocol::IncomingPacket &packet)
	{
		const auto &packetId = packet.getId();

		switch (packetId)
		{
			case pp::team_login::login_packet::LoginResult:
			{
				handleLoginResult(packet);
				break;
			}

			case pp::team_login::login_packet::EditorLoginResult:
			{
				handleEditorLoginResult(packet);
				break;
			}

			default:
			{
				// Log about unknown or unhandled packet
				WLOG("Received unknown packet " << static_cast<UInt32>(packetId)
					<< " from login server at " << m_host << ":" << m_port);
				break;
			}
		}
	}

	bool LoginConnector::connectionEstablished(bool success)
	{
		if (success)
		{
			ILOG("Connected to the login server");

			io::StringSink sink(m_connection->getSendBuffer());
			pp::OutgoingPacket packet(sink);

			// Write packet structure
			pp::team_login::team_write::login(packet,
				m_config.internalName,
				m_config.password);

			m_connection->flush();
			scheduleKeepAlive();
		}
		else
		{
			WLOG("Could not connect to the login server at " << m_host << ":" << m_port);
			scheduleConnect();
		}

		return true;
	}

	void LoginConnector::editorLoginRequest(std::shared_ptr<Editor> connection, const SHA1Hash & password)
	{
		io::StringSink sink(m_connection->getSendBuffer());
		pp::OutgoingPacket packet(sink);

		const String &name = connection->getName();

		// Look for login requests
		auto it = m_loginRequests.find(name);
		if (it != m_loginRequests.end())
		{
			// Timed out or already disconnected?
			if (it->second->creationTime < getCurrentTime() - constants::OneSecond * 15 ||
				!it->second->getConnection())
			{
				m_loginRequests.erase(it);
			}
			else
			{
				WLOG("TODO: Login already pending - ignored");

				// Oops, login already pending
				return;
			}
		}

		// Create a new login request object
		auto request = make_unique<EditorLoginRequest>(connection);
		request->creationTime = getCurrentTime();
		m_loginRequests[name] = std::move(request);

		// Write packet structure
		pp::team_login::team_write::editorLoginRequest(packet,
			connection->getName(),
			password);

		m_connection->flush();
	}

	void LoginConnector::scheduleConnect()
	{
		const GameTime now = getCurrentTime();
		m_timer.addEvent(
			std::bind(&LoginConnector::tryConnect, this), now + ReconnectDelay);
	}

	void LoginConnector::tryConnect()
	{
		m_connection = pp::Connector::create(m_ioService);
		m_connection->connect(m_host, m_port, *this, m_ioService);
	}

	void LoginConnector::scheduleKeepAlive()
	{

	}

	void LoginConnector::onScheduledKeepAlive()
	{

	}

	void LoginConnector::handleLoginResult(pp::Protocol::IncomingPacket &packet)
	{
		using namespace pp::team_login;

		// Read packet contents
		LoginResult result;
		UInt32 loginProtocolVersion = 0xffffffff;
		if (!login_read::loginResult(packet, result, loginProtocolVersion))
		{
			return;
		}

		// Compare protocol version
		if (loginProtocolVersion != pp::team_login::ProtocolVersion)
		{
			WLOG("Incompatible login server protocol detected! Please update...");
			return;
		}

		// Display result
		switch (result)
		{
			case login_result::Success:
			{
				ILOG("Successfully logged in at login server");
				break;
			}

			case login_result::UnknownTeamServer:
			{
				ELOG("Login server does not know this team server - invalid name");
				break;
			}

			case login_result::WrongPassword:
			{
				ELOG("Invalid password for this team server");
				break;
			}

			case login_result::AlreadyLoggedIn:
			{
				ELOG("A team server using this credentials is already logged in");
				break;
			}

			case login_result::ServerError:
			{
				ELOG("Internal server error at the login server");
				break;
			}

			default:
			{
				ELOG("Unknown login result: " << result);
				break;
			}
		}
	}

	void LoginConnector::handleEditorLoginResult(pp::Protocol::IncomingPacket & packet)
	{
		using namespace pp::team_login;

		// Read packet contents
		String username;
		EditorLoginResult result;
		if (!login_read::editorLoginResult(packet, username, result))
		{
			return;
		}

		// Look for login requests
		auto it = m_loginRequests.find(username);
		if (it == m_loginRequests.end())
		{
			WLOG("Could not find pending login request for account " << username);
			return;
		}

		// If the associated connection is no longer available
		auto connection = it->second->getConnection();

		// Remove login request from the list of requests as we already
		m_loginRequests.erase(it);

		// Check connection
		if (!connection)
		{
			WLOG("Editor is no longer connected and thus login request can't be processed");
			return;
		}

		// Display result
		switch (result)
		{
			case editor_login_result::Success:
			{
				ILOG("AUTH confirmed by login server");
				connection->onAuthResult(pp::editor_team::login_result::Success);
				break;
			}

			case editor_login_result::UnknownUserName:
			{
				ELOG("Login server does not know this account");
				connection->onAuthResult(pp::editor_team::login_result::WrongUserName);
				break;
			}

			case editor_login_result::WrongPassword:
			{
				ELOG("Invalid password");
				connection->onAuthResult(pp::editor_team::login_result::WrongPassword);
				break;
			}

			case editor_login_result::AlreadyLoggedIn:
			{
				ELOG("A user using this credentials is already logged in");
				connection->onAuthResult(pp::editor_team::login_result::AlreadyLoggedIn);
				break;
			}

			case editor_login_result::ServerError:
			{
				ELOG("Internal server error at the login server");
				connection->onAuthResult(pp::editor_team::login_result::ServerError);
				break;
			}

			default:
			{
				ELOG("Unknown login result: " << result);
				connection->onAuthResult(pp::editor_team::login_result::ServerError);
				break;
			}
		}
	}
}
