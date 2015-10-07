
#pragma once

#include "game_protocol/game_protocol.h"

namespace wowpp
{
	/// Basic interface which will be used for tile subscribers.
	struct ITileSubscriber
	{
		virtual ~ITileSubscriber() { }

		/// 
		virtual UInt32 convertTimestamp(UInt32 otherTimestamp, UInt32 otherTicks) const = 0;
		/// Gets the controlled object (if any) of the subscriber. Could be nullptr!
		virtual GameCharacter *getControlledObject() = 0;
		/// Sends a packet to the tile subscriber.
		virtual void sendPacket(game::Protocol::OutgoingPacket &packet, const std::vector<char> &buffer) = 0;
	};
}