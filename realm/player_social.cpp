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
#include "player_social.h"
#include "player.h"

namespace wowpp
{
	PlayerSocial::PlayerSocial(PlayerManager &manager, Player &player)
		: m_manager(manager)
		, m_player(player)
	{
	}

	PlayerSocial::~PlayerSocial()
	{
	}

	game::FriendResult PlayerSocial::addToSocialList(UInt64 guid, bool ignore)
	{
		auto characterId = m_player.getCharacterId();
		if (guid == characterId)
		{
			return (ignore ? game::friend_result::IgnoreSelf : game::friend_result::Self);
		}

		// Build flags
		UInt32 flag = (ignore ? game::social_flag::Ignored : game::social_flag::Friend);

		// Check if this contact is already in the social list
		auto it = m_contacts.find(guid);
		if (it != m_contacts.end())
		{
			if (it->second.flags & flag)
			{
				return (ignore ? game::friend_result::IgnoreAlreadyAdded : game::friend_result::AlreadyAdded);
			}
			else
			{
				// Change the flags (remove from friend list, add to ignore list and vice versa)
				it->second.flags |= flag;
			}
		}
		else
		{
			// Couldn't find the specified contact - add him
			game::SocialInfo info;
			info.flags = flag;
			m_contacts[guid] = std::move(info);
		}

		// Successfully added / switched mode
		return (ignore ? game::friend_result::IgnoreAdded : game::friend_result::AddedOffline);
	}

	game::FriendResult PlayerSocial::removeFromSocialList(UInt64 guid, bool ignore)
	{
		// Find the friend info
		auto it = m_contacts.find(guid);
		if (it == m_contacts.end())
		{
			return (ignore ? game::friend_result::IgnoreNotFound : game::friend_result::NotFound);
		}

		// Remove the contact from our social list
		UInt32 flag = (ignore ? game::social_flag::Ignored : game::social_flag::Friend);
		it->second.flags &= ~flag;
		if (it->second.flags == 0)
		{
			// No more flags - remove it entirely
			m_contacts.erase(it);
		}

		return (ignore ? game::friend_result::IgnoreRemoved : game::friend_result::Removed);
	}

	void PlayerSocial::setFriendNote(UInt64 guid, String note)
	{
		// Find the friend info
		auto it = m_contacts.find(guid);
		if (it == m_contacts.end())
		{
			return;
		}

		if (it->second.flags & game::social_flag::Friend)
		{
			it->second.note = std::move(note);
		}
	}

	void PlayerSocial::sendSocialList()
	{
		// Refresh the contact list
		for (auto &it : m_contacts)
		{
			// If it is a friend...
			if (it.second.flags & game::social_flag::Friend)
			{
				// Check if the friend is online and update the friend info
				Player *player = m_manager.getPlayerByCharacterGuid(it.first);
				if (player)
				{
					// Update area etc.
					GameCharacter *character = player->getGameCharacter();
					if (character)
					{
						it.second.status = game::friend_status::Online;
						it.second.level = character->getLevel();
						it.second.area = character->getZone();
						it.second.class_ = character->getClass();
						//TODO: AFK? DND?
					}
					else
					{
						WLOG("Could not get player character for friend list update!");
						it.second.status = game::friend_status::Offline;
					}
				}
				else
				{
					it.second.status = game::friend_status::Offline;
				}
			}
		}

		// Send the current contact list
		m_player.sendPacket(
			std::bind(game::server_write::contactList, std::placeholders::_1, std::cref(m_contacts)));
	}

	bool PlayerSocial::isFriend(UInt64 guid) const
	{
		// Find the friend info
		auto it = m_contacts.find(guid);
		if (it == m_contacts.end())
		{
			return false;
		}

		// Check the flags
		return (it->second.flags & game::social_flag::Friend) != 0;
	}

	bool PlayerSocial::isIgnored(UInt64 guid) const
	{
		// Find the friend info
		auto it = m_contacts.find(guid);
		if (it == m_contacts.end())
		{
			return false;
		}

		// Check the flags
		return (it->second.flags & game::social_flag::Ignored) != 0;
	}

	bool PlayerSocial::getIgnoreList(std::vector<UInt64> &out_list)
	{
		// Iterate through all social contacts
		for (auto &contact : m_contacts)
		{
			// Check if this contact is ignored
			if ((contact.second.flags & game::social_flag::Ignored) != 0)
			{
				// Add the contacts GUID to the output list
				out_list.push_back(contact.first);
			}
		}
		
		return true;
	}
}
