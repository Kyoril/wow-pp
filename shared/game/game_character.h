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

#include "game_unit.h"
#include "game_item.h"
#include "inventory.h"
#include "defines.h"
#include "loot_instance.h"

namespace wowpp
{
	namespace character_fields
	{
		enum Enum
		{
			DuelArbiter					= 0x00 + unit_fields::UnitFieldCount,
			CharacterFlags				= 0x02 + unit_fields::UnitFieldCount,
			GuildId						= 0x03 + unit_fields::UnitFieldCount,
			GuildRank					= 0x04 + unit_fields::UnitFieldCount,
			CharacterBytes				= 0x05 + unit_fields::UnitFieldCount,
			CharacterBytes2				= 0x06 + unit_fields::UnitFieldCount,
			CharacterBytes3				= 0x07 + unit_fields::UnitFieldCount,
			DuelTeam					= 0x08 + unit_fields::UnitFieldCount,
			GuildTimestamp				= 0x09 + unit_fields::UnitFieldCount,
			QuestLog1_1					= 0x0A + unit_fields::UnitFieldCount,
			QuestLog1_2					= 0x0B + unit_fields::UnitFieldCount,
			QuestLog1_3					= 0x0C + unit_fields::UnitFieldCount,
			QuestLog1_4					= 0x0D + unit_fields::UnitFieldCount,
			QuestLog2_1					= 0x0E + unit_fields::UnitFieldCount,
			QuestLog2_2					= 0x0F + unit_fields::UnitFieldCount,
			QuestLog2_3					= 0x10 + unit_fields::UnitFieldCount,
			QuestLog2_4					= 0x11 + unit_fields::UnitFieldCount,
			QuestLog3_1					= 0x12 + unit_fields::UnitFieldCount,
			QuestLog3_2					= 0x13 + unit_fields::UnitFieldCount,
			QuestLog3_3					= 0x14 + unit_fields::UnitFieldCount,
			QuestLog3_4					= 0x15 + unit_fields::UnitFieldCount,
			QuestLog4_1					= 0x16 + unit_fields::UnitFieldCount,
			QuestLog4_2					= 0x17 + unit_fields::UnitFieldCount,
			QuestLog4_3					= 0x18 + unit_fields::UnitFieldCount,
			QuestLog4_4					= 0x19 + unit_fields::UnitFieldCount,
			QuestLog5_1					= 0x1A + unit_fields::UnitFieldCount,
			QuestLog5_2					= 0x1B + unit_fields::UnitFieldCount,
			QuestLog5_3					= 0x1C + unit_fields::UnitFieldCount,
			QuestLog5_4					= 0x1D + unit_fields::UnitFieldCount,
			QuestLog6_1					= 0x1E + unit_fields::UnitFieldCount,
			QuestLog6_2					= 0x1F + unit_fields::UnitFieldCount,
			QuestLog6_3					= 0x20 + unit_fields::UnitFieldCount,
			QuestLog6_4					= 0x21 + unit_fields::UnitFieldCount,
			QuestLog7_1					= 0x22 + unit_fields::UnitFieldCount,
			QuestLog7_2					= 0x23 + unit_fields::UnitFieldCount,
			QuestLog7_3					= 0x24 + unit_fields::UnitFieldCount,
			QuestLog7_4					= 0x25 + unit_fields::UnitFieldCount,
			QuestLog8_1					= 0x26 + unit_fields::UnitFieldCount,
			QuestLog8_2					= 0x27 + unit_fields::UnitFieldCount,
			QuestLog8_3					= 0x28 + unit_fields::UnitFieldCount,
			QuestLog8_4					= 0x29 + unit_fields::UnitFieldCount,
			QuestLog9_1					= 0x2A + unit_fields::UnitFieldCount,
			QuestLog9_2					= 0x2B + unit_fields::UnitFieldCount,
			QuestLog9_3					= 0x2C + unit_fields::UnitFieldCount,
			QuestLog9_4					= 0x2D + unit_fields::UnitFieldCount,
			QuestLog10_1				= 0x2E + unit_fields::UnitFieldCount,
			QuestLog10_2				= 0x2F + unit_fields::UnitFieldCount,
			QuestLog10_3				= 0x30 + unit_fields::UnitFieldCount,
			QuestLog10_4				= 0x31 + unit_fields::UnitFieldCount,
			QuestLog11_1				= 0x32 + unit_fields::UnitFieldCount,
			QuestLog11_2				= 0x33 + unit_fields::UnitFieldCount,
			QuestLog11_3				= 0x34 + unit_fields::UnitFieldCount,
			QuestLog11_4				= 0x35 + unit_fields::UnitFieldCount,
			QuestLog12_1				= 0x36 + unit_fields::UnitFieldCount,
			QuestLog12_2				= 0x37 + unit_fields::UnitFieldCount,
			QuestLog12_3				= 0x38 + unit_fields::UnitFieldCount,
			QuestLog12_4				= 0x39 + unit_fields::UnitFieldCount,
			QuestLog13_1				= 0x3A + unit_fields::UnitFieldCount,
			QuestLog13_2				= 0x3B + unit_fields::UnitFieldCount,
			QuestLog13_3				= 0x3C + unit_fields::UnitFieldCount,
			QuestLog13_4				= 0x3D + unit_fields::UnitFieldCount,
			QuestLog14_1				= 0x3E + unit_fields::UnitFieldCount,
			QuestLog14_2				= 0x3F + unit_fields::UnitFieldCount,
			QuestLog14_3				= 0x40 + unit_fields::UnitFieldCount,
			QuestLog14_4				= 0x41 + unit_fields::UnitFieldCount,
			QuestLog15_1				= 0x42 + unit_fields::UnitFieldCount,
			QuestLog15_2				= 0x43 + unit_fields::UnitFieldCount,
			QuestLog15_3				= 0x44 + unit_fields::UnitFieldCount,
			QuestLog15_4				= 0x45 + unit_fields::UnitFieldCount,
			QuestLog16_1				= 0x46 + unit_fields::UnitFieldCount,
			QuestLog16_2				= 0x47 + unit_fields::UnitFieldCount,
			QuestLog16_3				= 0x48 + unit_fields::UnitFieldCount,
			QuestLog16_4				= 0x49 + unit_fields::UnitFieldCount,
			QuestLog17_1				= 0x4A + unit_fields::UnitFieldCount,
			QuestLog17_2				= 0x4B + unit_fields::UnitFieldCount,
			QuestLog17_3				= 0x4C + unit_fields::UnitFieldCount,
			QuestLog17_4				= 0x4D + unit_fields::UnitFieldCount,
			QuestLog18_1				= 0x4E + unit_fields::UnitFieldCount,
			QuestLog18_2				= 0x4F + unit_fields::UnitFieldCount,
			QuestLog18_3				= 0x50 + unit_fields::UnitFieldCount,
			QuestLog18_4				= 0x51 + unit_fields::UnitFieldCount,
			QuestLog19_1				= 0x52 + unit_fields::UnitFieldCount,
			QuestLog19_2				= 0x53 + unit_fields::UnitFieldCount,
			QuestLog19_3				= 0x54 + unit_fields::UnitFieldCount,
			QuestLog19_4				= 0x55 + unit_fields::UnitFieldCount,
			QuestLog20_1				= 0x56 + unit_fields::UnitFieldCount,
			QuestLog20_2				= 0x57 + unit_fields::UnitFieldCount,
			QuestLog20_3				= 0x58 + unit_fields::UnitFieldCount,
			QuestLog20_4				= 0x59 + unit_fields::UnitFieldCount,
			QuestLog21_1				= 0x5A + unit_fields::UnitFieldCount,
			QuestLog21_2				= 0x5B + unit_fields::UnitFieldCount,
			QuestLog21_3				= 0x5C + unit_fields::UnitFieldCount,
			QuestLog21_4				= 0x5D + unit_fields::UnitFieldCount,
			QuestLog22_1				= 0x5E + unit_fields::UnitFieldCount,
			QuestLog22_2				= 0x5F + unit_fields::UnitFieldCount,
			QuestLog22_3				= 0x60 + unit_fields::UnitFieldCount,
			QuestLog22_4				= 0x61 + unit_fields::UnitFieldCount,
			QuestLog23_1				= 0x62 + unit_fields::UnitFieldCount,
			QuestLog23_2				= 0x63 + unit_fields::UnitFieldCount,
			QuestLog23_3				= 0x64 + unit_fields::UnitFieldCount,
			QuestLog23_4				= 0x65 + unit_fields::UnitFieldCount,
			QuestLog24_1				= 0x66 + unit_fields::UnitFieldCount,
			QuestLog24_2				= 0x67 + unit_fields::UnitFieldCount,
			QuestLog24_3				= 0x68 + unit_fields::UnitFieldCount,
			QuestLog24_4				= 0x69 + unit_fields::UnitFieldCount,
			QuestLog25_1				= 0x6A + unit_fields::UnitFieldCount,
			QuestLog25_2				= 0x6B + unit_fields::UnitFieldCount,
			QuestLog25_3				= 0x6C + unit_fields::UnitFieldCount,
			QuestLog25_4				= 0x6D + unit_fields::UnitFieldCount,
			VisibleItem1_CREATOR		= 0x6E + unit_fields::UnitFieldCount,
			VisibleItem1_0				= 0x70 + unit_fields::UnitFieldCount,
			VisibleItem1_PROPERTIES		= 0x7C + unit_fields::UnitFieldCount,
			VisibleItem1_PAD			= 0x7D + unit_fields::UnitFieldCount,
			VisibleItem2_CREATOR		= 0x7E + unit_fields::UnitFieldCount,
			VisibleItem2_0				= 0x80 + unit_fields::UnitFieldCount,
			VisibleItem2_PROPERTIES		= 0x8C + unit_fields::UnitFieldCount,
			VisibleItem2_PAD			= 0x8D + unit_fields::UnitFieldCount,
			VisibleItem3_CREATOR		= 0x8E + unit_fields::UnitFieldCount,
			VisibleItem3_0				= 0x90 + unit_fields::UnitFieldCount,
			VisibleItem3_PROPERTIES		= 0x9C + unit_fields::UnitFieldCount,
			VisibleItem3_PAD			= 0x9D + unit_fields::UnitFieldCount,
			VisibleItem4_CREATOR		= 0x9E + unit_fields::UnitFieldCount,
			VisibleItem4_0				= 0xA0 + unit_fields::UnitFieldCount,
			VisibleItem4_PROPERTIES		= 0xAC + unit_fields::UnitFieldCount,
			VisibleItem4_PAD			= 0xAD + unit_fields::UnitFieldCount,
			VisibleItem5_CREATOR		= 0xAE + unit_fields::UnitFieldCount,
			VisibleItem5_0				= 0xB0 + unit_fields::UnitFieldCount,
			VisibleItem5_PROPERTIES		= 0xBC + unit_fields::UnitFieldCount,
			VisibleItem5_PAD			= 0xBD + unit_fields::UnitFieldCount,
			VisibleItem6_CREATOR		= 0xBE + unit_fields::UnitFieldCount,
			VisibleItem6_0				= 0xD0 + unit_fields::UnitFieldCount,
			VisibleItem6_PROPERTIES		= 0xDC + unit_fields::UnitFieldCount,
			VisibleItem6_PAD			= 0xDD + unit_fields::UnitFieldCount,
			VisibleItem7_CREATOR		= 0xCE + unit_fields::UnitFieldCount,
			VisibleItem7_0				= 0xD0 + unit_fields::UnitFieldCount,
			VisibleItem7_PROPERTIES		= 0xDC + unit_fields::UnitFieldCount,
			VisibleItem7_PAD			= 0xDD + unit_fields::UnitFieldCount,
			VisibleItem8_CREATOR		= 0xDE + unit_fields::UnitFieldCount,
			VisibleItem8_0				= 0xE0 + unit_fields::UnitFieldCount,
			VisibleItem8_PROPERTIES		= 0xEC + unit_fields::UnitFieldCount,
			VisibleItem8_PAD			= 0xED + unit_fields::UnitFieldCount,
			VisibleItem9_CREATOR		= 0xEE + unit_fields::UnitFieldCount,
			VisibleItem9_0				= 0xF0 + unit_fields::UnitFieldCount,
			VisibleItem9_PROPERTIES		= 0xFC + unit_fields::UnitFieldCount,
			VisibleItem9_PAD			= 0xFD + unit_fields::UnitFieldCount,
			VisibleItem10_CREATOR		= 0xFE + unit_fields::UnitFieldCount,
			VisibleItem10_0				= 0x100 + unit_fields::UnitFieldCount,
			VisibleItem10_PROPERTIES	= 0x10C + unit_fields::UnitFieldCount,
			VisibleItem10_PAD			= 0x10D + unit_fields::UnitFieldCount,
			VisibleItem11_CREATOR		= 0x10E + unit_fields::UnitFieldCount,
			VisibleItem11_0				= 0x110 + unit_fields::UnitFieldCount,
			VisibleItem11_PROPERTIES	= 0x11C + unit_fields::UnitFieldCount,
			VisibleItem11_PAD			= 0x11D + unit_fields::UnitFieldCount,
			VisibleItem12_CREATOR		= 0x11E + unit_fields::UnitFieldCount,
			VisibleItem12_0				= 0x120 + unit_fields::UnitFieldCount,
			VisibleItem12_PROPERTIES	= 0x12C + unit_fields::UnitFieldCount,
			VisibleItem12_PAD			= 0x12D + unit_fields::UnitFieldCount,
			VisibleItem13_CREATOR		= 0x12E + unit_fields::UnitFieldCount,
			VisibleItem13_0				= 0x130 + unit_fields::UnitFieldCount,
			VisibleItem13_PROPERTIES	= 0x13C + unit_fields::UnitFieldCount,
			VisibleItem13_PAD			= 0x13D + unit_fields::UnitFieldCount,
			VisibleItem14_CREATOR		= 0x13E + unit_fields::UnitFieldCount,
			VisibleItem14_0				= 0x140 + unit_fields::UnitFieldCount,
			VisibleItem14_PROPERTIES	= 0x14C + unit_fields::UnitFieldCount,
			VisibleItem14_PAD			= 0x14D + unit_fields::UnitFieldCount,
			VisibleItem15_CREATOR		= 0x14E + unit_fields::UnitFieldCount,
			VisibleItem15_0				= 0x150 + unit_fields::UnitFieldCount,
			VisibleItem15_PROPERTIES	= 0x15C + unit_fields::UnitFieldCount,
			VisibleItem15_PAD			= 0x15D + unit_fields::UnitFieldCount,
			VisibleItem16_CREATOR		= 0x15E + unit_fields::UnitFieldCount,
			VisibleItem16_0				= 0x160 + unit_fields::UnitFieldCount,
			VisibleItem16_PROPERTIES	= 0x16C + unit_fields::UnitFieldCount,
			VisibleItem16_PAD			= 0x16D + unit_fields::UnitFieldCount,
			VisibleItem17_CREATOR		= 0x16E + unit_fields::UnitFieldCount,
			VisibleItem17_0				= 0x170 + unit_fields::UnitFieldCount,
			VisibleItem17_PROPERTIES	= 0x17C + unit_fields::UnitFieldCount,
			VisibleItem17_PAD			= 0x17D + unit_fields::UnitFieldCount,
			VisibleItem18_CREATOR		= 0x17E + unit_fields::UnitFieldCount,
			VisibleItem18_0				= 0x180 + unit_fields::UnitFieldCount,
			VisibleItem18_PROPERTIES	= 0x18C + unit_fields::UnitFieldCount,
			VisibleItem18_PAD			= 0x18D + unit_fields::UnitFieldCount,
			VisibleItem19_CREATOR		= 0x18E + unit_fields::UnitFieldCount,
			VisibleItem19_0				= 0x190 + unit_fields::UnitFieldCount,
			VisibleItem19_PROPERTIES	= 0x19C + unit_fields::UnitFieldCount,
			VisibleItem19_PAD			= 0x19D + unit_fields::UnitFieldCount,
			ChosenTitle					= 0x19E + unit_fields::UnitFieldCount,
			Pad0						= 0x19F + unit_fields::UnitFieldCount,
			InvSlotHead					= 0x1A0 + unit_fields::UnitFieldCount,
			PackSlot_1					= 0x1CE + unit_fields::UnitFieldCount,
			BankSlot_1					= 0x1EE + unit_fields::UnitFieldCount,
			BankBagSlot_1				= 0x226 + unit_fields::UnitFieldCount,
			VendorBuybackSlot_1			= 0x234 + unit_fields::UnitFieldCount,
			KeyringSlot_1				= 0x24C + unit_fields::UnitFieldCount,
			VanitypetSlot_1				= 0x28C + unit_fields::UnitFieldCount,
			Farsight					= 0x2B0 + unit_fields::UnitFieldCount,
			KnownTitles					= 0x2B2 + unit_fields::UnitFieldCount,
			Xp							= 0x2B4 + unit_fields::UnitFieldCount,
			NextLevelXp					= 0x2B5 + unit_fields::UnitFieldCount,
			SkillInfo1_1				= 0x2B6 + unit_fields::UnitFieldCount,
			CharacterPoints_1			= 0x436 + unit_fields::UnitFieldCount,
			CharacterPoints_2			= 0x437 + unit_fields::UnitFieldCount,
			Track_Creatures				= 0x438 + unit_fields::UnitFieldCount,
			Track_Resources				= 0x439 + unit_fields::UnitFieldCount,
			BlockPercentage				= 0x43A + unit_fields::UnitFieldCount,
			DodgePercentage				= 0x43B + unit_fields::UnitFieldCount,
			ParryPercentage				= 0x43C + unit_fields::UnitFieldCount,
			Expertise					= 0x43D + unit_fields::UnitFieldCount,
			OffHandExpertise			= 0x43E + unit_fields::UnitFieldCount,
			CritPercentage				= 0x43F + unit_fields::UnitFieldCount,
			RangedCritPercentage		= 0x440 + unit_fields::UnitFieldCount,
			OffHandCritPercentage		= 0x441 + unit_fields::UnitFieldCount,
			SpellCritPercentage			= 0x442 + unit_fields::UnitFieldCount,
			ShieldBlock					= 0x449 + unit_fields::UnitFieldCount,
			ExploredZones_1				= 0x44A + unit_fields::UnitFieldCount,
			RestStateExperience			= 0x4CA + unit_fields::UnitFieldCount,
			Coinage						= 0x4CB + unit_fields::UnitFieldCount,
			ModDamageDonePos			= 0x4CC + unit_fields::UnitFieldCount,
			ModDamageDoneNeg			= 0x4D3 + unit_fields::UnitFieldCount,
			ModDamageDonePct			= 0x4DA + unit_fields::UnitFieldCount,
			ModHealingDonePos			= 0x4E1 + unit_fields::UnitFieldCount,
			ModTargetResistance			= 0x4E2 + unit_fields::UnitFieldCount,
			ModTargetPhysicalResistance	= 0x4E3 + unit_fields::UnitFieldCount,
			FieldBytes					= 0x4E4 + unit_fields::UnitFieldCount,
			AmmoId						= 0x4E5 + unit_fields::UnitFieldCount,
			SelfResSpell				= 0x4E6 + unit_fields::UnitFieldCount,
			PvPMedals					= 0x4E7 + unit_fields::UnitFieldCount,
			BuybackPrice_1				= 0x4E8 + unit_fields::UnitFieldCount,
			BuybackTimestamp_1			= 0x4F4 + unit_fields::UnitFieldCount,
			Kills						= 0x500 + unit_fields::UnitFieldCount,
			TodayContribution			= 0x501 + unit_fields::UnitFieldCount,
			YesterdayContribution		= 0x502 + unit_fields::UnitFieldCount,
			LifetimeHonrableKills		= 0x503 + unit_fields::UnitFieldCount,
			FieldBytes2					= 0x504 + unit_fields::UnitFieldCount,
			WatchedFactionIndex			= 0x505 + unit_fields::UnitFieldCount,
			CombatRating_1				= 0x506 + unit_fields::UnitFieldCount,
			ArenaTeamInfo_1_1			= 0x51E + unit_fields::UnitFieldCount,
			HonorCurrency				= 0x530 + unit_fields::UnitFieldCount,
			ArenaCurrency				= 0x531 + unit_fields::UnitFieldCount,
			ModManaRegen				= 0x532 + unit_fields::UnitFieldCount,
			ModManaRegenInterrupt		= 0x533 + unit_fields::UnitFieldCount,
			MaxLevel					= 0x534 + unit_fields::UnitFieldCount,
			DailyQuest_1				= 0x535 + unit_fields::UnitFieldCount,

