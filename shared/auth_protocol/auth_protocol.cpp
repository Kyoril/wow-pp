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

#include <iostream>
#include <sstream>
#include <ostream>
#include "auth_protocol.h"

namespace wowpp
{
	namespace auth
	{
		bool client_read::logonChallenge(io::Reader &packet,
			UInt8 &out_version1,
			UInt8 &out_version2,
			UInt8 &out_version3,
			UInt16 &out_build,
			AuthPlatform &out_platform,
			AuthSystem &out_system,
			AuthLocale &out_locale,
			String &out_userName)
		{
			// Temporary storage
			UInt8 error = 0;
			UInt16 size = 0;
			UInt32 gameName = 0, platform = 0, os = 0, locale = 0, timezone = 0, ip = 0;

			// Read packet data
			packet
				>> io::read<NetUInt8>(error)
				>> io::read<NetUInt16>(size)
				>> io::read<NetUInt32>(gameName)
				>> io::read<NetUInt8>(out_version1)
				>> io::read<NetUInt8>(out_version2)
				>> io::read<NetUInt8>(out_version3)
				>> io::read<NetUInt16>(out_build)
				>> io::read<NetUInt32>(platform)
				>> io::read<NetUInt32>(os)
				>> io::read<NetUInt32>(locale)
				>> io::read<NetUInt32>(timezone)
				>> io::read<NetUInt32>(ip)
				>> io::read_container<NetUInt8>(out_userName);
			if (packet)
			{
				// Convert platform
				switch (platform)
				{
					case 0x00783836:	// " x86"
					{
						out_platform = auth_platform::x86;
						break;
					}

					default:
					{
						out_platform = auth_platform::Unknown;
						break;
					}
				}

				// Convert os
				switch (os)
				{
					case 0x0057696e:	// " Win"
					{
						out_system = auth_system::Windows;
						break;
					}

					case 0x004f5358:	// " OSX"
					{
						out_system = auth_system::MacOS;
						break;
					}

					default:
					{
						out_system = auth_system::Unknown;
						break;
					}
				}

				// Convert locale
				switch (locale)
				{
					case 0x66724652:	// "frFR"
					{
						out_locale = auth_locale::frFR;
						break;
					}

					case 0x64654445:	// "deDE"
					{
						out_locale = auth_locale::deDE;
						break;
					}

					case 0x656e4742:	// "enGB"
					{
						out_locale = auth_locale::enGB;
						break;
					}

					case 0x656e5553:	// "enUS"
					{
						out_locale = auth_locale::enUS;
						break;
					}

					case 0x69744954:	// "itIT"
					{
						out_locale = auth_locale::itIT;
						break;
					}

					case 0x6b6f4b52:	// "koKR"
					{
						out_locale = auth_locale::koKR;
						break;
					}

					case 0x7a68434e:	// "zhCN"
					{
						out_locale = auth_locale::zhCN;
						break;
					}

					case 0x7a685457:	// "zhTW"
					{
						out_locale = auth_locale::zhTW;
						break;
					}

					case 0x72755255:	// "ruRU"
					{
						out_locale = auth_locale::ruRU;
						break;
					}

					case 0x65734553:	// "esES"
					{
						out_locale = auth_locale::esES;
						break;
					}

					case 0x65734d58:	// "esMX"
					{
						out_locale = auth_locale::esMX;
						break;
					}

					case 0x70744252:	// "ptBR"
					{
						out_locale = auth_locale::ptBR;
						break;
					}

					default:
					{
						out_locale = auth_locale::Unknown;
						break;
					}
				}
			}

			return packet;
		}

		bool client_read::logonProof(io::Reader &packet, std::array<UInt8, 32> &out_A, std::array<UInt8, 20> &out_M1, std::array<UInt8, 20> &out_crc_hash, UInt8 &out_number_of_keys, UInt8 &out_securityFlags)
		{
			return packet
				>> io::read_range(out_A.begin(), out_A.end())
				>> io::read_range(out_M1.begin(), out_M1.end())
				>> io::read_range(out_crc_hash.begin(), out_crc_hash.end())
				>> io::read<NetUInt8>(out_number_of_keys)
				>> io::read<NetUInt8>(out_securityFlags);
		}

		bool client_read::realmList(io::Reader &packet)
		{
			// This packet is empty
			return packet;
		}


