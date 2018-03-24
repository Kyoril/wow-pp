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
#include "movement_validation.h"
#include "game/game_unit.h"
#include "game_protocol/op_codes.h"

namespace wowpp
{
	/// Combination of move flag inclusive and exclusive list.
	struct MoveFlagPermission
	{
		/// None of these flags MAY BE SET when the given movement flag is set.
		UInt32 exclusive;
		/// Any of these flags HAVE TO BE SET when the given movement flag is set.
		UInt32 inclusive;
		/// Flag may be enabled by the following op codes only.
		UInt16 transitionEnabledOpcode;
		/// Flag may be disabled by the following op codes only.
		UInt16 transitionDisabledOpcode;
	};

	using namespace game::movement_flags;
	using namespace game::client_packet;

	/// Contains a UInt32 value with movement flags that may NOT be set if the flag [index] is set.
	static const std::map<UInt32, MoveFlagPermission> moveFlagPermissions{
		//Flag				  Exclusive							  Inclusive			  Enabled op code		  Disabled op code
		{ Forward,			{ Backward | Root					, None				, 0						, 0						} },
		{ Backward,			{ Forward | Root					, None				, 0						, 0						} },
		{ StrafeLeft,		{ StrafeRight | Root				, None				, 0						, 0						} },
		{ StrafeRight,		{ StrafeLeft | Root					, None				, 0						, 0						} },
		{ TurnLeft,			{ TurnRight							, None				, 0						, 0						} },
		{ TurnRight,		{ TurnLeft							, None				, 0						, 0						} },
		{ PitchUp,			{ PitchDown | Root					, Swimming | Flying , 0						, 0						} },
		{ PitchDown,		{ PitchUp | Root					, Swimming | Flying , 0						, 0						} },
		{ WalkMode,			{ None								, None				, 0						, 0						} },
		{ OnTransport,		{ None								, None				, MoveChangeTransport	, MoveChangeTransport	} },
		{ Levitating,		{ None								, CanFly			, 0						, 0						} },
		{ Root,				{ Moving							, None				, ForceMoveRootAck		, ForceMoveUnrootAck	} },
		{ Falling,			{ Root | Flying 					, None				, 0						, 0						} },
		{ FallingFar,		{ Root | Swimming | Flying			, None				, 0						, 0						} },
		{ Swimming,			{ FallingFar						, None				, MoveStartSwim			, MoveStopSwim			} },
		{ Ascending,		{ Descending | Root					, Swimming | Flying , 0						, 0						} },
		{ Descending,		{ Ascending | Root					, Swimming | Flying , 0						, 0						} },
		{ CanFly,			{ Falling							, None				, MoveSetCanFlyAck		, MoveSetCanFlyAck		} },
		{ Flying,			{ Falling							, CanFly			, 0						, 0						} },
		{ SplineElevation,	{ None								, None				, 0						, 0						} },
		{ SplineEnabled,	{ None								, None				, 0						, 0						} },
		{ WaterWalking,		{ None								, None				, MoveWaterWalkAck		, MoveWaterWalkAck		} },
		{ SafeFall,			{ None								, None				, MoveFeatherFallAck	, MoveFeatherFallAck	} },
		{ Hover,			{ None								, None				, MoveHoverAck			, MoveHoverAck			} }
	};
	

	bool validateMovementInfo(UInt16 opCode, const MovementInfo& clientInfo, const MovementInfo& serverInfo)
	{
		// Process move flag permission table
		for (const auto& pair : moveFlagPermissions)
		{
			// Validate that transitions are allowed using the given opCode
			if (opCode != 0)
			{
				// Check for transition into "enabled" state
				if (pair.second.transitionEnabledOpcode != 0 &&
					pair.second.transitionEnabledOpcode != opCode &&
					(clientInfo.moveFlags & pair.first) && !(serverInfo.moveFlags & pair.first))
				{
					WLOG("MoveFlag enabled transition check failed for move flag " << pair.first 
						<< ": flag enabled in opcode 0x" << std::hex << opCode);
					return false;
				}

				// Check for transition into "disabled" state
				if (pair.second.transitionDisabledOpcode != 0 &&
					pair.second.transitionDisabledOpcode != opCode &&
					(serverInfo.moveFlags & pair.first) && !(clientInfo.moveFlags & pair.first))
				{
					WLOG("MoveFlag disabled transition check failed for move flag " << pair.first 
						<< ": flag disabled in opcode 0x" << std::hex << opCode);
					return false;
				}
			}

			// Only check inclusives and exclusives if flag is set in new (client) move flags
			if (clientInfo.moveFlags & pair.first)
			{
				// Check for exclusive flags (forbidden move flag combinations)
				if (pair.second.exclusive != None && (clientInfo.moveFlags & pair.second.exclusive))
				{
					WLOG("Exclusive move flag validation failed for move flag " << pair.first);
					return false;
				}

				// Check for missing movement flags (required dependencies, like CanFly for Flying)
				if (pair.second.inclusive != None && !(clientInfo.moveFlags & pair.second.inclusive))
				{
					WLOG("Inclusive move flag validation failed for move flag " << pair.first);
					return false;
				}
			}
		}

		// Falling flags have to be removed in MoveFallLand
		if ((opCode == MoveFallLand || opCode == MoveStartSwim || opCode == MoveSetFly) && 
			(clientInfo.moveFlags & (Falling | FallingFar)))
		{
			WLOG("Falling flag detected which shouldn't be there");
			return false;
		}

		// Falling flag has to be set in MSG_MOVE_FALL_RESET and MSG_MOVE_JUMP
		if ((opCode == MoveFallReset || opCode == MoveJump) && 
			!(clientInfo.moveFlags & Falling))
		{
			WLOG("Missing move flag Falling detected");
			return false;
		}

		return true;
	}

	bool validateSpeedAck(const PendingMovementChange& change, float receivedSpeed, MovementType& outMoveTypeSent)
	{
		switch (change.changeType)
		{
			case MovementChangeType::SpeedChangeWalk:				outMoveTypeSent = movement_type::Walk; break;
			case MovementChangeType::SpeedChangeRun:				outMoveTypeSent = movement_type::Run; break;
			case MovementChangeType::SpeedChangeRunBack:			outMoveTypeSent = movement_type::Backwards; break;
			case MovementChangeType::SpeedChangeSwim:				outMoveTypeSent = movement_type::Swim; break;
			case MovementChangeType::SpeedChangeSwimBack:			outMoveTypeSent = movement_type::SwimBackwards; break;
			case MovementChangeType::SpeedChangeTurnRate:			outMoveTypeSent = movement_type::Turn; break;
			case MovementChangeType::SpeedChangeFlightSpeed:		outMoveTypeSent = movement_type::Flight; break;
			case MovementChangeType::SpeedChangeFlightBackSpeed:	outMoveTypeSent = movement_type::FlightBackwards; break;
			default:
				WLOG("Incorrect ack data for speed change ack");
				return false;
		}

		if (std::fabs(receivedSpeed - change.speed) > FLT_EPSILON)
		{
			WLOG("Incorrect speed value received in ack");
			return false;
		}

		return true;
	}

	bool validateMoveFlagsOnApply(bool apply, UInt32 flags, UInt32 possiblyAppliedFlags)
	{
		// If apply is true, at least one of the possible flags needs to be set
		if (apply && !(flags & possiblyAppliedFlags))
		{
			return false;
		}

		// If apply is false, none of the possible flags may be set
		if (!apply && (flags & possiblyAppliedFlags))
		{
			return false;
		}

		return true;
	}
}