			CharacterFieldCount			= 0x54E + unit_fields::UnitFieldCount
		};
	}

	namespace player_item_slots
	{
		enum Enum
		{
			Start		= 0,
			End			= 118,

			Count_		= (End - Start)
		};
	};

	typedef player_item_slots::Enum PlayerItemSlots;

	namespace player_equipment_slots
	{
		enum Enum
		{
			Start			= 0,

			Head			= 0,
			Neck			= 1,
			Shoulders		= 2,
			Body			= 3,
			Chest			= 4,
			Waist			= 5,
			Legs			= 6,
			Feet			= 7,
			Wrists			= 8,
			Hands			= 9,
			Finger1			= 10,
			Finger2			= 11,
			Trinket1		= 12,
			Trinket2		= 13,
			Back			= 14,
			Mainhand		= 15,
			Offhand			= 16,
			Ranged			= 17,
			Tabard			= 18,

			End				= 19,
			Count_			= (End - Start)
		};
	}

	typedef player_equipment_slots::Enum PlayerEquipmentSlots;

	namespace player_inventory_slots
	{
		enum Enum
		{
			Bag_0			= 255,
			Start			= 19,
			End				= 23,

			Count_			= (End - Start)
		};
	}

	typedef player_inventory_slots::Enum PlayerInventorySlots;

