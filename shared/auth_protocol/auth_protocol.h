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
#include "auth_incoming_packet.h"
#include "auth_outgoing_packet.h"
#include "binary_io/reader.h"
#include "common/big_number.h"
#include <array>

namespace wowpp
{
	namespace auth
	{
		struct Protocol
		{
			typedef auth::IncomingPacket IncomingPacket;
			typedef auth::OutgoingPacket OutgoingPacket;
		};


		/// Enumerates all OP codes sent by the client.
		namespace client_packet
		{
			enum
			{
				/// Sent by the client right after the connection was established.
				LogonChallenge		= 0x00,
				/// Sent by the client after auth_result::Success was received from the server.
				LogonProof			= 0x01,
				/// <Not sent at the moment>
				ReconnectChallenge	= 0x02,
				/// <Not sent at the moment>
				ReconnectProof		= 0x03,
				/// Sent by the client after receive of successful LogonProof from the server, or if
				/// the player steps back to the realm list.
				RealmList			= 0x10,
			};
		}

		/// Enumerates possible OP codes which the server can send to the client.
		namespace server_packet
		{
			enum
			{
				LogonChallenge		= 0x00,
				LogonProof			= 0x01,
				ReconnectChallenge	= 0x02,
				ReconnectProof		= 0x03,
				RealmList			= 0x10,
				XferInitiated		= 0x30,
				XferData			= 0x31,
			};
		}

		/// Enumerates possible authentification result codes.
		namespace auth_result
		{
			enum Type
			{
				/// Success.
				Success					= 0x00,
				/// This WoW account has been closed and is no longer available for use. Please go to <site>/banned.html for further information.
				FailBanned				= 0x03,
				/// The information you have entered is not valid. Please check the spelling of the account name and password. If you need help in retrieving a lost or stolen password, see <site> for more information.
				FailUnknownAccount		= 0x04,
				/// The information you have entered is not valid. Please check the spelling of the account name and password. If you need help in retrieving a lost or stolen password, see <site> for more information.
				FailIncorrectPassword	= 0x05,
				/// This account is already logged into WoW. Please check the spelling and try again.
				FailAlreadyOnline		= 0x06,
				/// You have used up your prepaid time for this account. Please purchase more to continue playing.
				FailNoTime				= 0x07,
				/// Could not log in to WoW at this time. Please try again later.
				FailDbBusy				= 0x08,
				/// Unable to validate game version. This may be caused by file corruption or interference of another program. Please visit <site> for more information and possible solutions to this issue.
				FailVersionInvalid		= 0x09,
				/// Downloading...
				FailVersionUpdate		= 0x0A,
				/// Unable to connect.
				FailInvalidServer		= 0x0B,
				/// This WoW account has been temporarily suspended. Please go to <site>/banned.html for further information.
				FailSuspended			= 0x0C,
				/// Unable to connect.
				FailNoAccess			= 0x0D,
				/// Connected.
				SuccessSurvey			= 0x0E,
				/// Access to this account has been blocked by parental controls. Your settings may be changed in your account preferences at <site>.
				FailParentControl		= 0x0F,
				/// You have applied a lock to your account. You can change your locked status by calling your account lock phone number.
				FailLockedEnforced		= 0x10,
				/// Your trial subscription has expired. Please visit <site> to upgrade your account.
				FailTrialEnded			= 0x11,
				/// This account is now attached to a Battle.net account. Please login with your Battle.net account email address and password.
				FailUseBattleNet		= 0x12
			};
		}

		typedef auth_result::Type AuthResult;

		/// Enumerates possible client platform architectures.
		namespace auth_platform
		{
			enum Type
			{
				/// Unknown platform architecture.
				Unknown = 0x00,
				/// 32 bit platform.
				x86		= 0x01,
				/// 64 bit platform.
				x64		= 0x02
			};
		}

		typedef auth_platform::Type AuthPlatform;

