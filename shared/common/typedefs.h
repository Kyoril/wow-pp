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

#pragma once

#include <cstdint>
#include <string>

namespace wowpp
{
	// Unsigned typedefs
	typedef std::uint8_t UInt8;
	typedef std::uint16_t UInt16;
	typedef std::uint32_t UInt32;
	typedef std::uint64_t UInt64;

	// Signed typedefs
	typedef std::int8_t Int8;
	typedef std::int16_t Int16;
	typedef std::int32_t Int32;
	typedef std::int64_t Int64;

	// String
	typedef std::string String;
	
	// Network typedefs
	typedef UInt8 NetUInt8;
	typedef UInt16 NetUInt16;
	typedef UInt32 NetUInt32;
	typedef UInt64 NetUInt64;
	typedef Int8 NetInt8;
	typedef Int16 NetInt16;
	typedef Int32 NetInt32;
	typedef Int64 NetInt64;
	typedef UInt16 NetPort;

	// Game
	typedef UInt32 Identifier;
	typedef NetUInt32 NetIdentifier;
	typedef UInt64 ObjectGuid;
	typedef NetUInt64 NetObjectGuid;
	typedef UInt64 DatabaseId;
	typedef NetUInt64 NetDatabaseId;
	typedef UInt64 GameTime;
	typedef NetUInt64 NetGameTime;
	typedef Identifier PacketBegin;
	typedef NetUInt8 NetPacketBegin;
	typedef Identifier PacketId;
	typedef NetUInt8 NetPacketId;
	typedef UInt16 PacketSize;
	typedef NetUInt16 NetPacketSize;
}