	namespace player_inventory_pack_slots
	{
		enum Enum
		{
			Start			= 23,
			End				= 39,

			Count_			= (End - Start)
		};
	}

	typedef player_inventory_pack_slots::Enum PlayerInventoryPackSlots;

	namespace player_bank_item_slots
	{
		enum Enum
		{
			Start			= 39,
			End				= 67,

			Count_			= (End - Start)
		};
	}

	typedef player_bank_item_slots::Enum PlayerBankItemSlots;

	namespace player_bank_bag_slots
	{
		enum Enum
		{
			Start			= 67,
			End				= 74,

			Count_			= (End - Start)
		};
	}

	typedef player_bank_bag_slots::Enum PlayerBankBagSlots;

	namespace player_buy_back_slots
	{
		enum Enum
		{
			Start			= 74,
			End				= 86,

			Count_			= (End - Start)
		};
	}

	typedef player_buy_back_slots::Enum PlayerBuyBackSlots;

	namespace player_key_ring_slots
	{
		enum Enum
		{
			Start			= 86,
			End				= 118,

			Count_			= (End - Start)
		};
	}

	typedef player_key_ring_slots::Enum PlayerKeyRingSlots;

	namespace group_update_flags
	{
		enum Type
		{
			None = 0x00000000,
			Status = 0x00000001,
			CurrentHP = 0x00000002,
			MaxHP = 0x00000004,
			PowerType = 0x00000008,
			CurrentPower = 0x00000010,
			MaxPower = 0x00000020,
			Level = 0x00000040,
			Zone = 0x00000080,
			Position = 0x00000100,
			Auras = 0x00000200,
			PetGUID = 0x00000400,
			PetName = 0x00000800,
			PetModelID = 0x00001000,
			PetCurrentHP = 0x00002000,
			PetMaxHP = 0x00004000,
			PetPowerType = 0x00008000,
			PetCurrentPower = 0x00010000,
			PetMaxPower = 0x00020000,
			PetAuras = 0x00040000,

