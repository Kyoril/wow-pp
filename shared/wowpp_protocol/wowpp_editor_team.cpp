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
#include "wowpp_editor_team.h"

namespace wowpp
{
	namespace pp
	{
		namespace editor_team
		{
			namespace editor_write
			{
				void login(pp::OutgoingPacket &out_packet, const String &internalName, const SHA1Hash &password)
				{
					out_packet.start(editor_packet::Login);
					out_packet
					        << io::write<NetUInt32>(ProtocolVersion)
					        << io::write_dynamic_range<NetUInt8>(internalName)
					        << io::write_range<SHA1Hash>(password);
					out_packet.finish();
				}

				void keepAlive(pp::OutgoingPacket &out_packet)
				{
					out_packet.start(editor_packet::KeepAlive);
					// This packet is empty
					out_packet.finish();
				}

				void projectHashMap(pp::OutgoingPacket &out_packet, const std::map<String, String> &hashMap)
				{
					out_packet.start(editor_packet::ProjectHashMap);
					out_packet << io::write<NetUInt32>(hashMap.size());
					for (const auto &pair : hashMap)
					{
						out_packet
							<< io::write_dynamic_range<NetUInt8>(pair.first)
							<< io::write_dynamic_range<NetUInt8>(pair.second);
					}
					out_packet.finish();
				}
			}

			namespace team_write
			{
				void loginResult(pp::OutgoingPacket &out_packet, LoginResult result)
				{
					out_packet.start(team_packet::LoginResult);
					out_packet
					        << io::write<NetUInt32>(ProtocolVersion)
					        << io::write<NetUInt8>(result);
					out_packet.finish();
				}
			}

			namespace editor_read
			{
				bool login(io::Reader &packet, String &out_username, size_t maxUsernameLength, SHA1Hash &out_password)
				{
					UInt32 teamProtocolVersion = 0xffffffff;
					packet
					        >> io::read<NetUInt32>(teamProtocolVersion)
					        >> io::read_container<NetUInt8>(out_username, static_cast<NetUInt8>(maxUsernameLength))
					        >> io::read_range(out_password);

					// Handle team protocol version based packet changes if needed

					return packet;
				}

				bool keepAlive(io::Reader &packet)
				{
					//TODO
					return packet;
				}

				bool projectHashMap(io::Reader &packet, std::map<String, String> &out_hashMap)
				{
					UInt32 entries = 0;
					packet
						>> io::read<NetUInt32>(entries);
					
					out_hashMap.clear();
					for (UInt32 i = 0; i < entries; ++i)
					{
						String key, value;
						packet
							>> io::read_container<NetUInt8>(key)
							>> io::read_container<NetUInt8>(value);
						out_hashMap[key] = value;
					}

					return packet;
				}
			}

			namespace team_read
			{
				bool loginResult(io::Reader &packet, LoginResult &out_result, UInt32 &out_serverVersion)
				{
					if (!(packet
					        >> io::read<NetUInt32>(out_serverVersion)
					        >> io::read<NetUInt8>(out_result)))
					{
						return false;
					}

					return true;
				}
			}
		}
	}
}
