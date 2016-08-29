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
#include "common/sha1.h"

namespace wowpp
{
	namespace proto
	{
		class Project;
	}

	namespace pp
	{
		namespace editor_team
		{
			static const UInt32 ProtocolVersion = 0x03;

			namespace data_entry_type
			{
				enum Type
				{
					Spells,
					Units,
					Objects,
					Maps,
					Emotes,
					UnitLoot,
					ObjectLoot,
					ItemLoot,
					SkinningLoot,
					Skills,
					Trainers,
					Vendors,
					Talents,
					Items,
					ItemSets,
					Classes,
					Races,
					Levels,
					Triggers,
					Zones,
					Quests,
					Factions,
					FactionTemplates,
					AreaTriggers,
					SpellCategories,
				};
			}

			typedef data_entry_type::Type DataEntryType;

			namespace data_entry_change_type
			{
				enum Type
				{
					/// New entry was added.
					Added,
					/// Existing entry was modified.
					Modified,
					/// Existing entry was removed.
					Removed
				};
			}

			typedef data_entry_change_type::Type DataEntryChangeType;


			namespace editor_packet
			{
				enum Type
				{
					/// Sent by the editor to log in at the team server.
					Login,
					/// Ping message to keep the connection between the editor and the team server alive.
					KeepAlive,
					/// Sent to compare the local editor hashtable with the hashes on the team server to detect file changes.
					ProjectHashMap,
					/// Sent on save by the editor to notify the team server about local changes.
					EntryUpdate,
				};
			}

			typedef editor_packet::Type EditorPacket;

			namespace team_packet
			{
				enum Type
				{
					/// Result of the team login answer.
					LoginResult,
					/// Compressed file content.
					CompressedFile,
					/// Notifies the editor that it is up to date now.
					EditorUpToDate,
					/// Sent to connects editors by the team server to notify them about changes made by someone else right now.
					/// The editors have the opportunity to discard some or all of these changes locally (for example to prevent
					/// loss of work done locally which simply hasn't been sent yet).
					EntryUpdate,
				};
			}

			typedef team_packet::Type TeamPacket;

			namespace login_result
			{
				enum Type
				{
					/// Team server could successfully log in.
					Success,
					/// The login server does not know this account.
					WrongUserName,
					/// The login server does not accept the password of this account.
					WrongPassword,
					/// An account with the same name is already online at the login server.
					AlreadyLoggedIn,
					/// Connection timeout, maybe between the login server and the team server.
					TimedOut,
					/// Something went wrong at the login server...
					ServerError
				};
			}

			typedef login_result::Type LoginResult;


			/// Contains methods for writing packets from the team server.
			namespace editor_write
			{
				/// Packet used to log in at the login server.
				/// @param out_packet Packet buffer where the data will be written to.
				/// @param internalName Internal name of the team server which the login server should know.
				/// @param password Password to verify that this team server is valid at the login server.
				void login(
				    pp::OutgoingPacket &out_packet,
				    const String &username,
				    const SHA1Hash &password
				);

				/// A simple empty packet which is used to keep the connection between the login server
				/// and the team server alive.
				/// @param out_packet Packet buffer where the data will be written to.
				void keepAlive(
				    pp::OutgoingPacket &out_packet
				);

				/// 
				void projectHashMap(
					pp::OutgoingPacket &out_packet,
					const std::map<String, String> &hashMap
				);

				/// 
				void entryUpdate(
					pp::OutgoingPacket &out_packet,
					const std::map<DataEntryType, std::map<UInt32, DataEntryChangeType>> &changes,
					const proto::Project &project
				);
			}

			/// Contains methods for writing packets from the login server.
			namespace team_write
			{
				/// TODO: ADD DESCRIPTION
				/// @param out_packet Packet buffer where the data will be written to.
				/// @param result The result of the login attempt of the realm.
				void loginResult(
				    pp::OutgoingPacket &out_packet,
				    LoginResult result
				);

				void compressedFile(
					pp::OutgoingPacket &out_packet,
					const String &filename,
					std::istream &fileStream
				);

				void editorUpToDate(
					pp::OutgoingPacket &out_packet
				);

				void entryUpdate(
					pp::OutgoingPacket &out_packet,
					const std::map<DataEntryType, std::map<UInt32, DataEntryChangeType>> &changes,
					const proto::Project &project
				);
			}

			/// Contains methods for reading packets coming from the team server.
			namespace editor_read
			{
				/// TODO: ADD DESCRIPTION
				/// @param packet Packet buffer where the data will be read from.
				/// @param out_username
				/// @param maxUsernameLength
				/// @param out_password
				/// @returns false if the packet has not enough data or if there was an error
				/// reading the packet's content.
				bool login(
				    io::Reader &packet,
				    String &out_username,
				    size_t maxUsernameLength,
				    SHA1Hash &out_password
				);

				/// TODO: ADD DESCRIPTION
				/// @param packet Packet buffer where the data will be read from.
				/// @returns false if the packet has not enough data or if there was an error
				/// reading the packet's content.
				bool keepAlive(
				    io::Reader &packet
				);
				
				/// TODO: ADD DESCRIPTION
				bool projectHashMap(
					io::Reader &packet,
					std::map<String, String> &out_hashMap
				);

				/// 
				bool entryUpdate(
					io::Reader &packet,
					std::map<DataEntryType, std::map<UInt32, DataEntryChangeType>> &out_changes,
					proto::Project &out_project
				);
			}

			/// Contains methods for reading packets coming from the login server.
			namespace team_read
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

				/// 
				bool compressedFile(
					io::Reader &packet,
					String &out_filename,
					std::ostream &out_stream
				);

				/// 
				bool editorUpToDate(
					io::Reader &packet
				);

				/// 
				bool entryUpdate(
					io::Reader &packet,
					std::map<DataEntryType, std::map<UInt32, DataEntryChangeType>> &out_changes,
					proto::Project &out_project
				);
			}
		}
	}
}
