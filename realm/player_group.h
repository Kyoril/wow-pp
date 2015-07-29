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

	namespace group_member_status
	{
		enum Type
		{
			Offline				= 0x0000,
			Online				= 0x0001,
			PvP					= 0x0002,
			Dead				= 0x0004,
			Ghost				= 0x0008,
			Unknown_1			= 0x0040,
			ReferAFriendBuddy	= 0x0100
		};
	}

	typedef group_member_status::Type GroupMemberStatus;

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

	namespace group_update_flags
	{
		enum Type
		{
			None			= 0x00000000,
			Status			= 0x00000001,
			CurrentHP		= 0x00000002,
			MaxHP			= 0x00000004,
			PowerType		= 0x00000008,
			CurrentPower	= 0x00000010,
			MaxPower		= 0x00000020,
			Level			= 0x00000040,
			Zone			= 0x00000080,
			Position		= 0x00000100,
			Auras			= 0x00000200,
			PetGUID			= 0x00000400,
			PetName			= 0x00000800,
			PetModelID		= 0x00001000,
			PetCurrentHP	= 0x00002000,
			PetMaxHP		= 0x00004000,
			PetPowerType	= 0x00008000,
			PetCurrentPower	= 0x00010000,
			PetMaxPower		= 0x00020000,
			PetAuras		= 0x00040000,

			Pet = (PetGUID | PetName | PetModelID | PetCurrentHP | PetMaxHP | PetPowerType | PetCurrentPower | PetMaxPower | PetAuras),
			Full = (Status | CurrentHP | MaxHP | PowerType | CurrentPower | MaxPower | Level | Zone | Position | Auras | Pet)
		};
	}

	typedef group_update_flags::Type GroupUpdateFlags;

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
		void broadcastPacket(F creator)
		{
			for (auto &member : m_members)
			{
				auto *player = m_playerManager.getPlayerByCharacterGuid(member.first);
				if (player)
				{
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
