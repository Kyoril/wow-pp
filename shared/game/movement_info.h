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

#include "game/defines.h"
#include "binary_io/writer.h"
#include "binary_io/reader.h"
#include <vector>

namespace wowpp
{
	namespace movement_type
	{
		enum Type
		{
			Walk			= 0,
			Run				= 1,
			Backwards		= 2,
			Swim			= 3,
			SwimBackwards	= 4,
			Turn			= 5,
			Flight			= 6,
			FlightBackwards	= 7,

			Count			= 8
		};
	}

	typedef movement_type::Type MovementType;

	/// 
	class MovementInfo final
	{
	public:

		/// 
		explicit MovementInfo();

	public:

		// Character data
		game::MovementFlags moveFlags;
		UInt8 moveFlags2;
		UInt32 time;
		float x, y, z, o;
		// Transport data
		UInt64 transportGuid;
		float tX, tY, tZ, tO;
		UInt32 transportTime;
		// Swimming and flying data
		float pitch;
		// Last fall time
		UInt32 fallTime;
		// Jumping data
		float jumpVelocity, jumpSinAngle, jumpCosAngle, jumpXYSpeed;
		// Taxi data
		float unknown1;
	};

	io::Writer &operator << (io::Writer &w, MovementInfo const& info);
	io::Reader &operator >> (io::Reader &r, MovementInfo& info);
}
