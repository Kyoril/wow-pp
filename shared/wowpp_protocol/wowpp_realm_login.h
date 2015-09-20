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
#include "common/big_number.h"
#include "wowpp_protocol.h"
#include "binary_io/reader.h"

namespace wowpp
{
	namespace pp
	{
		namespace realm_login
		{
			static const UInt32 ProtocolVersion = 0x03;

			namespace realm_packet
			{
				enum Type
				{
					/// Sent by the realm to log in at the login server.
					Login,
					/// Sent by the realm to update the number of current players.
					UpdateCurrentPlayers,
					/// Sent by the realm to notify the login server about a player who wants to log in.
					PlayerLogin,
					/// Sent by the realm to notify the login server about a player who logged out.
					PlayerLogout,
					/// Ping message to keep the connection between the realm and the login server alive.
					KeepAlive,
					/// Sent by the realm to notify the login server about tutorial data.
					TutorialData
				};
			}

			typedef realm_packet::Type RealmPacket;

			namespace login_packet
			{
				enum Type
				{
					/// Result of the realm login answer.
					LoginResult,
					/// Sent by the login server if the login attempt of a player was successful.
					PlayerLoginSuccess,
					/// Sent by the login server if the login attempt of a player failed (invalid session id).
					PlayerLoginFailure
				};
			}

			typedef login_packet::Type LoginPacket;

			namespace login_result
			{
				enum Type
				{
					/// Realm could successfully log in.
					Success,
					/// The login server does not know this realm.
					UnknownRealm,
					/// The login server does not accept the password of this realm.
					WrongPassword,
					/// A realm with the same name is already online at the login server.
					AlreadyLoggedIn,
					/// Something went wrong at the login server...
					ServerError
				};
			}

			typedef login_result::Type LoginResult;


			/// Contains methods for writing packets from the realm server.
			namespace realm_write
			{
				/// Packet used to log in at the login server.
				/// @param out_packet Packet buffer where the data will be written to.
				/// @param internalName Internal name of the realm which the login server should know.
				/// @param password Password to verify that this realm is valid at the login server.
				/// @param visibleName The name which will be visible for players in the realm list.
				/// @param host The address of this realm where game clients will try to connect to.
				/// @param port The port at which this realm is accepting incoming game client connections.
				void login(
					pp::OutgoingPacket &out_packet,
					const String &internalName,
					const String &password,
					const String &visibleName,
					const String &host,
					NetPort port
					);

				/// Tells the login server how many players are logged in on the realm.
				/// @param out_packet Packet buffer where the data will be written to.
				/// @param playerCount The number of players which are logged in on this realm right now.
				void updateCurrentPlayers(
					pp::OutgoingPacket &out_packet,
					size_t playerCount
					);

				/// Notifies the login server that a player wants to log in on this realm. After this
				/// packet is sent, the login server will verify that the player successfully logged
				/// in at the login server before.
				/// @param out_packet Packet buffer where the data will be written to.
				/// @param accountName Account name of the player in uppercase letters.
				void playerLogin(
					pp::OutgoingPacket &out_packet,
					const String &accountName
					);

				/// Notifies the login server that a player is no longer logged in on this realm. This
				/// is useful for the login server to know if an account is online on a specific realm.
				/// @param out_packet Packet buffer where the data will be written to.
				void playerLogout(
					pp::OutgoingPacket &out_packet
					//TODO
					);

				/// A simple empty packet which is used to keep the connection between the login server
				/// and the realm alive.
				/// @param out_packet Packet buffer where the data will be written to.
				void keepAlive(
					pp::OutgoingPacket &out_packet
					);

				/// 
				void tutorialData(
					pp::OutgoingPacket &out_packet,
					UInt32 accountId,
					const std::array<UInt32, 8> &data
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
					LoginResult result,
					UInt32 realmID
					);

				/// TODO: ADD DESCRIPTION
				/// @param out_packet Packet buffer where the data will be written to.
				/// @param accountName Account name which successfully logged in in uppercase letters.
				/// @param accountId Account id so that the realm and the login server can talk by using
				/// this numeric value instead of a string.
				/// @param sessionKey The session key used for srp-6 calculations.
				/// @param v V field of the player.
				/// @param s S field of the player.
				void playerLoginSuccess(
					pp::OutgoingPacket &out_packet,
					const String &accountName,
					UInt32 accountId,
					const BigNumber &sessionKey,
					const BigNumber &v,
					const BigNumber &s,
					const std::array<UInt32, 8> &tutorialData
					);

				/// TODO: ADD DESCRIPTION
				/// @param out_packet Packet buffer where the data will be written to.
				/// @param accountName Account name whos login attempt failed.
				void playerLoginFailure(
					pp::OutgoingPacket &out_packet,
					const String &accountName
					);
			}

			/// Contains methods for reading packets coming from the realm server. 
			namespace realm_read
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
					size_t maxPasswordLength,
					String &out_visibleName,
					size_t maxVisibleNameLength,
					String &out_host,
					size_t maxHostLength,
					NetPort &out_port
					);

				/// TODO: ADD DESCRIPTION
				/// @param packet Packet buffer where the data will be read from.
				/// @param out_playercount Number of players which are logged in at the realm.
				/// @returns false if the packet has not enough data or if there was an error
				/// reading the packet's content.
				bool updateCurrentPlayers(
					io::Reader &packet,
					size_t &out_playerCount
					);

				/// TODO: ADD DESCRIPTION
				/// @param packet Packet buffer where the data will be read from.
				/// @param out_accountName Account name of the player in uppercase letters.
				/// @returns false if the packet has not enough data or if there was an error
				/// reading the packet's content.
				bool playerLogin(
					io::Reader &packet,
					String &out_accountName
					);

				/// TODO: ADD DESCRIPTION
				/// @param packet Packet buffer where the data will be read from.
				/// @returns false if the packet has not enough data or if there was an error
				/// reading the packet's content.
				bool playerLogout(
					io::Reader &packet
					//TODO
					);

				/// TODO: ADD DESCRIPTION
				/// @param packet Packet buffer where the data will be read from.
				/// @returns false if the packet has not enough data or if there was an error
				/// reading the packet's content.
				bool keepAlive(
					io::Reader &packet
					);

				/// 
				bool tutorialData(
					io::Reader &packet,
					UInt32 &out_accountId,
					std::array<UInt32, 8> &out_data
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
					UInt32 &out_serverVersion,
					UInt32 &out_realmID
					);

				/// TODO: ADD DESCRIPTION
				/// @param packet Packet buffer where the data will be read from.
				/// @param out_accountName Account name which successfully logged in in uppercase letters.
				/// @param out_accountId Account id so that the realm and the login server can talk by using
				/// this numeric value instead of a string.
				/// @param out_sessionKey The session key used for srp-6 calculations.
				/// @param out_v V field of the player.
				/// @param out_s S field of the player.
				/// @returns false if the packet has not enough data or if there was an error
				/// reading the packet's content.
				bool playerLoginSuccess(
					io::Reader &packet,
					String &out_accountName,
					UInt32 &out_accountId,
					BigNumber &out_sessionKey,
					BigNumber &out_v,
					BigNumber &out_s,
					std::array<UInt32, 8> &out_tutorialData
					);

				/// TODO: ADD DESCRIPTION
				/// @param packet Packet buffer where the data will be read from.
				/// @param out_accountName Account name whos login attempt failed.
				/// @returns false if the packet has not enough data or if there was an error
				/// reading the packet's content.
				bool playerLoginFailure(
					io::Reader &packet,
					String &out_accountName
					);
			}
		}
	}
}
