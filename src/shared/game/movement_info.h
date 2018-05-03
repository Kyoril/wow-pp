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
				/// Handy readable shortcut. Used when the player does absolutely nothing, and is just standing without
				/// any special movement abilities.
				None = 0x00000000,

				/// Set when the player has pressed the "Move forward" key (usually W)
				Forward = 0x00000001,
				/// Set when the player has pressed the "Move backward" key (usually S)
				Backward = 0x00000002,
				/// Set when the player has pressed the "Move strafe left" key (usually Q)
				StrafeLeft = 0x00000004,
				/// Set when the player has pressed the "Move strafe right" key (usually E)
				StrafeRight = 0x00000008,
				/// Set when the player has pressed the "Turn left" key (usually A).
				TurnLeft = 0x00000010,
				/// Set when the player has pressed the "Turn right" key (usually D).
				TurnRight = 0x00000020,


				PitchUp = 0x00000040,

				PitchDown = 0x00000080,

				/// Set when the player has enabled the walk mode, even if he is not moving at all.
				WalkMode = 0x00000100,
				/// Set while the player is on a transport.
				OnTransport = 0x00000200,

				/// Set while the player is actively flying. Isn't set by priest spell levitate. Unknown purpose...
				Levitating = 0x00000400,

				/// Set while the player is rooted. Only set while the player is not falling!
				Root = 0x00000800,

				/// Set while the player is jumping or falling, and hasn't passed the height value that he had
				/// while this flag was applied.
				Falling = 0x00001000,
				/// Set while the player is falling and has passed the height value that he had while the Falling
				/// flag was applied.
				FallingFar = 0x00002000,

				/// Set while the player is rooted but still falling. The current jump / fall will be continued
				/// and the PendingRoot flag has to be transformed into a Root flag on landing.
				PendingRoot = 0x00100000,

				/// Set while the player is swimming (not touching the ground).
				Swimming = 0x00200000,

				/// Set while the player is moving strafe upwards. Only possible while flying or swimming.
				Ascending = 0x00400000,
				/// Set while the player is moving strafe downwards. Only possible while flying or swimming.
				Descending = 0x00800000,

				/// Set while the unit is able to fly.
				CanFly = 0x01000000,
				/// Set while the unit is actively flying.
				Flying = 0x02000000,

				/// TODO
				SplineElevation = 0x04000000,
				/// TODO
				SplineEnabled = 0x08000000,

				/// Set while the unit is able to walk on water, even if the unit isn't moving at all or on water.
				WaterWalking = 0x10000000,
				/// Set while the unit is slow falling, even if the unit isn't falling at all.
				SafeFall = 0x20000000,
				/// Set while the unit is hovering (priest spell: levitate).
				Hover = 0x40000000,

				/// Combination of all flags that cause a position change.
				Moving =
					Forward | Backward | StrafeLeft | StrafeRight | PitchUp | PitchDown |
					Falling | FallingFar | Ascending | Descending | SplineElevation,
				/// Combination of all flags that cause a rotation change.
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

		/// Movement flags which describe the current movement state.
		game::MovementFlags moveFlags;
		/// Unknown purpose right now...
		UInt8 moveFlags2;
		/// The movement timestamp at the client side.
		UInt32 time;
		/// Absolute location and rotation values.
		float x, y, z, o;

		// Transport data
		
		/// Transporter guid if any.
		UInt64 transportGuid;
		/// Location and rotation values relative to the transporters world location.
		float tX, tY, tZ, tO;
		/// The current transporter time value at the client side.
		UInt32 transportTime;

		// Swimming and flying data
		
		/// Pitch value.
		float pitch;
		/// The amount of time that the player is falling in milliseconds.
		UInt32 fallTime;
		/// Jump data only set when the player is jumping or is knocked back. Not set
		/// if the player is simply falling down without jumping before (for example).
		float jumpVelocity, jumpSinAngle, jumpCosAngle, jumpXYSpeed;
		
		// Taxi data

		/// Unknown purpose right now.
		float unknown1;

	public:
		// Data which isn't sent to the client but is used by the
		// server for movement validation for example.

		/// Z value as it was when the player started jumping.
		float jumpStartZ;
	};

	io::Writer &operator << (io::Writer &w, MovementInfo const &info);
	io::Reader &operator >> (io::Reader &r, MovementInfo &info);


	/// Determines whether a unit started falling down based on two movement infos.
	/// @param clientInfo The new movement info sent by the client.
	/// @param serverInfo The old movement info known by the server from the last move packet or spawn.
	/// @returns true if the unit started falling in this new movement packet.
	inline bool unitStartsFalling(const MovementInfo& clientInfo, const MovementInfo& serverInfo)
	{
		return (clientInfo.moveFlags & game::movement_flags::Falling) && !(serverInfo.moveFlags & game::movement_flags::Falling);
	}
}
