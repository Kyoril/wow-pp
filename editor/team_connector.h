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
#include "wowpp_protocol/wowpp_editor_team.h"
#include "proto_data/project.h"
#include "common/timer_queue.h"
#include "common/sha1.h"

namespace wowpp
{
	namespace editor
	{
		// Forwards
		struct Configuration;
		
		/// This class manages the connection to the login server.
		class TeamConnector final : public pp::IConnectorListener
		{
		public:

			/// 
			boost::signals2::signal<void(bool)> connected;
			/// 
			boost::signals2::signal<void()> disconnected;
			/// 
			boost::signals2::signal<void(UInt32, UInt32)> loginResult;
			/// 
			boost::signals2::signal<void(const String&)> fileUpdate;
			/// 
			boost::signals2::signal<void()> editorUpToDate;

		public:

			/// Creates a new instance of the team connector class.
			/// @param ioService Reference to the IO service object where sockets and timers
			///        are registered.
			/// @param config Reference to the realm server configuration.
			/// @param timer Reference to a global timer queue which can be used to create custom
			///        timers.
			explicit TeamConnector(
				boost::asio::io_service &ioService,
				const Configuration &config,
				TimerQueue &timer);
			~TeamConnector();

			/// @copydoc wowpp::pp::IConnectorListener::connectionLost()
			void connectionLost() override;
			/// @copydoc wowpp::pp::IConnectorListener::connectionMalformedPacket()
			void connectionMalformedPacket() override;
			/// @copydoc wowpp::pp::IConnectorListener::connectionPacketReceived()
			void connectionPacketReceived(pp::Protocol::IncomingPacket &packet) override;
			/// @copydoc wowpp::pp::IConnectorListener::connectionEstablished()
			bool connectionEstablished(bool success) override;

			/// Notifies the login server about a login request of a player on this realm. The
			/// login server will check if this request is valid. This is done so that no one
			/// can skip the login server check.
			/// @param accountName Name of the account which the player wants to login with in
			///        uppercase letters.
			bool editorLoginRequest(const String &accountName, const SHA1Hash &password);
			/// Notifies the team server about the sha1 hashes of the local project files in order
			/// to check for changed files to download from the team server.
			/// @param hashes The hash map. Key value is the manager name, value is the hash string.
			void projectHashMap(const std::map<String, String> &hashes);
			/// Sends all changed entries to the server.
			void sendEntryChanges(const std::map<pp::editor_team::DataEntryType, std::map<UInt32, pp::editor_team::DataEntryChangeType>> &changes, const proto::Project &project);

			/// Tries to connect with the team server and schedules a new attempt if failed.
			void tryConnect();

		private:

			/// Schedules the next login attempt after a few seconds in case of disconnection
			/// of the login server.
			void scheduleConnect();
			/// Schedules send of keep-alive-packet to the login server which shall keep
			/// the connection alive (no timeout).
			void scheduleKeepAlive();
			/// Sends the keep-alive packet to the login server and schedules the next keep-alive event.
			void onScheduledKeepAlive();

			// Packet handlers
			void handleLoginResult(pp::Protocol::IncomingPacket &packet);
			void handleCompressedFile(pp::Protocol::IncomingPacket &packet);
			void handleEditorUpToDate(pp::Protocol::IncomingPacket &packet);

		private:

			boost::asio::io_service &m_ioService;
			const Configuration &m_config;
			TimerQueue &m_timer;
			std::shared_ptr<pp::Connector> m_connection;
			String m_host;
			NetPort m_port;
		};
	}
}
