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

#include "binary_io/writer.h"
#include "binary_io/reader.h"
#include "common/typedefs.h"

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

	// TODO: Remove this namespace maybe. Was ported here to avoid including game/defines.h here
	namespace game
	{
		namespace movement_flags
		{
			enum Type
			{
				None = 0x00000000,
				Forward = 0x00000001,
				Backward = 0x00000002,
				StrafeLeft = 0x00000004,
				StrafeRight = 0x00000008,
				TurnLeft = 0x00000010,
				TurnRight = 0x00000020,
				PitchUp = 0x00000040,
				PitchDown = 0x00000080,
				WalkMode = 0x00000100,
				OnTransport = 0x00000200,
				Levitating = 0x00000400,
				Root = 0x00000800,
				Falling = 0x00001000,
				FallingFar = 0x00004000,
				Swimming = 0x00200000,
				Ascending = 0x00400000,
				Descending = 0x00800000,
				CanFly = 0x01000000,
				Flying = 0x02000000,
				SplineElevation = 0x04000000,
				SplineEnabled = 0x08000000,
				WaterWalking = 0x10000000,
				SafeFall = 0x20000000,
				Hover = 0x40000000,

				Moving =
				Forward | Backward | StrafeLeft | StrafeRight | PitchUp | PitchDown |
				Falling | FallingFar | Ascending | Descending | SplineElevation,
				Turning =
				TurnLeft | TurnRight,

			};
		}

		typedef movement_flags::Type MovementFlags;
	}

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

	io::Writer &operator << (io::Writer &w, MovementInfo const &info);
	io::Reader &operator >> (io::Reader &r, MovementInfo &info);
}
