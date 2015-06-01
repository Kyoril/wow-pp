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
#include "binary_io/writer.h"
#include "binary_io/reader.h"
#include <vector>

namespace wowpp
{
	namespace type_id
	{
		enum Enum
		{
			Object				= 0x00,
			Item				= 0x01,
			Container			= 0x02,
			Unit				= 0x03,
			Player				= 0x04,
			GameObject			= 0x05,
			DynamicObject		= 0x06,
			Corpse				= 0x07
		};
	}

	typedef type_id::Enum TypeID;

	namespace type_mask
	{
		enum Enum
		{
			Object				= 0x0001,
			Item				= 0x0002,
			Container			= 0x0006,
			Unit				= 0x0008,
			Player				= 0x0010,
			GameObject			= 0x0020,
			DynamicObject		= 0x0040,
			Corpse				= 0x0080,
			Seer				= Unit | DynamicObject
		};
	}

	typedef type_mask::Enum TypeMask;

	namespace high_guid
	{
		enum Enum
		{
			Item				= 0x4000,
			Container			= 0x4000,
			Player				= 0x0000,
			GameObject			= 0xF110,
			Transport			= 0xF120,
			Unit				= 0xF130,
			Pet					= 0xF140,
			DynamicObject		= 0xF100,
			Corpse				= 0xF101,
			MOTransport			= 0x1FC0
		};
	}

	typedef high_guid::Enum HighGUID;

	/// Gets the high part of a guid which can be used to determine the object type by it's GUID.
	inline HighGUID guidHiPart(UInt64 guid) { return static_cast<HighGUID>(static_cast<UInt32>((guid >> 0x0000000000000030) & 0x0000FFFF)); }
	/// Determines whether the given GUID belongs to a creature.
	inline bool isCreatureGUID(UInt64 guid) { return guidHiPart(guid) == high_guid::Unit; }
	/// Determines whether the given GUID belongs to a pet.
	inline bool isPetGUID(UInt64 guid) { return guidHiPart(guid) == high_guid::Pet; }
	/// Determines whether the given GUID belongs to a player.
	inline bool isPlayerGUID(UInt64 guid) { return guidHiPart(guid) == high_guid::Player; }
	/// Determines whether the given GUID belongs to a unit.
	inline bool isUnitGUID(UInt64 guid) { return isPlayerGUID(guid) || isCreatureGUID(guid) || isPetGUID(guid); }
	/// Determines whether the given GUID belongs to an item.
	inline bool isItemGUID(UInt64 guid) { return guidHiPart(guid) == high_guid::Item; }
	/// Determines whether the given GUID belongs to a game object (chest for example).
	inline bool isGameObjectGUID(UInt64 guid) { return guidHiPart(guid) == high_guid::GameObject; }
	/// Determines whether the given GUID belongs to a corpse.
	inline bool isCorpseGUID(UInt64 guid) { return guidHiPart(guid) == high_guid::Corpse; }
	/// Determines whether the given GUID belongs to a MO Transport. (what does MO stand for?)
	inline bool isMOTransportGUID(UInt64 guid) { return guidHiPart(guid) == high_guid::MOTransport; }
	/// Creates a GUID based on some settings.
	inline UInt64 createGUID(UInt64 low, UInt64 entry, HighGUID high) { return static_cast<UInt64>(low | (entry << 24) | (static_cast<UInt64>(high) << 48)); }
	/// Determines if a GUID has an entry part based on it's type.
	inline bool guidHasEntryPart(UInt64 guid)
	{
		switch (guidHiPart(guid))
		{
			case high_guid::Item:
			case high_guid::Player:
			case high_guid::DynamicObject:
			case high_guid::Corpse:
			case high_guid::MOTransport:
				return false;
			default:
				return true;
		}
	}
	/// Gets the entry part of a GUID or 0 if the GUID does not have an entry part.
	inline UInt32 guidEntryPart(UInt64 guid)
	{
		if (guidHasEntryPart(guid))
		{
			return static_cast<UInt32>((guid >> 24) & static_cast<UInt64>(0x0000000000FFFFFF));
		}

		return 0;
	}
	/// Gets the lower part of a GUID based on it's type.
	inline UInt32 guidLowerPart(UInt64 guid)
	{
		const UInt64 low2 = 0x00000000FFFFFFFF;
		const UInt64 low3 = 0x0000000000FFFFFF;

		return guidHasEntryPart(guid) ?
			static_cast<UInt32>(guid & low3) : static_cast<UInt32>(guid & low2);
	}

