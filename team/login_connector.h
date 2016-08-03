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

#include "common/constants.h"
#include "network/connector.h"
#include "wowpp_protocol/wowpp_connector.h"
#include "common/timer_queue.h"
#include "common/sha1.h"

namespace wowpp
{
	// Forwards
	struct Configuration;

	/// This class manages the connection to the login server.
	class LoginConnector final : public pp::IConnectorListener
	{
	public:

		/// Creates a new instance of the login connector class.
		/// @param ioService Reference to the IO service object where sockets and timers
		///        are registered.
		/// @param playerManager Reference of the player manager which manages all connected players.
		/// @param config Reference to the realm server configuration.
		/// @param timer Reference to a global timer queue which can be used to create custom
		///        timers.
		explicit LoginConnector(
			boost::asio::io_service &ioService,
			const Configuration &config,
			TimerQueue &timer);
		~LoginConnector();

		/// @copydoc wowpp::pp::IConnectorListener::connectionLost()
		void connectionLost() override;
		/// @copydoc wowpp::pp::IConnectorListener::connectionMalformedPacket()
		void connectionMalformedPacket() override;
		/// @copydoc wowpp::pp::IConnectorListener::connectionPacketReceived()
		void connectionPacketReceived(pp::Protocol::IncomingPacket &packet) override;
		/// @copydoc wowpp::pp::IConnectorListener::connectionEstablished()
		bool connectionEstablished(bool success) override;

	public:

		void editorLoginRequest(const String &username, const SHA1Hash &password);

	private:

		/// Schedules the next login attempt after a few seconds in case of disconnection
		/// of the login server.
		void scheduleConnect();
		/// Tries to connect with the login server and schedules a new attempt if failed.
		void tryConnect();
		/// Schedules send of keep-alive-packet to the login server which shall keep
		/// the connection alive (no timeout).
		void scheduleKeepAlive();
		/// Sends the keep-alive packet to the login server and schedules the next keep-alive event.
		void onScheduledKeepAlive();

		// Packet handlers
		void handleLoginResult(pp::Protocol::IncomingPacket &packet);
		void handleEditorLoginResult(pp::Protocol::IncomingPacket &packet);

	private:

		/// Holds data of a pending player login request.
		struct EditorLoginRequest
		{
			String accountName;
			//TODO: timeout

			/// Initializes an empty player login request. Support for 
			/// default constructor.
			EditorLoginRequest()
			{
			}

			/// Initializes a login request for a specified player account name.
			/// @param account Name of the account to login with in uppercase letters.
			EditorLoginRequest(const String &account)
				: accountName(account)
			{
			}
		};

		boost::asio::io_service &m_ioService;
		const Configuration &m_config;
		TimerQueue &m_timer;
		std::shared_ptr<pp::Connector> m_connection;
		String m_host;
		NetPort m_port;
		std::vector<EditorLoginRequest> m_loginRequests;
	};
}
