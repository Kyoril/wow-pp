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

#include "game_object.h"
//#include "data/object_entry.h"
#include "common/timer_queue.h"
#include "common/countdown.h"

namespace wowpp
{
	namespace proto
	{
		class ObjectEntry;
	}

	namespace world_object_fields
	{
		enum Enum
		{
			/// 64 bit object GUID
			Createdby				= object_fields::ObjectFieldCount + 0x00,
			/// 
			DisplayId				= object_fields::ObjectFieldCount +0x02,
			/// 
			Flags					= object_fields::ObjectFieldCount +0x03,
			/// 
			Rotation				= object_fields::ObjectFieldCount +0x04,
			/// 
			State					= object_fields::ObjectFieldCount +0x08,
			/// 
			PosX					= object_fields::ObjectFieldCount +0x09,
			/// 
			PosY					= object_fields::ObjectFieldCount +0x0A,
			/// 
			PosZ					= object_fields::ObjectFieldCount +0x0B,
			/// 
			Facing					= object_fields::ObjectFieldCount +0x0C,
			/// 
			DynFlags				= object_fields::ObjectFieldCount +0x0D,
			/// 
			Faction					= object_fields::ObjectFieldCount +0x0E,
			/// 
			TypeID					= object_fields::ObjectFieldCount +0x0F,
			/// 
			Level					= object_fields::ObjectFieldCount +0x10,
			/// 
			ArtKit					= object_fields::ObjectFieldCount +0x11,
			/// 
			AnimProgress			= object_fields::ObjectFieldCount +0x12,
			/// 
			Padding					= object_fields::ObjectFieldCount +0x13,
			
			WorldObjectFieldCount	= object_fields::ObjectFieldCount +0x14
		};
	}

	namespace world_object_type
	{
		enum Type
		{
			Door					= 0,
			Button					= 1,
			QuestGiver				= 2,
			Chest					= 3,
			Binder					= 4,
			Generic					= 5,
			Trap					= 6,
			Chair					= 7,
			SpellFocus				= 8,
			Text					= 9,
			Goober					= 10,
			Transport				= 11,
			AreaDamage				= 12,
			Camera					= 13,
			MapObject				= 14,
			MOTransport				= 15,
			DuelArbiter				= 16,
			FishingNode				= 17,
			SummoningRitual			= 18,
			Mailbox					= 19,
			AuctionHouse			= 20,
			GuardPost				= 21,
			SpellCaster				= 22,
			MeetingStone			= 23,
			FlagStand				= 24,
			FishingHole				= 25,
			FlagDrop				= 26,
			MiniGame				= 27,
			LotteryKiosk			= 28,
			CapturePoint			= 29,
			AuraGenerator			= 30,
			DungeonDifficulty		= 31,
			BarberChair				= 32,
			DestructibleBuilding	= 33,
			GuildBank				= 34
		};
	}

	typedef world_object_type::Type WorldObjectType;

	namespace world_object_flags
	{
		enum Type
		{
			/// Disables interaction while animated.
			InUse			= 0x01,
			/// Requires Key, Spell, Event or something else to be opened. Makes "Locked" appear in tooltip.
			Locked			= 0x02,
			/// Condition is required to be able to interact with this object.
			InteractCond	= 0x04,
			/// Any kind of transport has this (Elevator, Boat, etc.).
			Transport		= 0x08,
			/// Not selectable at all.
			NotSelectable	= 0x10,
			/// Object does not despawn.
			NoDespawn		= 0x20,
			/// Summoned objects.
			Triggered		= 0x40
		};
	}

	namespace world_object_dyn_flags
	{
		enum Type
		{
			/// Enables interaction with this object.
			Activate			= 0x01,
			/// Possibly more distinct animation of object.
			Animate				= 0x02,
			/// Seems to disable interaction.
			NoInteract			= 0x04,
			/// Makes the object sparkle (quest objects for example).
			Sparkle				= 0x08
		};
	}

	/// 
	class WorldObject : public GameObject
	{
		friend io::Writer &operator << (io::Writer &w, WorldObject const& object);
		friend io::Reader &operator >> (io::Reader &r, WorldObject& object);

	public:

		/// 
		explicit WorldObject(
			proto::Project &project,
			TimerQueue &timers,
			const proto::ObjectEntry &entry);
		virtual ~WorldObject();

		/// 
		void initialize() override;
		/// 
		void relocate(float x, float y, float z, float o) override;
		/// 
		void writeCreateObjectBlocks(std::vector<std::vector<char>> &out_blocks, bool creation = true) const override;

		const proto::ObjectEntry &getEntry() const { return m_entry; }

		/// Gets the object type id value.
		ObjectType getTypeId() const override { return object_type::GameObject; }

	protected:

		TimerQueue &m_timers;
		const proto::ObjectEntry &m_entry;
	};

	io::Writer &operator << (io::Writer &w, WorldObject const& object);
	io::Reader &operator >> (io::Reader &r, WorldObject& object);
}
