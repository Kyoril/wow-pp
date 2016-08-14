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
#include "log/default_log_levels.h"

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

				void compressedFile(pp::OutgoingPacket & out_packet, const String &filename, std::istream & fileStream)
				{
					// Determine total file size
					fileStream.seekg(0, std::ios::end);
					size_t originalSize = fileStream.tellg();
					fileStream.seekg(0);

					// Compress using ZLib
					boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
					in.push(boost::iostreams::zlib_compressor(boost::iostreams::zlib::best_compression));
					in.push(fileStream);

					// Copy to output stream
					std::stringstream outStrm;
					boost::iostreams::copy(in, outStrm);
					outStrm.seekg(0, std::ios::end);
					size_t bufferSize = outStrm.tellg();
					outStrm.seekg(0, std::ios::beg);

					String buffer = outStrm.str();

					// Write packet
					out_packet.start(team_packet::CompressedFile);
					out_packet
						<< io::write_dynamic_range<NetUInt8>(filename)
						<< io::write<NetUInt32>(originalSize)
						<< io::write_range(buffer.begin(), buffer.begin() + bufferSize);
					out_packet.finish();
				}

				void editorUpToDate(pp::OutgoingPacket & out_packet)
				{
					// Write packet
					out_packet.start(team_packet::EditorUpToDate);
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

				bool compressedFile(io::Reader & packet, String &out_filename, std::ostream & out_stream)
				{
					// Read original packet size
					UInt32 origSize = 0;
					if (!(packet
						>> io::read_container<NetUInt8>(out_filename)
						>> io::read<NetUInt32>(origSize)))
					{
						return false;
					}

					try
					{
						// Determine remaining size
						const size_t remainingSize = packet.getSource()->size() - packet.getSource()->position();

						// Create buffer
						String buffer;
						buffer.resize(remainingSize);
						packet.getSource()->read(&buffer[0], remainingSize);
						std::istringstream inStrm(buffer);

						// Uncompress using ZLib
						boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
						in.push(boost::iostreams::zlib_decompressor());
						in.push(inStrm);

						boost::iostreams::copy(in, out_stream);
						return true;
					}
					catch (const std::exception &e)
					{
						ELOG("Protocol read error: " << e.what());
						return false;
					}
				}

				bool editorUpToDate(io::Reader & packet)
				{
					return true;
				}
			}
		}
	}
}
