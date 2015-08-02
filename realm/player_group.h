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
#include <boost/noncopyable.hpp>
#include <boost/signals2.hpp>
#include "player_manager.h"
#include "game_protocol/game_protocol.h"
#include "binary_io/vector_sink.h"
#include "common/linear_set.h"
#include "log/default_log_levels.h"
#include "player.h"
#include <vector>

namespace wowpp
{
	class GameCharacter;

	namespace roll_vote
	{
		enum Type
		{
			Pass			= 0,
			Need			= 1,
			Greed			= 2,

			NotEmitedYet	= 3,
			NotValid		= 4
		};
	}

	typedef roll_vote::Type RollVote;

	namespace group_type
	{
		enum Type
		{
			/// 5-man group.
			Normal			= 0,
			/// More than 5 man allowed, but no quest progression allowed.
			Raid			= 1
		};
	}

	typedef group_type::Type GroupType;

	namespace loot_method
	{
		enum Type
		{
			/// Everyone in the group can loot every item.
			FreeForAll		= 0,
			/// Loot rights cycles through group members.
			RoundRobin		= 1,
			/// There is a loot master who can assign loot to group members.
			MasterLoot		= 2,
			/// Round-robin for normal items, rolling for special ones. This is the default loot method in new groups.
			GroupLoot		= 3,
			/// Round-robin for normal items, selective rolling for special ones.
			NeedBeforeGreed	= 4
		};
	}

	typedef loot_method::Type LootMethod;

	/// Represents a group of players. In a group, experience points and loot will be shared.
	/// Group members are also able to see each others positions across different worlds, and are
	/// able to enter the same dungeon instances.
	class PlayerGroup final : public std::enable_shared_from_this<PlayerGroup>
	{
	public:

		typedef std::map<UInt64, game::GroupMemberSlot> MembersByGUID;
		typedef LinearSet<UInt64> InvitedMembers;

	public:

		/// Creates a new instance of a player group. Note that a group has to be
		/// created using the create method before it will be valid.
		explicit PlayerGroup(PlayerManager &playerManager);

		/// Creates the group and setup a leader.
		void create(GameCharacter &leader);
		/// Changes the loot method.
		void setLootMethod(LootMethod method);
		/// Sets a new group leader. The new leader has to be a member of this group.
		void setLeader(GameCharacter &newLeader);
		/// Adds a guid to the list of pending invites.
		game::PartyResult addInvite(UInt64 inviteGuid);
		/// Adds a new member to the group. The group member has to be invited first.
		game::PartyResult addMember(GameCharacter &member);
		/// Sends a group update message to all members of the group.
		void sendUpdate();

		/// Checks if the specified game character is a member of this group.
		bool isMember(GameCharacter &character) const;
		/// Returns the number of group members.
		size_t getMemberCount() const { return m_members.size(); }
		/// Determines whether this group has been created.
		bool isCreated() const { return m_leaderGUID != 0; }
		/// Determines whether this group is full already.
		bool isFull() const { return (m_type == group_type::Normal ? m_members.size() >= 5 : m_members.size() >= 40); }
		/// Gets the groups loot method.
		LootMethod getLootMethod() const { return m_lootMethod; }
		/// Gets the group leaders GUID.
		UInt64 getLeader() const { return m_leaderGUID; }

		template<class F>
		void broadcastPacket(F creator, UInt64 except = 0)
		{
			for (auto &member : m_members)
			{
				if (member.first == except)
					continue;

				auto *player = m_playerManager.getPlayerByCharacterGuid(member.first);
				if (player)
				{
					DLOG("SMSG_PARTY_MEMBER_STATS");

					player->sendPacket(creator);
				}
			}
		}

	private:

		PlayerManager &m_playerManager;
		UInt64 m_leaderGUID;
		String m_leaderName;
		GroupType m_type;
		MembersByGUID m_members;	// This is the map of actual party members.
		InvitedMembers m_invited;	// This is a list of pending member invites
		LootMethod m_lootMethod;
	};
}
