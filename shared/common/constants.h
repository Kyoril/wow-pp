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

#include "typedefs.h"
#include "common/big_number.h"

namespace wowpp
{
	namespace constants
	{
		/// This port is used by the WoW Client to connect with the login server.
		static const NetPort DefaultLoginPlayerPort = 3724;
		/// This is the default port used by a world server.
		static const NetPort DefaultWorldPlayerPort = 8129;
		/// This is the default port on which the login server listens for realms.
		static const NetPort DefaultLoginRealmPort = 6279;
		/// This is the default port on which the realm server listens for worlds.
		static const NetPort DefaultRealmWorldPort = 6280;
		/// This is the default port used by MySQL servers.
		static const NetPort DefaultMySQLPort = 3306;

		/// Every packet of the wowpp protocol has to start with this constant.
		static const NetPacketBegin PacketBegin = 0xfc;

		/// Invalid identifier constant
		static const UInt32 InvalidId = 0xffffffff;

		// These are shared constants used by srp-6 calculations
		namespace srp
		{
			static const BigNumber N("894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7");
			static const BigNumber g(7);
		}
	}
}
