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
#include "game/cheat_log.h"

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
		{ Root,				{ Moving | PendingRoot				, None				, 0			 		, ForceMoveUnrootAck	} },
		{ PendingRoot,		{ Root								, Falling			, ForceMoveRootAck		, 0						} },
		{ Falling,			{ Root | Flying 					, None				, 0						, 0						} },
		{ FallingFar,		{ Root | Swimming					, Falling			, 0						, 0						} },
		{ Swimming,			{ FallingFar						, None				, MoveStartSwim			, 0						} },
		{ Ascending,		{ Descending | Root					, Swimming | Flying , 0						, 0						} },
		{ Descending,		{ Ascending | Root					, Swimming | Flying , 0						, 0						} },
		{ CanFly,			{ None								, None				, MoveSetCanFlyAck		, MoveSetCanFlyAck		} },
		{ Flying,			{ Falling							, CanFly			, 0						, 0						} },
		{ SplineElevation,	{ None								, None				, 0						, 0						} },
		{ SplineEnabled,	{ None								, None				, 0						, 0						} },
		{ WaterWalking,		{ None								, None				, MoveWaterWalkAck		, MoveWaterWalkAck		} },
		{ SafeFall,			{ None								, None				, MoveFeatherFallAck	, MoveFeatherFallAck	} },
		{ Hover,			{ None								, None				, MoveHoverAck			, MoveHoverAck			} }
	};
	

	bool validateMovementInfo(UInt16 opCode, const MovementInfo& clientInfo, const MovementInfo& serverInfo, float allowedMoveSpeed)
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
					CLOG("MoveFlag enabled transition check failed for move flag 0x" << std::hex << pair.first
						<< ": flag enabled in opcode 0x" << std::hex << opCode);
					return false;
				}

				// Check for transition into "disabled" state
				if (pair.second.transitionDisabledOpcode != 0 &&
					pair.second.transitionDisabledOpcode != opCode &&
					(serverInfo.moveFlags & pair.first) && !(clientInfo.moveFlags & pair.first))
				{
					CLOG("MoveFlag disabled transition check failed for move flag 0x" << std::hex << pair.first
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
					CLOG("Exclusive move flag validation failed for move flag 0x" << std::hex << pair.first << ": move flags sent: 0x" << clientInfo.moveFlags);
					return false;
				}

				// Check for missing movement flags (required dependencies, like CanFly for Flying)
				if (pair.second.inclusive != None && !(clientInfo.moveFlags & pair.second.inclusive))
				{
					CLOG("Inclusive move flag validation failed for move flag 0x" << std::hex << pair.first << ": move flags sent: 0x" << clientInfo.moveFlags);
					return false;
				}
			}
		}

        UInt32 timeDiff = clientInfo.time - serverInfo.time;

		// Time only ever goes forward
		if (clientInfo.time < serverInfo.time)
		{
			CLOG("Client sent invalid timestamp in movement packet.");
			return false;
		}

		// While moving, heart beats can't be sent with more than 500ms timestamp difference from the client
		// TODO: With MoveSplineDone we receive much higher timestamps than 500 ms as we don't get any movement packets
		// while spline-moving. This should be checked somehow too, I guess, but it breaks this check here if you were
		// running before charging.
		if (serverInfo.time != 0 && (serverInfo.moveFlags & Moving) && (opCode != MoveSplineDone) && (clientInfo.time - serverInfo.time) > 500)
		{
			CLOG("Client sent too large timestamp in movement packet.");
			return false;
		}

		// MoveFallReset is only valid if the player was already falling
		if (opCode == MoveFallReset)
		{
			if (!(serverInfo.moveFlags & (Falling | FallingFar)))
			{
				CLOG("Client tried to reset a fall but wasn't falling!");
				return false;
			}

			if (fabs(clientInfo.jumpVelocity) > FLT_EPSILON)
			{
				CLOG("Client tried to send nonzero fall velocity in FallReset!");
				return false;
			}

			if (clientInfo.fallTime != 0)
			{
				CLOG("Client tried to send nonzero fall time in FallReset packet!");
				return false;
			}

			if (serverInfo.fallTime > 500)
			{
				CLOG("Client tried to reset a deep-fall.");
				return false;
			}
		}

		// If the player was already falling, he can't jump until he landed
		if (opCode == MoveJump)
		{
			if (serverInfo.moveFlags & (Falling | FallingFar))
			{
				CLOG("Client tried to send jump packet but was already falling!");
				return false;
			}

			if (clientInfo.fallTime != 0)
			{
				CLOG("Client tried to send nonzero fall time in MoveJump packet!");
				return false;
			}
		}

		// MoveFallReset and MoveJump should start a fresh fall
		if (opCode == MoveFallReset || opCode == MoveJump)
		{
			if (!(clientInfo.moveFlags & (Falling | FallingFar)))
			{
				CLOG("Client sent FallReset or Jump without falling flag set!");
				return false;
			}
		}

		// Client was about to get rooted, but he removed his pending root flag
		if ((serverInfo.moveFlags & PendingRoot) && !(clientInfo.moveFlags & PendingRoot))
		{
			// If the client doesn't have a full root set in this case, he is cheating
			if (!(clientInfo.moveFlags & Root) && opCode != ForceMoveUnrootAck)
			{
				CLOG("Client tried to remove PendingRoot flag without setting rooted flag!");
				return false;
			}
		}

		// Client got rooted, this can only happen in ForceMoveRoot, MoveFallLand, MoveSetFly or MoveStartSwim
		if ((clientInfo.moveFlags & Root) && !(serverInfo.moveFlags & Root))
		{
			if (opCode != ForceMoveRootAck && opCode != MoveFallLand && opCode != MoveSetFly && opCode != MoveStartSwim)
			{
				CLOG("Client tried to set rooted flag in wrong packet!");
				return false;
			}
		}

		// MoveSetFly can only occur if the client has the CanFly flag
		if (opCode == MoveSetFly && !(clientInfo.moveFlags & CanFly))
		{
			CLOG("Client sent MoveSetFly opcode but he cannot fly.");
			return false;
		}
		
		// If the client was falling before and is still falling...
		if ((clientInfo.moveFlags & Falling) && (serverInfo.moveFlags & Falling))
		{
			// The acceleration in fall time
			const float Acceleration = 19.75f;

			// Get the current fall time in seconds
			const float FallTimeSec = (clientInfo.fallTime / 1000.0f);

			// Determine the expected fall speed value based on fall time in seconds, and use the
			// client cap of -60.149 units/seconds as max fall speed
			const float ExpectedFallSpeed = std::min(60.149f, FallTimeSec * Acceleration);

			// Calculate fall speed
			const float FallTime = ((clientInfo.fallTime - serverInfo.fallTime) / 1000.0f);
			const float FallSpeed = FallTime != 0.0f ? ((clientInfo.z - serverInfo.z) / FallTime) : 0.0f;
			if (-FallSpeed > ExpectedFallSpeed)
			{
				CLOG("Client fall speed higher than expected");
				return false;
			}

			// ... we check for removal of the FallingFar flag which shouldn't be possible without landing!
			if (!(clientInfo.moveFlags & FallingFar) && (serverInfo.moveFlags & FallingFar))
			{
				CLOG("Client tried to remove FallingFar while still falling!");
				return false;
			}
		}


		// If the player is falling but didn't set FallingFar so far, we check if he should have
		// set this flag by now
		if (!(clientInfo.moveFlags & FallingFar) && (clientInfo.moveFlags & Falling))
		{
			if (clientInfo.z < clientInfo.jumpStartZ + clientInfo.jumpVelocity && clientInfo.fallTime >= 500)
			{
				CLOG("Client didn't set FALLING_FAR flag after falling too far!");
				return false;
			}

			if (clientInfo.z > clientInfo.jumpStartZ - clientInfo.jumpVelocity)
			{
				CLOG("Client jumped higher than possible with his current jumpVelocity value!");
				return false;
			}
		}

		// When FallingFar is enabled, we expect the z value to decrease
		if (clientInfo.moveFlags & FallingFar)
		{
			// TODO: Maybe we should not just check if z has decreased but also if it hasn't decreased too much or less based on gravity and fall time?

			// NOTE: In the first movement packet, client fall time == server fall time, so we don't expect
			// a z change there. Other than that, there has to be a decrease in z!
			if (clientInfo.fallTime > serverInfo.fallTime && clientInfo.z >= serverInfo.z)
			{
				CLOG("Client didn't decrease z axis while FALLING_FAR flag is enabled!");
				return false;
			}
		}

		// We were falling, but aren't falling anymore. This can only happen in a FallLand, SetFly or StartSwim
		if ((serverInfo.moveFlags & (Falling | FallingFar)) && !(clientInfo.moveFlags & (Falling | FallingFar)))
		{
			if (opCode != MoveFallLand && opCode != MoveSetFly && opCode != MoveStartSwim)
			{
				CLOG("Client tried to stop falling with a packet that cannot stop a fall.");
				return false;
			}

			if (serverInfo.time != 0 && serverInfo.fallTime + timeDiff != clientInfo.fallTime)
			{
				CLOG("Client tried to stop a fall but sent invalid fall time in the stopping packet!");
				return false;
			}

			if ((serverInfo.moveFlags & PendingRoot) && !(clientInfo.moveFlags & Root))
			{
				CLOG("Client sent a fall land packet while he had a pending root, but he didn't apply the full root.");
				return false;
			}
		}

		// Player is falling. Do basic fall parameter validations.
		if (clientInfo.moveFlags & (Falling | FallingFar))
		{
			// FallLand or StartSwim can't occur with falling flag
			if (opCode == MoveFallLand || opCode == MoveStartSwim)
			{
				CLOG("Client sent MoveFallLand or MoveStartSwim packet with falling flag!");
				return false;
			}

			// JumpSpeed always has to be a positive value.
			if (clientInfo.jumpXYSpeed < 0.0f)
			{
				CLOG("Client tried to send negative fall speed!");
				return false;
			}

			// JumpVelocity always has to be a negative value. Usually it is -7.96.
			if (clientInfo.jumpVelocity > 0.0f)
			{
				CLOG("Client tried to send impossible jump velocity!");
				return false;
			}

			// If the player was already falling and is still falling, then his fall parameters should stay the same
			if (serverInfo.moveFlags & (Falling | FallingFar))
			{
				// Client is trying to increase his jump speed in the middle of falling
				if (clientInfo.jumpXYSpeed > serverInfo.jumpXYSpeed)
				{
					// The speed can only be increased if the speed was previously zero.
					if (serverInfo.jumpXYSpeed > 0.0f)
					{
						CLOG("Client tried to send different fall speed during 2 subsequent fall packets!");
						return false;
					}

					// 2.5 is the maximum speed that a client can increase his speed by during a jump
					// Example: Stand still and press spacebar, then during the fall you press forward.
					// Your character will jump slightly forward and the maximum speed can not be greater than 2.5
					if (clientInfo.jumpXYSpeed > 2.5f)
					{
						CLOG("Client tried to increase fall speed during 2 subsequent fall packets!");
						return false;
					}
				}

				// MoveFallReset is an exception as it can change the fall angles.
				// This is the packet when you bang your head against something.
				if (opCode != MoveFallReset)
				{
					if (serverInfo.time != 0 && serverInfo.fallTime + timeDiff != clientInfo.fallTime)
					{
						CLOG("Client tried to send invalid fall time during 2 subsequent fall packets!");
						return false;
					}

					if (fabs(clientInfo.jumpVelocity - serverInfo.jumpVelocity) > FLT_EPSILON)
					{
						CLOG("Client tried to send invalid jump velocity during 2 subsequent fall packets!");
						return false;
					}
				}
			}
			else // If the player just started a fresh fall (wasn't falling before), then we need to check if his fall speed is valid
			{
				if ((serverInfo.time != 0 && clientInfo.fallTime > timeDiff) || clientInfo.fallTime > 500)
				{
					CLOG("Client tried to send too large fall time in packet that just started a fresh fall!");
					return false;
				}
				
				// Check here if jumpXYSpeed is > than currently allowed speed.
				if (clientInfo.jumpXYSpeed > allowedMoveSpeed)
				{
					CLOG("Client sent a jumpXYSpeed value that is bigger than the allowed movement speed");
					return false;
				}

				// TODO: Right now, two jumpVelocity values have been observed from a valid client, which are
				// 0.0f in case of simply falling down and -7.95555f in case of a normal jump. I don't know if
				// the client ever sends another value in here actually and need to investigate this further.
				if (clientInfo.jumpVelocity > 0.0f || clientInfo.jumpVelocity < -7.95555f)
				{
					CLOG("Client sent unexpected or invalid jumpVelocity value (" << clientInfo.jumpVelocity << ")!");
					return false;
				}
			}
		}

		// Transport data should stay the same during 2 subsequent packets
		if ((serverInfo.moveFlags & OnTransport) && (clientInfo.moveFlags & OnTransport) &&
			opCode != MoveChangeTransport)
		{
			if (clientInfo.transportGuid != serverInfo.transportGuid)
			{
				CLOG("Client tried to send different transport GUID during 2 subsequent packets!");
				return false;
			}

			if (serverInfo.time != 0 && serverInfo.transportTime + timeDiff != clientInfo.transportTime)
			{
				CLOG("Client tried to send invalid transport time during 2 subsequent packets!");
				return false;
			}
		}

		// When root was applied...
		if (opCode == ForceMoveRootAck)
		{
			// Check if the player was falling. If so, he has to set PendingRoot
			if (clientInfo.moveFlags & (Falling | FallingFar))
			{
				if (!(clientInfo.moveFlags & PendingRoot))
				{
					CLOG("PendingRoot flag required in root ack while falling is active!");
					return false;
				}
			}
			// Player wasn't falling, so he has to set Root
			else
			{
				if (!(clientInfo.moveFlags & Root))
				{
					CLOG("Root flag required in root ack while falling is inactive!");
					return false;
				}
			}
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
				CLOG("Incorrect ack data for speed change ack");
				return false;
		}

		if (std::fabs(receivedSpeed - change.speed) > FLT_EPSILON)
		{
			CLOG("Incorrect speed value received in ack");
			return false;
		}

		return true;
	}

	bool validateMoveFlagsOnApply(bool apply, UInt32 flags, UInt32 possiblyAppliedFlags)
	{
		// If apply is true, at least one of the possible flags needs to be set
		if (apply && !(flags & possiblyAppliedFlags))
		{
			CLOG("Ack apply op code for movement flags (0x" << std::hex << flags << ") should have had one of 0x" << possiblyAppliedFlags);
			return false;
		}

		// If apply is false, none of the possible flags may be set
		if (!apply && (flags & possiblyAppliedFlags))
		{
			CLOG("Ack misapply op code for movement flags (0x" << std::hex << flags << ") should not have one of 0x" << possiblyAppliedFlags);
			return false;
		}

		return true;
	}

	bool validateMovementSpeed(float expectedSpeed, const MovementInfo & clientInfo, const MovementInfo & serverInfo)
	{
		math::Vector3 lastPos(serverInfo.x, serverInfo.y, 0.0f);
		math::Vector3 newPos(clientInfo.x, clientInfo.y, 0.0f);

		const float distanceSq = (lastPos - newPos).squared_length();
		const UInt32 moveTime = clientInfo.time - serverInfo.time;
		const float maxDist =
			static_cast<float>(moveTime) / 1000.0f * expectedSpeed;

		if (distanceSq > ::powf(maxDist + 0.138f, 2.0f))
		{
			CLOG("Distance was too much! Max dist: " << maxDist << ", distance was " << (lastPos - newPos).length() << "\n"
				<< "\tmoveTime: " << moveTime << "\n"
				<< "\texpectedSpeed: " << expectedSpeed << "\n"
				<< "\tisFalling: " << ((serverInfo.moveFlags & (Falling | FallingFar)) != 0));
			return false;
		}

		return true;
	}
}