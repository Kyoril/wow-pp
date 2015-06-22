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

#include "common/typedefs.h"
#include "game/defines.h"

namespace wowpp
{
	class Player;

	/// Handles the social list of this player.
	class PlayerSocial final
	{
	public:

		/// Initializes a new empty social list which will be assigned to
		/// a specific player (used in the sendSocialList method for example).
		/// @param player The player to assign this social list to.
		explicit PlayerSocial(Player &player);

		/// Adds a new contact to our social list.
		/// @param guid The contact's guid.
		/// @param ignore True to add this friend to the ignore list, otherwise from the friend list.
		game::FriendResult addToSocialList(UInt32 guid, bool ignore);
		/// Removes a contact from our social list.
		/// @param guid The contact's guid.
		/// @param ignore True to remove this friend from the ignore list, otherwise from the friend lsit.
		void removeFromSocialList(UInt32 guid, bool ignore);
		/// Updates the note of a specific friend. Target has to be a friend!
		/// @param guid The friends guid.
		/// @param note The new note of the friend.
		void setFriendNote(UInt32 guid, String note);
		/// Sends the current social list to the owning player.
		void sendSocialList() const;
		/// Determines if the specified guid is a friend of us.
		/// @param guid The contact's guid.
		bool isFriend(UInt32 guid) const;
		/// Determines if the specified guid is ignored by us.
		/// @param guid The contact's guid.
		bool isIgnored(UInt32 guid) const;

	private:

		Player &m_player;
	};
}