	namespace object_fields
	{
		enum Enum
		{
			/// 64 bit object GUID
			Guid				= 0x00,
			/// Object type id
			Type				= 0x02,
			/// 
			Entry				= 0x03,
			/// Object model scale (1.0 = 100%)
			ScaleX				= 0x04,
			/// 
			Padding				= 0x05,

			ObjectFieldCount	= 0x06
		};
	}

	namespace object_type
	{
		enum Enum
		{
			Object = 0,
			Item = 1,
			Container = 2,
			Unit = 3,
			Character = 4,
			GameObject = 5,
			DynamicObject = 6,
			Corpse = 7
		};
	}

	typedef object_type::Enum ObjectType;


	/// 
	class GameObject
	{
		friend io::Writer &operator << (io::Writer &w, GameObject const& object);
		friend io::Reader &operator >> (io::Reader &r, GameObject& object);

	public:

		/// 
		explicit GameObject();
		virtual ~GameObject();

		virtual void initialize();

		/// Gets the number of values of this object.
		UInt32 getValueCount() const { return m_values.size(); }

		UInt8 getByteValue(UInt16 index, UInt8 offset) const;
		UInt16 getUInt16Value(UInt16 index, UInt8 offset) const;
		Int32 getInt32Value(UInt16 index) const;
		UInt32 getUInt32Value(UInt16 index) const;
		UInt64 getUInt64Value(UInt16 index) const;
		float getFloatValue(UInt16 index) const;

		void setByteValue(UInt16 index, UInt8 offset, UInt8 value);
		void setUInt16Value(UInt16 index, UInt8 offset, UInt16 value);
		void setUInt32Value(UInt16 index, UInt32 value);
		void setInt32Value(UInt16 index, Int32 value);
		void setUInt64Value(UInt16 index, UInt64 value);
		void setFloatValue(UInt16 index, float value);

		void setGuid(UInt64 guid) { setUInt64Value(object_fields::Guid, guid); }
		UInt64 getGuid() const { return getUInt64Value(object_fields::Guid); }

		void setCreateBits();

		/// Writes the value update block of this game object.
		/// @param writer The writer object used to write the values.
		/// @param creation Set to true if this is the value update packet used at object creation.
		///	       At object creation, all values which aren't equal to zero will be written, not just the
		///	       updated ones.
		void writeValueUpdateBlock(io::Writer &writer, bool creation = true) const;

		/// Gets the location of this object.
		void getLocation(float &out_x, float &out_y, float &out_z, float &out_o) const { out_x = m_x; out_y = m_y; out_z = m_z; out_o = m_o; }
		/// Gets the map id of this object.
		UInt32 getMapId() const { return m_mapId; }

		/// Moves the object to the given position on it's map id.
		void relocate(float x, float y, float z, float o);
		/// Updates the map id of this object.
		void setMapId(UInt32 mapId);

		/// Gets the object type id value.
		virtual ObjectType getTypeId() const { return object_type::Object; }

	protected:

		std::vector<UInt32> m_values;
		std::vector<UInt32> m_valueBitset;
		UInt32 m_mapId;
		float m_x, m_y, m_z;
		float m_o;
		UInt32 m_objectType;
		UInt32 m_objectTypeId;

	};

	io::Writer &operator << (io::Writer &w, GameObject const& object);
	io::Reader &operator >> (io::Reader &r, GameObject& object);
}