			Pet = (PetGUID | PetName | PetModelID | PetCurrentHP | PetMaxHP | PetPowerType | PetCurrentPower | PetMaxPower | PetAuras),
			Full = (Status | CurrentHP | MaxHP | PowerType | CurrentPower | MaxPower | Level | Zone | Position | Auras | Pet)
		};
	}

	struct FactionState
	{
		UInt32 id;
		UInt32 listId;
		UInt32 flags;
		Int32 standing;
		bool changed;
	};

	typedef std::map<UInt32, FactionState> FactionStateList;
	typedef std::map<UInt32, game::ReputationRank> ForcedReactions;

	typedef group_update_flags::Type GroupUpdateFlags;

	namespace proto
	{
		class SpellEntry;
		class ItemEntry;
		class SkillEntry;
		class QuestEntry;
	}

	struct ItemPosCount final
	{
		UInt16 position;
		UInt8 count;

		explicit ItemPosCount(UInt16 position, UInt8 count)
			: position(position)
			, count(count)
		{
		}
	};

	typedef std::vector<ItemPosCount> ItemPosCountVector;

	struct QuestStatusData
	{
		game::QuestStatus status;
		// May be 0 if completed.
		GameTime expiration;
		// What is this for?
		bool explored;
		std::array<UInt16, 4> creatures;
		std::array<UInt16, 4> objects;
		// Recomputed on inventory changes.
		std::array<UInt16, 4> items;

