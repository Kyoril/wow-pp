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
#include "team_connector.h"
#include "wowpp_protocol/wowpp_realm_login.h"
#include "configuration.h"
#include "common/clock.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	namespace editor
	{
		static const auto ReconnectDelay = (constants::OneSecond * 4);
		static const auto KeepAliveDelay = (constants::OneMinute / 2);

		TeamConnector::TeamConnector(boost::asio::io_service &ioService, const Configuration &config, TimerQueue &timer)
			: m_ioService(ioService)
			, m_config(config)
			, m_timer(timer)
			, m_host(m_config.teamAddress)
			, m_port(m_config.teamPort)
		{
		}

		TeamConnector::~TeamConnector()
		{
		}

		void TeamConnector::connectionLost()
		{
			WLOG("Lost connection with the team server at " << m_host << ":" << m_port);
			disconnected();

			m_connection->resetListener();
			m_connection.reset();
			scheduleConnect();
		}

		void TeamConnector::connectionMalformedPacket()
		{
			WLOG("Received malformed packet from the team server");
			m_connection->resetListener();
			m_connection.reset();
		}

		void TeamConnector::connectionPacketReceived(pp::Protocol::IncomingPacket &packet)
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
					handleEditorLoginSuccess(packet);
					break;
				}

				case pp::realm_login::login_packet::PlayerLoginFailure:
				{
					handleEditorLoginFailure(packet);
					break;
				}

				default:
				{
					// Log about unknown or unhandled packet
					WLOG("Received unknown packet " << static_cast<UInt32>(packetId)
						<< " from team server at " << m_host << ":" << m_port);
					break;
				}
			}
		}

		bool TeamConnector::connectionEstablished(bool success)
		{
			connected(success);

			if (success)
			{
				ILOG("Connected to the team server");

				scheduleKeepAlive();
			}

			return true;
		}

		bool TeamConnector::editorLoginRequest(const String &accountName)
		{
			using namespace pp::realm_login;

			if (!m_connection)
			{
				return false;
			}

			m_connection->sendSinglePacket(
				std::bind(realm_write::playerLogin, std::placeholders::_1, std::cref(accountName)));

			return true;
		}

		void TeamConnector::notifyEditorLogin(UInt32 accountId)
		{
			//TODO
		}

		void TeamConnector::notifyEditorLogout(UInt32 accountId)
		{
			//TODO
		}

		void TeamConnector::scheduleConnect()
		{
			const GameTime now = getCurrentTime();
			m_timer.addEvent(
				std::bind(&TeamConnector::tryConnect, this), now + ReconnectDelay);
		}

		void TeamConnector::tryConnect()
		{
			m_connection = pp::Connector::create(m_ioService);
			m_connection->connect(m_host, m_port, *this, m_ioService);
		}

		void TeamConnector::scheduleKeepAlive()
		{

		}

		void TeamConnector::onScheduledKeepAlive()
		{

		}

		void TeamConnector::handleLoginResult(pp::Protocol::IncomingPacket &packet)
		{
			using namespace pp::realm_login;

		}

		void TeamConnector::handleEditorLoginSuccess(pp::Protocol::IncomingPacket &packet)
		{
		}

		void TeamConnector::handleEditorLoginFailure(pp::Protocol::IncomingPacket &packet)
		{
		}
	}
}
