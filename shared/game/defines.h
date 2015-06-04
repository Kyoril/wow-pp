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

namespace wowpp
{
	namespace game
	{
		namespace gender
		{
			enum Type
			{
				Male		= 0x00,
				Female		= 0x01,
				None		= 0x02,

				Max			= 0x02
			};
		}

		typedef gender::Type Gender;


		namespace race
		{
			enum Type
			{
				Human		= 0x01,
				Orc			= 0x02,
				Dwarf		= 0x03,
				NightElf	= 0x04,
				Undead		= 0x05,
				Tauren		= 0x06,
				Gnome		= 0x07,
				Troll		= 0x08,
				Goblin		= 0x09,
				BloodElf	= 0x0A,
				Draenei		= 0x0B,

				AllPlayable = ((1 << (Human - 1)) | (1 << (Orc - 1)) | (1 << (Dwarf - 1)) | (1 << (NightElf - 1)) | (1 << (Undead - 1)) | (1 << (Tauren - 1)) | (1 << (Gnome - 1)) | (1 << (Troll - 1)) | (1 << (BloodElf - 1)) | (1 << (Draenei - 1))),
				Alliance = ((1 << (Human - 1)) | (1 << (Dwarf - 1)) | (1 << (NightElf - 1)) | (1 << (Gnome - 1)) | (1 << (Draenei - 1))),
				Horde = ((1 << (Orc - 1)) | (1 << (Undead - 1)) | (1 << (Tauren - 1)) | (1 << (Troll - 1)) | (1 << (BloodElf - 1))),

				Max			= 0x0B
			};
		}

		typedef race::Type Race;


		namespace char_class
		{
			enum Type
			{
				Warrior		= 0x01,
				Paladin		= 0x02,
				Hunter		= 0x03,
				Rogue		= 0x04,
				Priest		= 0x05,
				Shaman		= 0x07,
				Mage		= 0x08,
				Warlock		= 0x09,
				Druid		= 0x0B,

				AllPlayable = ((1 << (Warrior - 1)) | (1 << (Paladin - 1)) | (1 << (Hunter - 1)) | (1 << (Rogue - 1)) | (1 << (Priest - 1)) | (1 << (Shaman - 1)) | (1 << (Mage - 1)) | (1 << (Warlock - 1)) | (1 << (Druid - 1))),
				AllCreature = ((1 << (Warrior - 1)) | (1 << (Paladin - 1)) | (1 << (Mage - 1))),
				WandUsers	= ((1 << (Priest - 1)) | (1 << (Mage - 1)) | (1 << (Warlock - 1))),

				Max			= 0x0C
			};
		}

		typedef char_class::Type CharClass;

		namespace chat_msg
		{
			enum Type
			{
				Addon				= 0xFFFFFFFF,
				System				= 0x00,
				Say					= 0x01,
				Party				= 0x02,
				Raid				= 0x03,
				Guild				= 0x04,
				Officer				= 0x05,
				Yell				= 0x06,
				Whisper				= 0x07,
				WhisperInform		= 0x08,
				Reply				= 0x09,
				Emote				= 0x0A,
				TextEmote			= 0x0B,
				MonsterSay			= 0x0C,
				MonsterParty		= 0x0D,
				MonsterYell			= 0x0E,
				MonsterWhisper		= 0x0F,
				MonsterEmote		= 0x10,
				Channel				= 0x11,
				ChannelJoin			= 0x12,
				ChannelLeave		= 0x13,
				ChannelList			= 0x14,
				ChannelNotice		= 0x15,
				ChannelNoticeUser	= 0x16,
				Afk					= 0x17,
				Dnd					= 0x18,
				Ignored				= 0x19,
				Skill				= 0x1A,
				Loot				= 0x1B,
				Money				= 0x1C,
				Opening				= 0x1D,
				TradeSkill			= 0x1E,
				PetInfo				= 0x1F,
				CombatMiscInfo		= 0x20,
				CombatXPGain		= 0x21,
				CombatHonorGain		= 0x22,
				CombatFactionChange	= 0x23,
				BGSystemNeutral		= 0x24,
				BGSystemAlliance	= 0x25,
				BGSystemHorde		= 0x26,
				RaidLeader			= 0x27,
				RaidWarning			= 0x28,
				RaidBossWhisper		= 0x29,
				RaidBossEmote		= 0x2A,
				Filtered			= 0x2B,
				Battleground		= 0x2C,
				BattlegroundLeader	= 0x2D,
				Restricted			= 0x2E
			};
		}

		typedef chat_msg::Type ChatMsg;

		namespace language
		{
			enum Type
			{
				Universal			= 0x00,
				Orcish				= 0x01,
				Darnassian			= 0x02,
				Taurahe				= 0x03,
				Dwarvish			= 0x04,
				Common				= 0x07,
				Demonic				= 0x08,
				Titan				= 0x09,
				Thalassian			= 0x0A,
				Draconic			= 0x0B,
				Kalimag				= 0x0C,
				Gnomish				= 0x0D,
				Troll				= 0x0E,
				GutterSpeak			= 0x21,
				Draenei				= 0x23,
				Zombie				= 0x24,
				GnomishBinary		= 0x25,
				GoblinBinary		= 0x26,
				Addon				= 0xFFFFFFFF,

				Count_				= 0x13
			};
		}

		typedef language::Type Language;
	}
}
