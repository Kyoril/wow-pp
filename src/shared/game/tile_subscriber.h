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

#include "game_protocol/game_protocol.h"

namespace wowpp
{
	/// Basic interface which will be used for tile subscribers.
	struct ITileSubscriber
	{
		virtual ~ITileSubscriber() { }

		/// Determines whether this subscriber wants to ignore a specific guid.
		virtual bool isIgnored(UInt64 guid) const = 0;
		///
		virtual UInt32 convertTimestamp(UInt32 otherTimestamp, UInt32 otherTicks) const = 0;
		/// Gets the controlled object (if any) of the subscriber. Could be nullptr!
		virtual GameCharacter *getControlledObject() = 0;
		/// Sends a packet to the tile subscriber.
		virtual void sendPacket(game::Protocol::OutgoingPacket &packet, const std::vector<char> &buffer) = 0;

		// TODO: We need to make sure that we know whose objects are spawned
	};
}