		QuestStatusData()
			: status(game::QuestStatus::Available)
			, expiration(0)
			, explored(false)
		{
			creatures.fill(0);
			objects.fill(0);
			items.fill(0);
		}
	};

	class GameCreature;
	class WorldObject;

	namespace spell_mod_op
	{
		enum Type
		{
			/// Spell damage modified.
			Damage				= 0,
			/// Spell aura duration modified.
			Duration			= 1,
			/// Spell threat modified.
			Threat				= 2,
			/// Effect1 base points modified.
			Effect1				= 3,
			/// Spell charges modified.
			Charges				= 4,
			/// Spell range modified.
			Range				= 5,
			/// Spell radius modified.
			Radius				= 6,
			/// Spell critical hit chance modified.
			CritChance			= 7,
			/// All effect base points modified.
			AllEffects			= 8,
			/// Amount of spell delay on hit while casting modified.
			PreventSpellDelay	= 9,
			/// Spell cast time modified (also cast time for channeled spells)
			CastTime			= 10,
			/// Spell cooldown modified.
			Cooldown			= 11,
			/// Effect2 base points modified.
			Effect2				= 12,
			/// Spell cost modified.
			Cost				= 14,
			/// Critical spell damage modified.
			CritDamageBonus		= 15,
			/// Chance to miss or resist this spell modified.
			ResistMissChance	= 16,
			/// Number of targets to jump to modified (chain heal etc.)
			JumpTargets			= 17,
			/// Increases the proc chance (used in resto druid talent "Improved Nature's Grasp" for example)
			ChanceOfSuccess		= 18,
			/// ...
			ActivationTime		= 19,
			/// ...
			EffectPastFirst		= 20,
			/// Global cooldown modified.
			GlobalCooldown		= 21,
			/// Periodic damage of aura modified.
			Dot					= 22,
			/// Effect3 base points modified.
			Effect3				= 23,
			/// Bonus damage modified (? we already have Damage...).
			BonusDamage			= 24,
			/// Multiple value field modified.
			MultipleValue		= 27,
			/// Resist dispel chance modified.
			ResistDispelChance	= 28,

