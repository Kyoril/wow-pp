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
#include "player_group.h"
#include "game/game_character.h"
#include "world.h"
#include "database.h"

namespace wowpp
{
	// Implementation. Will store all available player group instances by their id.
	std::map<UInt64, std::shared_ptr<PlayerGroup>> PlayerGroup::GroupsById;


	PlayerGroup::PlayerGroup(UInt64 id, PlayerManager &playerManager, IDatabase &database)
		: m_id(id)
		, m_playerManager(playerManager)
		, m_database(database)
		, m_leaderGUID(0)
		, m_type(group_type::Normal)
		, m_lootMethod(loot_method::GroupLoot)
		, m_lootTreshold(2)
		, m_lootMaster(0)
	{
		m_targetIcons.fill(0);
	}

	bool PlayerGroup::createFromDatabase()
	{
		// Already created once
		if (m_leaderGUID != 0)
			return true;

		UInt64 leader = 0;
		std::vector<UInt64> memberGuids;
		if (!m_database.loadGroup(m_id, leader, memberGuids))
		{
			// The group will automatically be deleted and not stored when not saved in GroupsById
			ELOG("Could not load group from database");
			return false;
		}

		// Convert leader id into cross realm compatible guid
		m_playerManager.getCrossRealmGUID(leader);

		// Save leader information
		m_leaderGUID = leader;

		// Add the leader first
		if (!addOfflineMember(leader))
		{
			// Leader no longer exists?
			ELOG("Could not add group leader");
			return false;
		}

		// Same for members
		for (auto &memberGuid : memberGuids)
		{
			m_playerManager.getCrossRealmGUID(memberGuid);
			addOfflineMember(memberGuid);
		}

		// Save group for later user
		GroupsById[m_id] = shared_from_this();
		return true;
	}

	void PlayerGroup::create(GameCharacter &leader)
	{
		// Already created once
		if (m_leaderGUID != 0)
			return;

		// Save group leader values
		m_leaderGUID = leader.getGuid();
		m_leaderName = leader.getName();

		// Add the leader as a group member
		auto &newMember = m_members[m_leaderGUID];
		newMember.name = m_leaderName;
		newMember.group = 0;
		newMember.assistant = false;

		// Other checks have already been done in addInvite method, so we are good to go here
		leader.modifyGroupUpdateFlags(group_update_flags::Full, true);

		// Save group
		GroupsById[m_id] = shared_from_this();
		m_database.createGroup(m_id, m_leaderGUID);
	}

	void PlayerGroup::setLootMethod(LootMethod method, UInt64 lootMaster, UInt32 lootTreshold)
	{
		m_lootMethod = method;
		m_lootTreshold = lootTreshold;
		m_lootMaster = lootMaster;
	}

	bool PlayerGroup::isMember(UInt64 guid) const
	{
		auto it = m_members.find(guid);
		return (it != m_members.end());
	}

	void PlayerGroup::setLeader(UInt64 guid)
	{
		// New character has to be a member of this group
		auto it = m_members.find(guid);
		if (it == m_members.end())
		{
			return;
		}

		m_leaderGUID = it->first;
		m_leaderName = it->second.name;

		broadcastPacket(
			std::bind(game::server_write::groupSetLeader, std::placeholders::_1, std::cref(m_leaderName)));

		m_database.setGroupLeader(m_id, m_leaderGUID);
	}

	game::PartyResult PlayerGroup::addMember(GameCharacter &member)
	{
		UInt64 guid = member.getGuid();
		if (!m_invited.contains(guid))
		{
			return game::party_result::NotInYourParty;
		}

		// Remove from invite list
		m_invited.remove(guid);

		// Already full?
		if (isFull())
		{
			return game::party_result::PartyFull;
		}

		// Create new group member
		auto &newMember = m_members[guid];
		newMember.status = 1;
		newMember.name = member.getName();
		newMember.group = 0;
		newMember.assistant = false;
		member.modifyGroupUpdateFlags(group_update_flags::Full, true);

		// Update group list
		sendUpdate();

		// Make sure that all group members know about us
		for (auto &it : m_members)
		{
			auto *player = m_playerManager.getPlayerByCharacterGuid(it.first);
			if (!player)
			{
				// TODO
				continue;
			}

			for (auto &it2 : m_members)
			{
				if (it2.first == it.first)
				{
					continue;
				}

				auto *player2 = m_playerManager.getPlayerByCharacterGuid(it2.first);
				if (!player2)
				{
					// TODO
					continue;
				}

				// Spawn that player for us
				player->sendPacket(
					std::bind(game::server_write::partyMemberStats, std::placeholders::_1, std::cref(*player2->getGameCharacter())));
			}
		}

		// Update database
		m_database.addGroupMember(m_id, guid);
		return game::party_result::Ok;
	}

	game::PartyResult PlayerGroup::addInvite(UInt64 inviteGuid)
	{
		// Can't invite any more members since this group is full already.
		if (isFull())
		{
			return game::party_result::PartyFull;
		}

		m_invited.add(inviteGuid);
		return game::party_result::Ok;
	}

	void PlayerGroup::removeMember(UInt64 guid)
	{
		auto it = m_members.find(guid);
		if (it != m_members.end())
		{
			if (m_members.size() <= 2)
			{
				disband(false);
				return;
			}
			else
			{
				auto *player = m_playerManager.getPlayerByCharacterGuid(guid);
				if (player)
				{
					auto *node = player->getWorldNode();
					if (node)
					{
						player->getGameCharacter()->setGroupId(0);
						node->characterGroupChanged(guid, 0);
					}

					// Send packet
					player->sendPacket(
						std::bind(game::server_write::groupListRemoved, std::placeholders::_1));
					player->setGroup(std::shared_ptr<PlayerGroup>());
				}

				m_members.erase(it);

				if (m_leaderGUID == guid && !m_members.empty())
				{
					auto firstMember = m_members.begin();
					if (firstMember != m_members.end())
					{
						setLeader(firstMember->first);
					}
				}

				sendUpdate();

				// Remove from database
				m_database.removeGroupMember(m_id, guid);
			}
		}
	}

