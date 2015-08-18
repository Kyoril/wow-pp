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

#include "login_connector.h"
#include "wowpp_protocol/wowpp_realm_login.h"
#include "configuration.h"
#include "player_manager.h"
#include "player.h"
#include "common/clock.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	static const auto ReconnectDelay = (constants::OneSecond * 4);
	static const auto KeepAliveDelay = (constants::OneMinute / 2);


	LoginConnector::LoginConnector(boost::asio::io_service &ioService, PlayerManager &playerManager, const Configuration &config, TimerQueue &timer)
		: m_ioService(ioService)
		, m_playerManager(playerManager)
		, m_config(config)
		, m_timer(timer)
		, m_host(m_config.loginAddress)
		, m_port(m_config.loginPort)
		, m_realmID(0)
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
			case pp::realm_login::login_packet::LoginResult:
			{
				handleLoginResult(packet);
				break;
			}

			case pp::realm_login::login_packet::PlayerLoginSuccess:
			{
				handlePlayerLoginSuccess(packet);
				break;
			}

			case pp::realm_login::login_packet::PlayerLoginFailure:
			{
				handlePlayerLoginFailure(packet);
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
			pp::realm_login::realm_write::login(packet,
				m_config.internalName,
				m_config.password,
				m_config.visibleName,
				m_config.playerHost,
				m_config.playerPort);

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

	bool LoginConnector::playerLoginRequest(const String &accountName)
	{
		using namespace pp::realm_login;

		if (!m_connection)
		{
			return false;
		}

		// Check if there is already a pending login request
		for (auto &request : m_loginRequests)
		{
			if (request.accountName == accountName)
			{
				WLOG("There is already a pending login request for that account!");
				return false;
			}
		}

		DLOG("Sending player login request to login server...");
		m_loginRequests.push_back(PlayerLoginRequest(accountName));

		m_connection->sendSinglePacket(
			std::bind(realm_write::playerLogin, std::placeholders::_1, std::cref(accountName)));

		return true;
	}

	void LoginConnector::notifyPlayerLogin(UInt32 accountId)
	{
		//TODO
	}

	void LoginConnector::notifyPlayerLogout(UInt32 accountId)
	{
		//TODO
	}

	void LoginConnector::scheduleConnect()
	{
		const GameTime now = getCurrentTime();
		m_timer.addEvent(
			std::bind(&LoginConnector::tryConnect, this), now + ReconnectDelay);
	}

	void LoginConnector::tryConnect()
	{
		ILOG("Trying to connect to the login server..");

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
		using namespace pp::realm_login;

		// Read packet contents
		LoginResult result;
		UInt32 loginProtocolVersion = 0xffffffff;
		if (!login_read::loginResult(packet, result, loginProtocolVersion, m_realmID))
		{
			return;
		}

		// Compare protocol version
		if (loginProtocolVersion != pp::realm_login::ProtocolVersion)
		{
			WLOG("Incompatible login server protocol detected! Please update...");
			return;
		}

		// Display result
		switch (result)
		{
			case login_result::Success:
			{
				ILOG("Successfully logged in at login server - should now appear in realm list");
				break;
			}

			case login_result::UnknownRealm:
			{
				ELOG("Login server does not know this realm - invalid name");
				break;
			}

			case login_result::WrongPassword:
			{
				ELOG("Login server didn't accept password - invalid password")
				break;
			}

			case login_result::AlreadyLoggedIn:
			{
				ELOG("A realm using this internal name is already online!");
				break;
			}

			case login_result::ServerError:
			{
				ELOG("Internal server error at the login server");
				break;
			}
				
			default:
			{
				ELOG("Unknown login result");
				break;
			}
		}
	}

	void LoginConnector::handlePlayerLoginSuccess(pp::Protocol::IncomingPacket &packet)
	{
		// Read packet contents
		String accountName;
		BigNumber key, v, s;
		UInt32 accountId;
		std::array<UInt32, 8> tutorialData;
		if (!pp::realm_login::login_read::playerLoginSuccess(packet, accountName, accountId, key, v, s, tutorialData))
		{
			ELOG("Could not read packet!");
			return;
		}

		// Get player
		auto player = m_playerManager.getPlayerByAccountName(accountName);
		if (player)
		{
			// Proceed with login procedure
			player->loginSucceeded(accountId, key, v, s, tutorialData);
		}

		// Remove pending session
		const auto p = std::find_if(
			m_loginRequests.begin(),
			m_loginRequests.end(),
			[&accountName](const PlayerLoginRequest &p)
		{
			return (accountName == p.accountName);
		});

		if (p != m_loginRequests.end())
		{
			m_loginRequests.erase(p);
		}
	}

	void LoginConnector::handlePlayerLoginFailure(pp::Protocol::IncomingPacket &packet)
	{
		// Read packet contents
		String accountName;
		if (!pp::realm_login::login_read::playerLoginFailure(packet, accountName))
		{
			//TODO
			return;
		}

		// Get player
		auto player = m_playerManager.getPlayerByAccountName(accountName);
		if (player)
		{
			// Proceed with login procedure
			player->loginFailed();
		}

		// Remove pending session
		const auto p = std::find_if(
			m_loginRequests.begin(),
			m_loginRequests.end(),
			[&accountName](const PlayerLoginRequest &p)
		{
			return (accountName == p.accountName);
		});

		if (p != m_loginRequests.end())
		{
			m_loginRequests.erase(p);
		}
	}

	void LoginConnector::sendTutorialData(UInt32 accountId, const std::array<UInt32, 8> &data)
	{
		using namespace pp::realm_login;

		if (!m_connection)
		{
			return;
		}

		m_connection->sendSinglePacket(
			std::bind(realm_write::tutorialData, std::placeholders::_1, accountId, std::cref(data)));
	}
}
