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

#include "player_social.h"
#include "player.h"

namespace wowpp
{
	PlayerSocial::PlayerSocial(Player &player)
		: m_player(player)
	{
	}

	game::FriendResult PlayerSocial::addToSocialList(UInt32 guid, bool ignore)
	{
		// Build flags
		UInt32 flag = game::social_flag::Friend;
		if (ignore)
			flag = game::social_flag::Ignored;

		// TODO: Check if this contact is already in the social list

		// TODO: Add
		return game::friend_result::AddedOffline;
	}

	void PlayerSocial::removeFromSocialList(UInt32 guid, bool ignore)
	{

	}

	void PlayerSocial::setFriendNote(UInt32 guid, String note)
	{

	}

	void PlayerSocial::sendSocialList() const
	{

	}

	bool PlayerSocial::isFriend(UInt32 guid) const
	{
		//TODO
		return false;
	}

	bool PlayerSocial::isIgnored(UInt32 guid) const
	{
		//TODO
		return false;
	}
}