			/// Since this is a bitmask, this marks the maximum number of spellmods so far
			Max_				= 32
		};
	}

	typedef spell_mod_op::Type SpellModOp;

	namespace spell_mod_type
	{
		enum Type
		{
			/// Equals aura_type::AddFlatModifier
			Flat		= 107,
			/// Equals aura_type::AddPctModifier
			Pct			= 108
		};
	}

	typedef spell_mod_type::Type SpellModType;

	/// Represents a spell modifier which is used to modify spells for a GameCharacter.
	/// This is only(?) used by talents, and is thus only available for characters.
	struct SpellModifier final
	{
		/// The spell modifier operation (what should be changed?)
		SpellModOp op;
		/// The modifier type (flag or percentage)
		SpellModType type;
		/// Charge count of this modifier (some are like "Increases damage of the next N casts")
		Int16 charges;
		/// The modifier value.
		Int32 value;
		/// Mask to determine which spells are modified.
		UInt64 mask;
		/// Affected spell index. (?)
		UInt32 spellId;
		/// Index of the affected spell index. (?)
		UInt8 effectId;
	};

	/// Contains a list of spell modifiers of a character.
	typedef std::list<SpellModifier> SpellModList;
	/// Stores spell modifiers by it's operation.
	typedef std::map<SpellModOp, SpellModList> SpellModsByOp;

	/// Represents a players character in the world.
	class GameCharacter : public GameUnit
	{
		// Serialization and deserialization for the wow++ protocol

		friend io::Writer &operator << (io::Writer &w, GameCharacter const &object);
		friend io::Reader &operator >> (io::Reader &r, GameCharacter &object);

	public:

		// Signals

		/// Fired when a proficiency was changes (weapon & armor prof.)
		boost::signals2::signal<void(Int32, UInt32)> proficiencyChanged;
		/// Fired when an inventory error occurred. Used to send a packet to the owning players client.
		boost::signals2::signal<void(game::InventoryChangeFailure, GameItem *, GameItem *)> inventoryChangeFailure;
		/// Fired when the characters combo points changes. Used to send a packet to the owning players client.
		boost::signals2::signal<void()> comboPointsChanged;
		/// Fired when the character gained some experience points. Used to send a packet to the owning players client.
		boost::signals2::signal<void(UInt64, UInt32, UInt32)> experienceGained;
		/// Fired when the characters home changed. Used to send a packet to the owning players client.
		boost::signals2::signal<void()> homeChanged;
		/// Fired when a quest status changed. Used to save quest status at the realm.
		boost::signals2::signal<void(UInt32 questId, const QuestStatusData & data)> questDataChanged;
		/// Fired when a kill credit for a specific quest was made. Used to send a packet to the owning players client.
		boost::signals2::signal<void(const proto::QuestEntry &, UInt64 guid, UInt32 entry, UInt32 count, UInt32 total)> questKillCredit;
		/// Fired when the character want to inspect loot of an object. Used to send packets to the owning players client.
		boost::signals2::signal<void(LootInstance &)> lootinspect;
		/// Fired when a spell mod was applied or misapplied on the character. Used to send packets to the owning players client.
		boost::signals2::signal<void(SpellModType, UInt8, SpellModOp, Int32)> spellModChanged;
		/// Fired when the character interacts with a game object.
		boost::signals2::signal<void(WorldObject &)> objectInteraction;
		/// Fired when a new spell was learned.
		boost::signals2::signal<void(const proto::SpellEntry &)> spellLearned;

	public:

		/// Creates a new instance of the GameCharacter class. Remember, the GameCharacter will not
		/// be valid, as you at least need to call GameCharacter::initialize(), GameCharacter::setMapId()
		/// and GameCharacter::relocate() to have a valid character. You also have to setup a valid GUID.
		explicit GameCharacter(
		    proto::Project &project,
		    TimerQueue &timers);
		/// Default destructor.
		~GameCharacter();

	public:

		// GameObject overrides

		/// @copydoc GameObject::initialize()
		virtual void initialize() override;
		/// @copydoc GameObject::getTypeId()
		virtual ObjectType getTypeId() const override { return object_type::Character; }
		/// @copydoc GameObject::getName
		const String &getName() const override { return m_name; }

	public:

		// GameUnit overrides

		/// @copydoc GameUnit::hasMainHandWeapon
		bool hasMainHandWeapon() const override;
		/// @copydoc GameUnit::hasOffHandWeapon
		bool hasOffHandWeapon() const override;
		/// @copydoc GameUnit::canBlock
		bool canBlock() const override;
		/// @copydoc GameUnit::canParry
		bool canParry() const override;
		/// @copydoc GameUnit::canDodge
		bool canDodge() const override;
		/// @copydoc GameUnit::canDualWield
		bool canDualWield() const override;
		/// @copydoc GameUnit::rewardExperience()
		void rewardExperience(GameUnit *victim, UInt32 experience) override;

	protected:

		// GameUnit overrides

		/// @copydoc GameUnit::levelChanged
		virtual void levelChanged(const proto::LevelEntry &levelInfo) override;
		/// @copydoc GameUnit::updateArmor
		virtual void updateArmor() override;
		/// @copydoc GameUnit::updateDamage
		virtual void updateDamage() override;
		/// @copydoc GameUnit::updateManaRegen
		virtual void updateManaRegen() override;
		/// @copydoc GameUnit::regenerateHealth
		virtual void regenerateHealth() override;
		/// @copydoc GameUnit::onThreaten
		void onThreat(GameUnit &threatener, float amount) override;

