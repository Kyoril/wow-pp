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
#include "movement_info.h"
#include <boost/signals2/signal.hpp>
#include "common/timer_queue.h"
#include "tile_index.h"
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

	namespace guid_type
	{
		enum Enum
		{
			Player = 0x0,
			GameObject = 0x1,
			Transport = 0x2,
			Unit = 0x3,
			Pet = 0x4,
			Item = 0x5
		};
	}

	typedef guid_type::Enum GUIDType;

	/// Gets the high part of a guid which can be used to determine the object type by it's GUID.
	inline GUIDType guidTypeID(UInt64 guid) { return static_cast<GUIDType>(static_cast<UInt32>((guid >> 52) & 0xF)); }
	/// Gets the realm id of a guid.
	inline UInt16 guidRealmID(UInt64 guid) { return static_cast<UInt16>((guid >> 56) & 0xFF); }
	/// Determines whether the given GUID belongs to a creature.
	inline bool isCreatureGUID(UInt64 guid) { return guidTypeID(guid) == guid_type::Unit; }
	/// Determines whether the given GUID belongs to a pet.
	inline bool isPetGUID(UInt64 guid) { return guidTypeID(guid) == guid_type::Pet; }
	/// Determines whether the given GUID belongs to a player.
	inline bool isPlayerGUID(UInt64 guid) { return guidTypeID(guid) == guid_type::Player; }
	/// Determines whether the given GUID belongs to a unit.
	inline bool isUnitGUID(UInt64 guid) { return isPlayerGUID(guid) || isCreatureGUID(guid) || isPetGUID(guid); }
	/// Determines whether the given GUID belongs to an item.
	inline bool isItemGUID(UInt64 guid) { return guidTypeID(guid) == guid_type::Item; }
	/// Determines whether the given GUID belongs to a game object (chest for example).
	inline bool isGameObjectGUID(UInt64 guid) { return guidTypeID(guid) == guid_type::GameObject; }
	/// Creates a GUID based on some settings.
	inline uint64_t createRealmGUID(uint64_t low, uint64_t realm, GUIDType type) { return static_cast<uint64_t>(low | (realm << 56) | (static_cast<uint64_t>(type) << 52)); }
	inline uint64_t createEntryGUID(uint64_t low, uint64_t entry, GUIDType type) { return static_cast<uint64_t>(low | (entry << 24) | (static_cast<uint64_t>(type) << 52) | 0xF100000000000000); }

	/// Determines if a GUID has an entry part based on it's type.
	inline bool guidHasEntryPart(UInt64 guid)
	{
		switch (guidTypeID(guid))
		{
			case guid_type::Item:
			case guid_type::Player:
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

	class VisibilityTile;
	class WorldInstance;

	/// 
	class GameObject
	{
		friend io::Writer &operator << (io::Writer &w, GameObject const& object);
		friend io::Reader &operator >> (io::Reader &r, GameObject& object);

	public:

		/// Fired when the object was added to a world instance and spawned.
		boost::signals2::signal<void()> spawned;
		/// Fired when the object was removed from a world instance and despawned.
		boost::signals2::signal<void(GameObject &)> despawned;
		/// Fired when the object moved, but before it's tile changed. Note that this will trigger a tile change.
		boost::signals2::signal<void(GameObject &, float, float, float, float)> moved;
		/// Fired when a tile change is pending for this object, after it has been moved. Note that at this time,
		/// the object does not belong to any tile and it's position already points to the new tile.
		/// First parameter is a reference of the old tile, second references the new tile.
		boost::signals2::signal<void(VisibilityTile&, VisibilityTile&)> tileChangePending;

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
		/// Gets the unit's tile index in the world grid.
		bool getTileIndex(TileIndex2D &out_index) const;

		/// Moves the object to the given position on it's map id.
		void relocate(float x, float y, float z, float o);
		/// Updates the map id of this object.
		void setMapId(UInt32 mapId);

		/// Gets the object type id value.
		virtual ObjectType getTypeId() const { return object_type::Object; }
		/// Determines whether the object has been updated.
		bool wasUpdated() const { return m_updated; }
		/// Marks all changed values of this object as unchanged.
		void clearUpdateMask();
		/// Calculates the distance of this object to another object.
		/// @param use3D If true, the distance will be calculated using 3d coordinates, otherwise,
		///              only 2d coordinates are used.
		float getDistanceTo(GameObject &other, bool use3D = true) const;

		const MovementInfo &getMovementInfo() { return m_movementInfo; }
		void setMovementInfo(const MovementInfo &info) { m_movementInfo = info; }

		/// Gets the world instance of this object. May be nullptr, if the object is
		/// not in any world.
		WorldInstance *getWorldInstance() { return m_worldInstance; }
		/// Sets the world instance of this object. nullptr is valid here, if the object
		/// is not in any world.
		void setWorldInstance(WorldInstance *instance);

		/// 
		float getAngle(float x, float z) const;
		/// 
		bool isInArc(float arcRadian, float x, float y) const;

	protected:

		void onWorldInstanceDestroyed();

	protected:

		std::vector<UInt32> m_values;
		std::vector<UInt32> m_valueBitset;
		UInt32 m_mapId;
		float m_x, m_y, m_z;
		float m_o;
		UInt32 m_objectType;
		UInt32 m_objectTypeId;
		bool m_updated;
		MovementInfo m_movementInfo;
		WorldInstance *m_worldInstance;
		boost::signals2::scoped_connection m_worldInstanceDestroyed;
	};

	io::Writer &operator << (io::Writer &w, GameObject const& object);
	io::Reader &operator >> (io::Reader &r, GameObject& object);

	void createUpdateBlocks(GameObject &object, std::vector<std::vector<char>> &out_blocks);
}
