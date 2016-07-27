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
#include "wowpp_protocol.h"
#include "binary_io/reader.h"

namespace wowpp
{
	namespace pp
	{
		namespace team_login
		{
			static const UInt32 ProtocolVersion = 0x01;

			namespace team_packet
			{
				enum Type
				{
					/// Sent by the team server to log in at the login server.
					Login,
					/// Ping message to keep the connection between the team server and the login server alive.
					KeepAlive
				};
			}

			typedef team_packet::Type TeamPacket;

			namespace login_packet
			{
				enum Type
				{
					/// Result of the team login answer.
					LoginResult
				};
			}

			typedef login_packet::Type LoginPacket;

			namespace login_result
			{
				enum Type
				{
					/// Team server could successfully log in.
					Success,
					/// The login server does not know this team server.
					UnknownTeamServer,
					/// The login server does not accept the password of this team server.
					WrongPassword,
					/// A team server with the same name is already online at the login server.
					AlreadyLoggedIn,
					/// Something went wrong at the login server...
					ServerError
				};
			}

			typedef login_result::Type LoginResult;


			/// Contains methods for writing packets from the team server.
			namespace team_write
			{
				/// Packet used to log in at the login server.
				/// @param out_packet Packet buffer where the data will be written to.
				/// @param internalName Internal name of the team server which the login server should know.
				/// @param password Password to verify that this team server is valid at the login server.
				void login(
				    pp::OutgoingPacket &out_packet,
				    const String &internalName,
				    const String &password
				);

				/// A simple empty packet which is used to keep the connection between the login server
				/// and the team server alive.
				/// @param out_packet Packet buffer where the data will be written to.
				void keepAlive(
				    pp::OutgoingPacket &out_packet
				);
			}

			/// Contains methods for writing packets from the login server.
			namespace login_write
			{
				/// TODO: ADD DESCRIPTION
				/// @param out_packet Packet buffer where the data will be written to.
				/// @param result The result of the login attempt of the realm.
				void loginResult(
				    pp::OutgoingPacket &out_packet,
				    LoginResult result
				);
			}

			/// Contains methods for reading packets coming from the team server.
			namespace team_read
			{
				/// TODO: ADD DESCRIPTION
				/// @param packet Packet buffer where the data will be read from.
				/// @param out_internalName
				/// @param maxInternalNameLength
				/// @param out_password
				/// @param maxPasswordLength
				/// @param out_visibleName
				/// @param maxVisibleNameLength
				/// @param out_host
				/// @param maxHostLength
				/// @param out_port
				/// @returns false if the packet has not enough data or if there was an error
				/// reading the packet's content.
				bool login(
				    io::Reader &packet,
				    String &out_internalName,
				    size_t maxInternalNameLength,
				    String &out_password,
				    size_t maxPasswordLength
				);

				/// TODO: ADD DESCRIPTION
				/// @param packet Packet buffer where the data will be read from.
				/// @returns false if the packet has not enough data or if there was an error
				/// reading the packet's content.
				bool keepAlive(
				    io::Reader &packet
				);
			}

			/// Contains methods for reading packets coming from the login server.
			namespace login_read
			{
				/// TODO: ADD DESCRIPTION
				/// @param packet Packet buffer where the data will be read from.
				/// @param out_result Result of the login attempt.
				/// @param out_serverVersion Protocol version used by the login server.
				/// @returns false if the packet has not enough data or if there was an error
				/// reading the packet's content.
				bool loginResult(
				    io::Reader &packet,
				    LoginResult &out_result,
				    UInt32 &out_serverVersion
				);
			}
		}
	}
}
