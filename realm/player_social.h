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

#include "common/typedefs.h"
#include "game/defines.h"
#include "player_manager.h"
#include "player.h"

namespace wowpp
{
	/// Handles the social list of this player.
	class PlayerSocial final
	{
	public:

		/// Initializes a new empty social list which will be assigned to
		/// a specific player (used in the sendSocialList method for example).
		/// @param player The player to assign this social list to.
		explicit PlayerSocial(PlayerManager &manager, Player &player);
		~PlayerSocial();	// For unique_ptr forward declaration

		/// Adds a new contact to our social list.
		/// @param guid The contact's guid.
		/// @param ignore True to add this friend to the ignore list, otherwise from the friend list.
		game::FriendResult addToSocialList(UInt64 guid, bool ignore);
		/// Removes a contact from our social list.
		/// @param guid The contact's guid.
		/// @param ignore True to remove this friend from the ignore list, otherwise from the friend list.
		game::FriendResult removeFromSocialList(UInt64 guid, bool ignore);
		/// Updates the note of a specific friend. Target has to be a friend!
		/// @param guid The friends guid.
		/// @param note The new note of the friend.
		void setFriendNote(UInt64 guid, String note);
		/// Sends the current social list to the owning player.
		void sendSocialList();
		/// Determines if the specified guid is a friend of us.
		/// @param guid The contact's guid.
		bool isFriend(UInt64 guid) const;
		/// Determines if the specified guid is ignored by us.
		/// @param guid The contact's guid.
		bool isIgnored(UInt64 guid) const;

		/// Sends a packet to all friends of this player who are online right now.
		template<class F>
		void sendToFriends(F generator)
		{
			// TODO: This could be a performance problem if there are a lot players online.
			// It would probably be the best to have a separate map to lookup all players who
			// have marked this player as their friend so we wouldn't need to check every connected
			// player!

			// Find all players who are online right now
			auto &myself = m_player;
			m_manager.foreachPlayer([&generator, &myself](Player &player)
				{
					// Check the players social list
					if (player.getSocial().isFriend(myself.getCharacterId()))
					{
						// Found a character who added us as his friend, so notify him
						player.sendPacket(generator);
					}
				});
		}

	private:

		PlayerManager &m_manager;
		Player &m_player;
		game::SocialInfoMap m_contacts;
	};
}