	private:

		// GameUnit overrides

		/// @copydoc GameUnit::classUpdated
		void classUpdated() override;
		
	public:

		// GameCharacter methods

		/// Adds a spell to the list of known spells of this character.
		/// Note that passive spells will also be cast after they are added.
		bool addSpell(const proto::SpellEntry &spell);
		/// Returns true, if the characters knows the specific spell.
		bool hasSpell(UInt32 spellId) const;
		/// Returns a spell from the list of known spells.
		bool removeSpell(const proto::SpellEntry &spell);
		/// Sets the name of this character.
		void setName(const String &name);
		/// Updates the zone where this character is. This variable is used by
		/// the friend list and the /who list.
		void setZone(UInt32 zoneIndex) { m_zoneIndex = zoneIndex; }
		/// Gets the zone index where this character is.
		UInt32 getZone() const { return m_zoneIndex; }
		/// Gets a list of all known spells of this character.
		const std::vector<const proto::SpellEntry *> &getSpells() const { return m_spells; }
		/// Gets the weapon proficiency mask of this character (which weapons can be
		/// wielded)
		UInt32 getWeaponProficiency() const { return m_weaponProficiency; }
		/// Gets the armor proficiency mask of this character (which armor types
		/// can be wielded: Cloth, Leather, Mail, Plate etc.)
		UInt32 getArmorProficiency() const {
			return m_armorProficiency;
		}
		/// Adds a new weapon proficiency to the mask.
		void addWeaponProficiency(UInt32 mask) {
			m_weaponProficiency |= mask;
			proficiencyChanged(2, m_weaponProficiency);
		}
		/// Adds a new armor proficiency to the mask.
		void addArmorProficiency(UInt32 mask) {
			m_armorProficiency |= mask;
			proficiencyChanged(4, m_armorProficiency);
		}
		/// Removes a weapon proficiency from the mask.
		void removeWeaponProficiency(UInt32 mask) {
			m_weaponProficiency &= ~mask;
			proficiencyChanged(2, m_weaponProficiency);
		}
		/// Removes an armor proficiency from the mask.
		void removeArmorProficiency(UInt32 mask) {
			m_armorProficiency &= ~mask;
			proficiencyChanged(4, m_armorProficiency);
		}
		/// Adds a new skill to the list of known skills of this character.
		void addSkill(const proto::SkillEntry &skill);
		/// Removes a skill from the list of known skills.
		void removeSkill(UInt32 skillId);
		/// Updates the skill values for a given skill of this character.
		void setSkillValue(UInt32 skillId, UInt16 current, UInt16 maximum);
		/// Gets the current skill value and max value of a given spell.
		/// @returns false if the character doesn't have this skill.
		bool getSkillValue(UInt32 skillId, UInt16 &out_current, UInt16 &out_max) const;
		/// Returns true if the character knows a specific skill.
		bool hasSkill(UInt32 skillId) const;
		/// Gets the GUID of the current combo point target.
		UInt64 getComboTarget() const { return m_comboTarget; }
		/// Gets the number of combo points of this character.
		UInt8 getComboPoints() const { return m_comboPoints; }
		/// Adds combo points to the specified target. This will reset combo points
		/// if target is a new target combo target. If a value of 0 is specified, the combo
		/// points will be reset to zero.
		void addComboPoints(UInt64 target, UInt8 points);
		/// Applies or removes item stats for this character.
		void applyItemStats(GameItem &item, bool apply);
		/// Gets the characters home location.
		void getHome(UInt32 &out_map, math::Vector3 &out_pos, float &out_rot) const;
		/// Updates the characters home location.
		void setHome(UInt32 map, const math::Vector3 &pos, float rot);
		/// Gets the current group update flags of this character.
		GroupUpdateFlags getGroupUpdateFlags() const { return m_groupUpdateFlags; }
		/// Modifies the group update flags, marking it as changed or unchaned.
		/// @param flags The group update flag to set or unset.
		/// @param apply Whether to set or unset the flag.
		void modifyGroupUpdateFlags(GroupUpdateFlags flags, bool apply);
		/// Resets group update flags so that the server knows that no group update has to be done
		/// for this character.
		void clearGroupUpdateFlags() { m_groupUpdateFlags = group_update_flags::None; }
		/// Gets the characters group id.
		UInt64 getGroupId() const { return m_groupId; }
		/// Sets the characters group id.
		void setGroupId(UInt64 groupId) { m_groupId = groupId; }
		/// Gets a reference to the characters inventory component.
		Inventory &getInventory() { return m_inventory; }
		/// Gets the current status of a given quest by its id.
		/// @returns Quest status.
		game::QuestStatus getQuestStatus(UInt32 quest) const;
		/// Accepts a new quest.
		/// @returns false if this wasn't possible (maybe questlog was full or not all requirements are met).
		bool acceptQuest(UInt32 quest);
		/// Abandons the specified quest.
		/// @returns false if this wasn't possible (maybe because the quest wasn't in the players quest log).
		bool abandonQuest(UInt32 quest);
		/// Updates the status of a specified quest to "completed". This does not work for quests that require
		/// a certain item.
		bool completeQuest(UInt32 quest);
		/// Makes a certain quest fail.
		bool failQuest(UInt32 quest);
		/// Rewards the given quest (gives items, xp and saves quest status).
		bool rewardQuest(UInt32 quest, UInt8 rewardChoice, std::function<void(UInt32)> callback);
		/// Called when a quest-related creature was killed.
		void onQuestKillCredit(GameCreature &killed);
		/// Determines whether the character fulfulls all requirements of the given quests.
		bool fulfillsQuestRequirements(const proto::QuestEntry &entry) const;
		/// Determines whether the players questlog is full.
		bool isQuestlogFull() const;
		/// Called when a quest item was added to the inventory.
		void onQuestItemAddedCredit(const proto::ItemEntry &entry, UInt32 amount);
		/// Called when a quest item was removed from the inventory.
		void onQuestItemRemovedCredit(const proto::ItemEntry &entry, UInt32 amount);
		/// Called when a quest item was removed from the inventory.
		void onQuestSpellCastCredit(UInt32 spellId, GameObject &target);
		/// Called when a character interacted with a quest object.
		void onQuestObjectCredit(UInt32 spellId, WorldObject &target);
		/// Determines if the player needs a specific item for a quest.
		bool needsQuestItem(UInt32 itemId) const;
		/// Modifies the character spell modifiers by applying or misapplying a new mod.
		/// @param mod The spell modifier to apply or misapply.
		/// @param apply Whether to apply or misapply the spell mod.
		void modifySpellMod(SpellModifier &mod, bool apply);
		/// Gets the total amount of spell mods for one type and one spell.
		Int32 getTotalSpellMods(SpellModType type, SpellModOp op, UInt32 spellId) const;
		/// Applys all matching spell mods of this character to a given value.
		/// @param op The spell modifier operation to apply.
		/// @param spellId Id of the spell, to know which modifiers do match.
		/// @param ref_value Reference of the base value, which will be modified by this method.
		/// @returns Delta value or 0 if ref_value didn't change.
		template<class T>
		T applySpellMod(SpellModOp op, UInt32 spellId, T& ref_value) const
		{
			float totalPct = 1.0f;
			Int32 totalFlat = 0;

			totalFlat += getTotalSpellMods(spell_mod_type::Flat, op, spellId);
			totalPct += float(getTotalSpellMods(spell_mod_type::Pct, op, spellId)) * 0.01f;

			const float diff = float(ref_value) * (totalPct - 1.0f) + float(totalFlat);
			ref_value = T(float(ref_value) + diff);

			return T(diff);
		}
		/// Modifies the bonus threat modifier for that character.
		/// @param schoolMask The damage schools which are affected.
		/// @param modifier The bonus amount where 0.4 equals +40%, -0.1 equals -10%.
		/// @param apply Whether to apply or misapply the threat bonus.
		void modifyThreatModifier(UInt32 schoolMask, float modifier, bool apply);
		/// Applys the characters threat modifiers.
		/// @param schoolMask The requested damage schools.
		/// @param ref_threat The current threat value, which can be modified by this method.
		void applyThreatMod(UInt32 schoolMask, float &ref_threat);