	bool PlayerGroup::removeInvite(UInt64 guid)
	{
		if (!m_invited.contains(guid))
		{
			return false;
		}
		
		m_invited.remove(guid);

		// Check if group is empty
		if (m_members.size() < 2)
		{
			disband(true);
		}

		return true;
	}

	void PlayerGroup::sendUpdate()
	{
		// Update member status
		for (auto &member : m_members)
		{
			auto *player = m_playerManager.getPlayerByCharacterGuid(member.first);
			if (!player)
			{
				member.second.status = game::group_member_status::Offline;
			}
			else
			{
				member.second.status = game::group_member_status::Online;
			}
		}

		// Send to every group member
		for (auto &member : m_members)
		{
			auto *player = m_playerManager.getPlayerByCharacterGuid(member.first);
			if (!player)
			{
				continue;
			}

			// Send packet
			player->sendPacket(
				std::bind(game::server_write::groupList, std::placeholders::_1, 
					member.first,
					m_type,
					false,
					member.second.group,
					member.second.assistant ? 1 : 0,
					0x50000000FFFFFFFELL, 
					std::cref(m_members),
					m_leaderGUID, 
					m_lootMethod,
					m_lootMaster,
					m_lootTreshold,
					0));
		}
	}

	void PlayerGroup::disband(bool silent)
	{
		if (!silent)
		{
			broadcastPacket(
				std::bind(game::server_write::groupDestroyed, std::placeholders::_1));
		}

		auto memberList = m_members;
		for (auto & it : memberList)
		{
			auto *player = m_playerManager.getPlayerByCharacterGuid(it.first);
			if (player)
			{
				auto *node = player->getWorldNode();
				if (node)
				{
					node->characterGroupChanged(it.first, 0);
				}

				// If the group is reset for every player, the group will be deleted!
				player->sendPacket(
					std::bind(game::server_write::groupListRemoved, std::placeholders::_1));
				player->setGroup(std::shared_ptr<PlayerGroup>());
			}
		}

		// Remove from database
		m_database.disbandGroup(m_id);

		// Erase group from the global list of all groups
		auto it = GroupsById.find(m_id);
		if (it != GroupsById.end()) GroupsById.erase(it);
	}

	UInt64 PlayerGroup::getMemberGuid(const String &name)
	{
		for (auto &member : m_members)
		{
			if (member.second.name == name)
			{
				return member.first;
			}
		}

		return 0;
	}

	UInt32 PlayerGroup::instanceBindingForMap(UInt32 map)
	{
		auto it = m_instances.find(map);
		if (it != m_instances.end())
		{
			return it->second;
		}

		return std::numeric_limits<UInt32>::max();
	}

	bool PlayerGroup::addInstanceBinding(UInt32 instance, UInt32 map)
	{
		auto it = m_instances.find(map);
		if (it != m_instances.end())
		{
			return false;
		}

		m_instances[map] = instance;
		return true;
	}

	void PlayerGroup::setTargetIcon(UInt8 target, UInt64 guid)
	{
		if (target >= m_targetIcons.size())
		{
			WLOG("Invalid target icon slot");
			return;
		}

		// Clean other target icons
		if (guid != 0)
		{
			UInt8 slot = 0;
			for (auto &targetGuid : m_targetIcons)
			{
				if (targetGuid == guid)
				{
					setTargetIcon(slot, 0);
				}

				slot++;
			}
		}

		m_targetIcons[target] = guid;
		broadcastPacket(
			std::bind(game::server_write::raidTargetUpdate, std::placeholders::_1, target, guid));
	}

	void PlayerGroup::sendTargetList(Player &player)
	{
		player.sendPacket(
			std::bind(game::server_write::raidTargetUpdateList, std::placeholders::_1, std::cref(m_targetIcons)));
	}

	void PlayerGroup::convertToRaidGroup()
	{
		if (m_type == group_type::Raid)
		{
			return;
		}

		m_type = group_type::Raid;
		sendUpdate();
	}

	void PlayerGroup::setAssistant(UInt64 guid, UInt8 flags)
	{
		auto it = m_members.find(guid);
		if (it != m_members.end())
		{
			it->second.assistant = (flags != 0);
			sendUpdate();
		}
	}

	bool PlayerGroup::isLeaderOrAssistant(UInt64 guid) const
	{
		auto it = m_members.find(guid);
		if (it != m_members.end())
		{
			return (it->first == m_leaderGUID || it->second.assistant);
		}

		return false;
	}

	bool PlayerGroup::addOfflineMember(UInt64 guid)
	{
		// Search for the leading character (Note: We use the database to resolve the character name)
		game::CharEntry charEntry;
		if (!m_database.getCharacterById(guid, charEntry))
		{
			// Could not find the group leader - abort
			ELOG("Could not find character by guid " << guid);
			return false;
		}

		if (guid == m_leaderGUID)
		{
			m_leaderName = charEntry.name;
		}

		// Add group member
		auto &newMember = m_members[guid];
		newMember.name = charEntry.name;
		newMember.group = 0;
		newMember.assistant = false;
		newMember.status = game::group_member_status::Offline;
		return true;
	}

}