		/// Enumerates possible operating systems a client can run on.
		namespace auth_system
		{
			enum Type
			{
				/// Unknown or unsupported operating system.
				Unknown = 0x00,
				/// Windows operating system.
				Windows = 0x01,
				/// Mac OS X operating system.
				MacOS	= 0x02
			};
		}

		typedef auth_system::Type AuthSystem;

		/// Enumerates possible client localizations.
		namespace auth_locale
		{
			enum Type
			{
				/// Unknown or unsupported locale.
				Unknown = 0x00,
				/// French (France)
				frFR = 0x01,
				/// German (Germany)
				deDE = 0x02,
				/// English (Great Britain)
				enGB = 0x03,
				/// English (USA)
				enUS = 0x04,
				/// Italian (Italy)
				itIT = 0x05,
				/// Korean (Korea)
				koKR = 0x06,
				/// Simplified Chinese (China)
				zhCN = 0x07,
				/// Traditional Chinese (Taiwan)
				zhTW = 0x08,
				/// Russian (Russia)
				ruRU = 0x09,
				/// Spanish (Spain)
				esES = 0x0A,
				/// Spanish (Mexico)
				esMX = 0x0B,
				/// Portuguese (Brazil)
				ptBR = 0x0C
			};
		}

		typedef auth_locale::Type AuthLocale;

		/// Enumerate possible account flags.
		namespace account_flags
		{
			enum Type
			{
				/// Player account has game master rights.
				GameMaster	= 0x000001,
				/// Player account is a trial account.
				Trial		= 0x000008,
				/// Arena tournament enabled.
				ProPass		= 0x800000
			};
		}

		typedef account_flags::Type AccountFlags;

		namespace realm_flags
		{
			enum Type
			{
				None			= 0x00,
				Invalid			= 0x01,
				Offline			= 0x02,
				SpecifyBuild	= 0x04,		// Client will show realm version in realm list screen: "Realmname (major.minor.patch.build)"
				NewPlayers		= 0x20,
				Recommended		= 0x40,
				Full			= 0x80
			};
		}

		typedef realm_flags::Type RealmFlags;

		struct RealmEntry
		{
			String name;
			String address;
			NetPort port;
			UInt32 icon;
			RealmFlags flags;

			explicit RealmEntry()
				: name("UNNAMED")
				, address("127.0.0.1")
				, port(8127)
				, icon(0)
				, flags(realm_flags::None)
			{
			}

			RealmEntry(const RealmEntry &Other)
				: name(Other.name)
				, address(Other.address)
				, port(Other.port)
				, icon(Other.icon)
				, flags(Other.flags)
			{
			}

			RealmEntry &operator=(const RealmEntry &Other)
			{
				name = Other.name;
				address = Other.address;
				port = Other.port;
				icon = Other.icon;
				flags = Other.flags;
				return *this;
			}
		};

		struct client_read
		{
			static bool logonChallenge(
				io::Reader &packet,
				UInt8 &out_version1,
				UInt8 &out_version2,
				UInt8 &out_version3,
				UInt16 &out_build,
				AuthPlatform &out_platform,
				AuthSystem &out_system,
				AuthLocale &out_locale,
				String &out_userName
				);

			static bool logonProof(
				io::Reader &packet,
				std::array<UInt8, 32> &out_A,
				std::array<UInt8, 20> &out_M1,
				std::array<UInt8, 20> &out_crc_hash,
				UInt8 &out_number_of_keys,
				UInt8 &out_securityFlags
				);

			static bool realmList(
				io::Reader &packet
				);
		};


		struct server_write
		{
			static void logonChallenge(
				auth::OutgoingPacket &out_packet,
				auth_result::Type result,
				const BigNumber &B,
				const BigNumber &g,
				const BigNumber &N,
				const BigNumber &s,
				const BigNumber &unk3
				);

			static void logonProof(
				auth::OutgoingPacket &out_packet,
				auth_result::Type result,
				const std::array<UInt8, 20> &hash
				);

			static void realmList(
				auth::OutgoingPacket &out_packet,
				const std::vector<RealmEntry> &realmList
				);
		};
	}
}