	public:

		// WARNING: THESE METHODS ARE ONLY CALLED WHEN LOADED FROM THE DATABASE. THEY SHOULD NOT
		// BE CALLED ANYWHERE ELSE!

		/// Manually sets data for a specified quest id.
		void setQuestData(UInt32 quest, const QuestStatusData &data);

	private:

		/// Updates the amount of available talent points of this character, by calculating it.
		void updateTalentPoints();
		/// Initializes class-specific mechanics like combo-point generation on enemy dodge for
		/// warriors (which will enable "Overpower" ability) etc.
		void initClassEffects();
		/// Updates nearby game objects after a quest change happened (some game objects are only
		/// lootable and/or shining when certain quests are incomplete).
		void updateNearbyQuestObjects();

	private:

		// Variables

		String m_name;
		UInt32 m_zoneIndex;
		UInt32 m_weaponProficiency;
		UInt32 m_armorProficiency;
		std::vector<const proto::SkillEntry *> m_skills;
		std::vector<const proto::SpellEntry *> m_spells;
		UInt64 m_comboTarget;
		UInt8 m_comboPoints;
		float m_healthRegBase, m_manaRegBase;
		GroupUpdateFlags m_groupUpdateFlags;
		bool m_canBlock;	// Set by spell, do not set manually
		bool m_canParry;	// Set by spell, do not set manually
		bool m_canDualWield;// Set by spell, do not set manually
		FactionStateList m_factions;
		ForcedReactions m_forcedReactions;
		UInt64 m_groupId;	// Used to determine if in same player group as other characters
		UInt32 m_homeMap;
		math::Vector3 m_homePos;
		float m_homeRotation;
		boost::signals2::scoped_connection m_doneMeleeAttack;
		std::map<UInt32, QuestStatusData> m_quests;
		Inventory m_inventory;
		/// We use a map here since multiple quests could require the same item and thus,
		/// if one quest gets rewarded/abandoned, and the other quest is still incomplete,
		/// we would have no performant way to check this.
		std::map<UInt32, Int8> m_requiredQuestItems;
		SpellModsByOp m_spellModsByOp;
		std::array<float, 7> m_threatModifier;
		std::vector<Countdown> m_questTimeouts;
	};

	/// Serializes a GameCharacter to an io::Writer object for the wow++ protocol.
	/// @param w The writer used to write to.
	/// @param object The GameCharacter object to serialize.
	io::Writer &operator << (io::Writer &w, GameCharacter const &object);
	/// Deserializes a GameCharacter from an io::Reader object for the wow++ protocol.
	/// @param r The reader used to read from.
	/// @param out_object A reference of the GameCharacter object whose values will be read.
	io::Reader &operator >> (io::Reader &r, GameCharacter &out_object);
}