		void server_write::logonChallenge(auth::OutgoingPacket &out_packet, auth_result::Type result,
			const BigNumber &B, const BigNumber &g, const BigNumber &N, const BigNumber &s, const BigNumber &unk3)
		{
			out_packet.start(server_packet::LogonChallenge);
			out_packet
				<< io::write<NetUInt8>(0)			// Unknown
				<< io::write<NetUInt8>(result);

			// On success, there are more data values to write
			if (result == auth_result::Success)
			{
				// Write B with 32 byte length and g
				std::vector<UInt8> B_ = B.asByteArray(32);
				out_packet
					<< io::write_range(B_.begin(), B_.end())
					<< io::write<NetUInt8>(1)		// Unknown
					<< io::write<NetUInt8>(g.asUInt32())
					<< io::write<NetUInt8>(32);		// Unknown

				// Write N with 32 byte length
				std::vector<UInt8> N_ = N.asByteArray(32);
				out_packet
					<< io::write_range(N_.begin(), N_.end());

				// Write s
				std::vector<UInt8> s_ = s.asByteArray();
				out_packet
					<< io::write_range(s_.begin(), s_.end());

				// Write unk3 with 16 bytes length
				std::vector<UInt8> unk3_ = unk3.asByteArray(16);
				out_packet
					<< io::write_range(unk3_.begin(), unk3_.end());

				// Write security flags
				UInt8 securityFlags = 0x00;	// 0x00...0x04
				out_packet
					<< io::write<NetUInt8>(securityFlags);

				if (securityFlags & 0x01)
				{
					out_packet
						<< io::write<NetUInt32>(0)
						<< io::write<NetUInt64>(0)
						<< io::write<NetUInt64>(0);
				}

				if (securityFlags & 0x02)
				{
					out_packet
						<< io::write<NetUInt8>(0)
						<< io::write<NetUInt8>(0)
						<< io::write<NetUInt8>(0)
						<< io::write<NetUInt8>(0)
						<< io::write<NetUInt64>(0);
				}

				if (securityFlags & 0x04)
				{
					out_packet
						<< io::write<NetUInt8>(1);
				}
			}

			out_packet.finish();
		}

		void server_write::logonProof(auth::OutgoingPacket &out_packet, auth_result::Type result, const std::array<UInt8, 20> &hash)
		{
			out_packet.start(server_packet::LogonProof);
			out_packet
				<< io::write<NetUInt8>(result);

			if (result == auth_result::FailIncorrectPassword || result == auth_result::FailUnknownAccount)
			{
				out_packet
					<< io::write<NetUInt8>(3)
					<< io::write<NetUInt8>(0);
			}

			// Append proof hash on success
			if (result == auth_result::Success)
			{
				out_packet
					<< io::write_range(hash.begin(), hash.end())
#if SUPPORTED_CLIENT_BUILD >= 8606
					<< io::write<NetUInt32>(0x00800000)
#endif
					<< io::write<NetUInt32>(0x00)
#if SUPPORTED_CLIENT_BUILD >= 6546
					<< io::write<NetUInt16>(0x00)
#endif
					;
			}

			out_packet.finish();
		}

		void server_write::realmList(auth::OutgoingPacket &out_packet, const std::vector<RealmEntry> &realmList)
		{
			out_packet.start(server_packet::RealmList);
			
			// Packet size for later use
			size_t sizePosition = out_packet.sink().position();
			out_packet
				<< io::write<NetUInt16>(0);

			size_t bodyPosition = out_packet.sink().position();

			// Realm count
			out_packet
				<< io::write<NetUInt32>(0)					// unused value
				<< io::write<NetUInt16>(realmList.size())	// Build 8606+
				;

			// Iterate through all realms
			for (auto &realm : realmList)
			{
				//TODO: Get value from database
				UInt8 AmountOfCharacters = 0;

				// 1.x clients not support explicitly REALM_FLAG_SPECIFYBUILD, so manually form similar name as show in more recent clients
				std::ostringstream nameStrm;
				nameStrm << realm.name;

				// To be able to support a custom port, we need to provide the address like this: XXX.XXX.XXX.XXX:PORT
				std::ostringstream addrStrm;
				addrStrm << realm.address << ':' << realm.port;

				const String displayName = nameStrm.str();
				const String address = addrStrm.str();
				out_packet
					<< io::write<NetUInt8>(realm.icon)								// Build 8606+
					<< io::write<NetUInt8>(0x00)									// Build 8606+: locked if 0x01
					<< io::write<NetUInt8>(realm.flags)
					<< io::write_range(displayName.begin(), displayName.end())
					<< io::write<NetUInt8>(0)									// C-Style string terminator
					<< io::write_range(address.begin(), address.end())
					<< io::write<NetUInt8>(0)									// C-Style string terminator
					<< io::write<float>(0.0f)
					<< io::write<NetUInt8>(AmountOfCharacters)
					<< io::write<NetUInt8>(1)									// Timezone (Cfg_Categories.dbc)
#if SUPPORTED_CLIENT_BUILD >= 6546
					<< io::write<NetUInt8>(0x2C)
#endif
					;

				// Build 8606+
				if (realm.flags & realm_flags::SpecifyBuild)
				{
					out_packet
#if SUPPORTED_CLIENT_BUILD == 6546
						<< io::write<NetUInt8>(2)
						<< io::write<NetUInt8>(0)
						<< io::write<NetUInt8>(12)
						<< io::write<NetUInt16>(6546)
#else
						<< io::write<NetUInt8>(2)
						<< io::write<NetUInt8>(4)
						<< io::write<NetUInt8>(3)
						<< io::write<NetUInt16>(8606)
#endif
						;
				}
			}

			// End of realm list
			out_packet
				<< io::write<NetUInt16>(0x0010)		// Build 8606+
				;

			// Rewrite packet size
			size_t end = out_packet.sink().position();
			size_t size = end - bodyPosition;
			out_packet.writePOD<NetUInt16>(sizePosition, static_cast<NetUInt16>(size));
			out_packet.finish();
		}
	}
}
