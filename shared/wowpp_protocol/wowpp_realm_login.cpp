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

#include <iostream>
#include "wowpp_realm_login.h"

namespace wowpp
{
	namespace pp
	{
		namespace realm_login
		{
			namespace realm_write
			{
				void login(pp::OutgoingPacket &out_packet, const String &internalName, const String &password, const String &visibleName, const String &host, NetPort port)
				{
					out_packet.start(realm_packet::Login);
					out_packet
						<< io::write<NetUInt32>(ProtocolVersion)
						<< io::write_dynamic_range<NetUInt8>(internalName)
						<< io::write_dynamic_range<NetUInt8>(password)
						<< io::write_dynamic_range<NetUInt8>(visibleName)
						<< io::write_dynamic_range<NetUInt8>(host)
						<< io::write<NetPort>(port);
					out_packet.finish();
				}

				void updateCurrentPlayers(pp::OutgoingPacket &out_packet, size_t playerCount)
				{
					out_packet.start(realm_packet::UpdateCurrentPlayers);
					out_packet
						<< io::write<NetUInt32>(playerCount);
					out_packet.finish();
				}

				void playerLogin(pp::OutgoingPacket &out_packet, const String &accountName)
				{
					out_packet.start(realm_packet::PlayerLogin);
					out_packet
						<< io::write_dynamic_range<NetUInt8>(accountName);
					out_packet.finish();
				}

				void playerLogout(pp::OutgoingPacket &out_packet /*TODO */)
				{
					out_packet.start(realm_packet::PlayerLogout);
					// This packet is empty right now (TODO)
					out_packet.finish();
				}

				void keepAlive(pp::OutgoingPacket &out_packet)
				{
					out_packet.start(realm_packet::KeepAlive);
					// This packet is empty
					out_packet.finish();
				}
			}

			namespace login_write
			{
				void loginResult(pp::OutgoingPacket &out_packet, LoginResult result)
				{
					out_packet.start(login_packet::LoginResult);
					out_packet
						<< io::write<NetUInt32>(ProtocolVersion)
						<< io::write<NetUInt8>(result);
					out_packet.finish();
				}

				void playerLoginSuccess(pp::OutgoingPacket &out_packet, const String &accountName, UInt32 accountId, const BigNumber &sessionKey, const BigNumber &v, const BigNumber &s)
				{
					out_packet.start(login_packet::PlayerLoginSuccess);
					out_packet
						<< io::write_dynamic_range<NetUInt8>(accountName)
						<< io::write<NetUInt32>(accountId);

					// Write numbers
					auto keyBuf = sessionKey.asByteArray();
					auto vBuf = v.asByteArray();
					auto sBuf = s.asByteArray();
					out_packet
						<< io::write_dynamic_range<NetUInt16>(keyBuf)
						<< io::write_dynamic_range<NetUInt16>(vBuf)
						<< io::write_dynamic_range<NetUInt16>(sBuf);

					out_packet.finish();
				}

				void playerLoginFailure(pp::OutgoingPacket &out_packet, const String &accountName)
				{
					out_packet.start(login_packet::PlayerLoginFailure);
					out_packet
						<< io::write_dynamic_range<NetUInt8>(accountName);
					out_packet.finish();
				}
			}

			namespace realm_read
			{
				bool login(io::Reader &packet, String &out_internalName, size_t maxInternalNameLength, String &out_password, size_t maxPasswordLength, String &out_visibleName, size_t maxVisibleNameLength, String &out_host, size_t maxHostLength, NetPort &out_port)
				{
					UInt32 realmProtocolVersion = 0xffffffff;
					packet
						>> io::read<NetUInt32>(realmProtocolVersion)
						>> io::read_container<NetUInt8>(out_internalName, static_cast<NetUInt8>(maxInternalNameLength))
						>> io::read_container<NetUInt8>(out_password, static_cast<NetUInt8>(maxPasswordLength))
						>> io::read_container<NetUInt8>(out_visibleName, static_cast<NetUInt8>(maxVisibleNameLength))
						>> io::read_container<NetUInt8>(out_host, static_cast<NetUInt8>(maxHostLength))
						>> io::read<NetPort>(out_port);
					
					// Handle realm protocol version based packet changes if needed

					return packet;
				}

				bool updateCurrentPlayers(io::Reader &packet, size_t &out_playerCount)
				{
					return packet
						>> io::read<NetUInt32>(out_playerCount);
				}

				bool playerLogin(io::Reader &packet, String &out_accountName)
				{
					return packet
						>> io::read_container<NetUInt8>(out_accountName);
				}

				bool playerLogout(io::Reader &packet /*TODO */)
				{
					//TODO
					return packet;
				}

				bool keepAlive(io::Reader &packet)
				{
					//TODO
					return packet;
				}
			}

			namespace login_read
			{
				bool loginResult(io::Reader &packet, LoginResult &out_result, UInt32 &out_serverVersion)
				{
					return packet
						>> io::read<NetUInt32>(out_serverVersion)
						>> io::read<NetUInt8>(out_result);
				}

				bool playerLoginSuccess(io::Reader &packet, String &out_accountName, UInt32 &out_accountId, BigNumber &out_sessionKey, BigNumber &out_v, BigNumber &out_s)
				{
					if (!(packet
						>> io::read_container<NetUInt8>(out_accountName)
						>> io::read<NetUInt32>(out_accountId)))
					{
						return false;
					}

					std::vector<UInt8> sessionKey, v, s;
					if (!(packet
						>> io::read_container<NetUInt16>(sessionKey)
						>> io::read_container<NetUInt16>(v)
						>> io::read_container<NetUInt16>(s)))
					{
						return false;
					}

					out_sessionKey.setBinary(sessionKey);
					out_v.setBinary(v);
					out_s.setBinary(s);

					return packet;
				}

				bool playerLoginFailure(io::Reader &packet, String &out_accountName)
				{
					return packet
						>> io::read_container<NetUInt8>(out_accountName);
				}
			}
		}
	}
}
