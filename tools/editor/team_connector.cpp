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
#include "configuration.h"
#include "common/clock.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	namespace editor
	{
		static const auto ReconnectDelay = (constants::OneSecond * 4);
		static const auto KeepAliveDelay = (constants::OneMinute / 2);

		TeamConnector::TeamConnector(boost::asio::io_service &ioService, const Configuration &config, proto::Project &project, TimerQueue &timer)
			: m_ioService(ioService)
			, m_config(config)
			, m_project(project)
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
			disconnected();
		}

		void TeamConnector::connectionPacketReceived(pp::Protocol::IncomingPacket &packet)
		{
			const auto &packetId = packet.getId();

			switch (packetId)
			{
				case pp::editor_team::team_packet::LoginResult:
				{
					handleLoginResult(packet);
					break;
				}

				case pp::editor_team::team_packet::CompressedFile:
				{
					handleCompressedFile(packet);
					break;
				}

				case pp::editor_team::team_packet::EditorUpToDate:
				{
					handleEditorUpToDate(packet);
					break;
				}

				case pp::editor_team::team_packet::EntryUpdate:
				{
					handleEntryUpdate(packet);
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

		void TeamConnector::connectionDataSent(size_t size)
		{
			dataSent(size);
		}

		bool TeamConnector::editorLoginRequest(const String &accountName, const SHA1Hash &password)
		{
			using namespace pp::editor_team;

			if (!m_connection)
			{
				return false;
			}

			m_connection->sendSinglePacket(
				std::bind(editor_write::login, std::placeholders::_1, std::cref(accountName), std::cref(password)));

			return true;
		}

		void TeamConnector::scheduleConnect()
		{
			const GameTime now = getCurrentTime();
			m_timer.addEvent(
				std::bind(&TeamConnector::tryConnect, this), now + ReconnectDelay);
		}

		void TeamConnector::projectHashMap(const std::map<String, String>& hashes)
		{
			using namespace pp::editor_team;

			m_connection->sendSinglePacket(
				std::bind(editor_write::projectHashMap, std::placeholders::_1, std::cref(hashes)));
		}

		void TeamConnector::sendEntryChanges(const std::map<pp::editor_team::DataEntryType, std::map<UInt32, pp::editor_team::DataEntryChangeType>>& changes)
		{
			using namespace pp::editor_team;

			// TODO

			m_connection->sendSinglePacket(
				std::bind(editor_write::entryUpdate, std::placeholders::_1, std::cref(changes), std::cref(m_project)));
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
			UInt32 protocolVersion = 0x00;
			pp::editor_team::LoginResult result;
			if (!pp::editor_team::team_read::loginResult(packet, result, protocolVersion))
			{
				return;
			}

			// Close connection on error
			if (result != pp::editor_team::login_result::Success)
			{
				m_connection->close();
				m_connection->resetListener();
				m_connection.reset();
			}

			loginResult(result, protocolVersion);
		}

		void TeamConnector::handleCompressedFile(pp::Protocol::IncomingPacket & packet)
		{
			// Determine project data path
			boost::filesystem::path p(m_config.dataPath);
			p /= "wowpp";

			// Temporary file path
			boost::filesystem::path p2 = p / "update.tmp";

			// Create temporary file
			std::ofstream outFile(p2.c_str(), std::ios::out | std::ios::binary);
			if (!outFile)
			{
				ELOG("Could not create temporary file!");
				return;
			}

			// Receive packet data
			String filename;
			if (!pp::editor_team::team_read::compressedFile(packet, filename, outFile))
			{
				// Close temporary file and try to delete it
				outFile.close();
				try
				{
					boost::filesystem::remove(p2);
				}
				catch (...)
				{
					// Ignore any exceptions as deleting this file is not important yet
				}
				return;
			}

			// Notify UI
			fileUpdate(filename);

			// Close this file anyway to release it
			outFile.close();

			// Not try to delete the remaining old file
			try
			{
				boost::filesystem::remove(p / (filename + ".wppdat"));
			}
			catch (const std::exception &e)
			{
				ELOG("Could not delete old project file: " << e.what());
				return;
			}

			// Rename temporary file to old file
			try
			{
				boost::filesystem::rename(p2, p / (filename + ".wppdat"));
			}
			catch (const std::exception &e)
			{
				ELOG("Could not rename temporary file: " << e.what());
				return;
			}
		}

		void TeamConnector::handleEditorUpToDate(pp::Protocol::IncomingPacket & packet)
		{
			// Fire signal to notify UI
			editorUpToDate();
		}

		void TeamConnector::handleEntryUpdate(pp::Protocol::IncomingPacket & packet)
		{
			// Receive changes and also patch local project
			std::map<pp::editor_team::DataEntryType, std::map<UInt32, pp::editor_team::DataEntryChangeType>> changes;
			if (!pp::editor_team::team_read::entryUpdate(packet, changes, m_project))
			{
				return;
			}

			// Save changes
			m_project.save(m_project.getLastPath());
		}

	}
}
