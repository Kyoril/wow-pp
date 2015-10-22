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
#include "defines.h"

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
			QuestLog14_4				= 0x41+ unit_fields::UnitFieldCount,
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

	struct SpellEntry;
	struct ItemEntry;
	struct SkillEntry;

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

	/// 
	class GameCharacter : public GameUnit
	{
		friend io::Writer &operator << (io::Writer &w, GameCharacter const& object);
		friend io::Reader &operator >> (io::Reader &r, GameCharacter& object);

	public:

		boost::signals2::signal<void(Int32, UInt32)> proficiencyChanged;
		boost::signals2::signal<void(game::InventoryChangeFailure, GameItem*, GameItem*)> inventoryChangeFailure;
		boost::signals2::signal<void()> comboPointsChanged;
		boost::signals2::signal<void(UInt64, UInt32, UInt32)> experienceGained;
		boost::signals2::signal<void()> homeChanged;

	public:

		/// 
		explicit GameCharacter(
			TimerQueue &timers,
			DataLoadContext::GetRace getRace,
			DataLoadContext::GetClass getClass,
			DataLoadContext::GetLevel getLevel,
			DataLoadContext::GetSpell getSpell);
		~GameCharacter();

		/// @copydoc GameObject::initialize()
		virtual void initialize() override;
		/// @copydoc GameObject::getTypeId()
		virtual ObjectType getTypeId() const override { return object_type::Character; }

		/// Adds an item to a given slot.
		void addItem(std::shared_ptr<GameItem> item, UInt16 slot);
		/// Adds a spell to the list of known spells of this character.
		/// Note that passive spells will also be cast after they are added.
		void addSpell(const SpellEntry &spell);
		/// Returns true, if the characters knows the specific spell.
		bool hasSpell(UInt32 spellId) const;
		/// Returns a spell from the list of known spells.
		bool removeSpell(const SpellEntry &spell);
		/// Sets the name of this character.
		void setName(const String &name);
		/// Gets the name of this character.
		const String &getName() const override { return m_name; }
		/// Updates the zone where this character is. This variable is used by
		/// the friend list and the /who list.
		void setZone(UInt32 zoneIndex) { m_zoneIndex = zoneIndex; }
		/// Gets the zone index where this character is.
		UInt32 getZone() const { return m_zoneIndex; }
		/// Gets a list of all known spells of this character.
		const std::vector<const SpellEntry*> &getSpells() const { return m_spells; }
		/// Gets a list of all items of this character.
		const std::map<UInt16, std::shared_ptr<GameItem>> &getItems() const { return m_itemSlots; }
		/// Gets the weapon proficiency mask of this character (which weapons can be
		/// wielded)
		UInt32 getWeaponProficiency() const { return m_weaponProficiency; }
		/// Gets the armor proficiency mask of this character (which armor types
		/// can be wielded: Cloth, Leather, Mail, Plate etc.)
		UInt32 getArmorProficiency() const { return m_armorProficiency; }
		/// Adds a new weapon proficiency to the mask.
		void addWeaponProficiency(UInt32 mask) { m_weaponProficiency |= mask; proficiencyChanged(2, m_weaponProficiency); }
		/// Adds a new armor proficiency to the mask.
		void addArmorProficiency(UInt32 mask) { m_armorProficiency |= mask; proficiencyChanged(4, m_armorProficiency); }
		/// Removes a weapon proficiency from the mask.
		void removeWeaponProficiency(UInt32 mask) { m_weaponProficiency &= ~mask; proficiencyChanged(2, m_weaponProficiency); }
		/// Removes an armor proficiency from the mask.
		void removeArmorProficiency(UInt32 mask) { m_armorProficiency &= ~mask; proficiencyChanged(4, m_armorProficiency); }
		/// Adds a new skill to the list of known skills of this character.
		void addSkill(const SkillEntry &skill);
		/// 
		void removeSkill(UInt32 skillId);
		/// 
		void setSkillValue(UInt32 skillId, UInt16 current, UInt16 maximum);
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
		/// Determines whether the given slot is a valid slot in the specified bag.
		bool isValidItemPos(UInt8 bag, UInt8 slot) const;
		/// Swaps the items of two given slots.
		void swapItem(UInt16 src, UInt16 dst);
		/// Tries to get the item object at the given slot. May return nullptr!
		GameItem *getItemByPos(UInt8 bag, UInt8 slot) const;
		/// @copydoc GameUnit::rewardExperience()
		void rewardExperience(GameUnit *victim, UInt32 experience) override;
		/// Determines, whether a specific amount of items can be stored.
		game::InventoryChangeFailure canStoreItem(UInt8 bag, UInt8 slot, ItemPosCountVector &dest, const ItemEntry &item, UInt32 count, bool swap, UInt32 *noSpaceCount = nullptr) const;
		/// Removes an amount of items from the player.
		void removeItem(UInt8 bag, UInt8 slot, UInt8 count);
		/// Gets the characters home location.
		void getHome(UInt32 &out_map, game::Position &out_pos, float &out_rot) const;
		/// Updates the characters home location.
		void setHome(UInt32 map, const game::Position &pos, float rot);
		/// Gets an item by it's guid.
		GameItem *getItemByGUID(UInt64 guid, UInt8 &out_bag, UInt8 &out_slot);

		GroupUpdateFlags getGroupUpdateFlags() const { return m_groupUpdateFlags; }
		void modifyGroupUpdateFlags(GroupUpdateFlags flags, bool apply);
		void clearGroupUpdateFlags() { m_groupUpdateFlags = group_update_flags::None; }
		bool canBlock() const override;
		bool canParry() const override;
		bool canDodge() const override;

		/// Gets the characters group id.
		UInt64 getGroupId() const { return m_groupId; }
		/// Sets the characters group id.
		void setGroupId(UInt64 groupId) { m_groupId = groupId; }

	protected:

		virtual void levelChanged(const LevelEntry &levelInfo) override;
		virtual void updateArmor() override;
		virtual void updateDamage() override;
		virtual void updateManaRegen() override;
		virtual void regenerateHealth() override;

	private:

		void updateTalentPoints();

	private:

		String m_name;
		UInt32 m_zoneIndex;
		UInt32 m_weaponProficiency;
		UInt32 m_armorProficiency;
		std::vector<const SkillEntry*> m_skills;
		std::vector<const SpellEntry*> m_spells;
		std::map<UInt16, std::shared_ptr<GameItem>> m_itemSlots;
		UInt64 m_comboTarget;
		UInt8 m_comboPoints;
		float m_healthRegBase, m_manaRegBase;
		GroupUpdateFlags m_groupUpdateFlags;
		bool m_canBlock;	// Set by spell
		bool m_canParry;	// Set by spell
		FactionStateList m_factions;
		ForcedReactions m_forcedReactions;
		UInt64 m_groupId;
		UInt32 m_homeMap;
		game::Position m_homePos;
		float m_homeRotation;
	};

	io::Writer &operator << (io::Writer &w, GameCharacter const& object);
	io::Reader &operator >> (io::Reader &r, GameCharacter& object);
}
