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

#include "defines.h"
#include "common/typedefs.h"
#include "binary_io/writer.h"
#include "binary_io/reader.h"
#include "movement_info.h"
#include "common/timer_queue.h"
#include "tile_index.h"
#include "math/vector3.h"
#include "common/macros.h"

namespace wowpp
{
	namespace type_id
	{
		enum Enum
		{
			/// Should not really be used, as no such object should exist in the world.
			Object				= 0x00,
			/// Object is an item.
			Item				= 0x01,
			/// Object is a bag.
			Container			= 0x02,
			/// Object is a creature or pet.
			Unit				= 0x03,
			/// Object is a player character.
			Player				= 0x04,
			/// Object is a world object (chest, chair, etc.)
			GameObject			= 0x05,
			/// Unknown
			DynamicObject		= 0x06,
			/// Object is a player corpse.
			Corpse				= 0x07
		};
	}

	typedef type_id::Enum TypeID;

	namespace type_mask
	{
		enum Enum
		{
			/// Should be included in every object.
			Object				= 0x0001,
			/// Object is an item.
			Item				= 0x0002,
			/// Object is a bag (also an item).
			Container			= 0x0006,
			/// Object is a unit.
			Unit				= 0x0008,
			/// Object is a player character (also a unit).
			Player				= 0x0010,
			/// Object is a world object.
			GameObject			= 0x0020,
			/// Unknown
			DynamicObject		= 0x0040,
			/// Object is a corpse.
			Corpse				= 0x0080,
			/// Unknown
			Seer				= Unit | DynamicObject
		};
	}

	typedef type_mask::Enum TypeMask;

	namespace guid_type
	{
		enum Enum
		{
			/// Player guid constant.
			Player = 0x0,
			/// World object guid constant.
			GameObject = 0x1,
			/// Transporter object guid constant.
			Transport = 0x2,
			/// Creature guid constant.
			Unit = 0x3,
			/// Pet guid constant.
			Pet = 0x4,
			/// Item guid constant (also used for bags).
			Item = 0x5
		};
	}

	typedef guid_type::Enum GUIDType;

	/// Gets the high part of a guid which can be used to determine the object type by it's GUID.
	inline GUIDType guidTypeID(UInt64 guid) {
		return static_cast<GUIDType>(static_cast<UInt32>((guid >> 52) & 0xF));
	}
	/// Gets the realm id of a guid.
	inline UInt16 guidRealmID(UInt64 guid) {
		return static_cast<UInt16>((guid >> 56) & 0xFF);
	}
	/// Determines whether the given GUID belongs to a creature.
	inline bool isCreatureGUID(UInt64 guid) {
		return guidTypeID(guid) == guid_type::Unit;
	}
	/// Determines whether the given GUID belongs to a pet.
	inline bool isPetGUID(UInt64 guid) {
		return guidTypeID(guid) == guid_type::Pet;
	}
	/// Determines whether the given GUID belongs to a player.
	inline bool isPlayerGUID(UInt64 guid) {
		return guidTypeID(guid) == guid_type::Player;
	}
	/// Determines whether the given GUID belongs to a unit.
	inline bool isUnitGUID(UInt64 guid) {
		return isPlayerGUID(guid) || isCreatureGUID(guid) || isPetGUID(guid);
	}
	/// Determines whether the given GUID belongs to an item.
	inline bool isItemGUID(UInt64 guid) {
		return guidTypeID(guid) == guid_type::Item;
	}
	/// Determines whether the given GUID belongs to a game object (chest for example).
	inline bool isGameObjectGUID(UInt64 guid) {
		return guidTypeID(guid) == guid_type::GameObject;
	}
	/// Creates a GUID based on some settings.
	inline uint64_t createRealmGUID(uint64_t low, uint64_t realm, GUIDType type) {
		return static_cast<uint64_t>(low | (realm << 56) | (static_cast<uint64_t>(type) << 52));
	}
	inline uint64_t createEntryGUID(uint64_t low, uint64_t entry, GUIDType type) {
		return static_cast<uint64_t>(low | (entry << 24) | (static_cast<uint64_t>(type) << 52) | 0xF100000000000000);
	}

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
			/// Object entry (if this object has any entry).
			Entry				= 0x03,
			/// Object model scale (1.0 = 100%)
			ScaleX				= 0x04,
			/// Unused.
			Padding				= 0x05,

			/// Number of object field values.
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

	// Forwards
	class VisibilityTile;
	class WorldInstance;
	class GameCharacter;
	namespace proto
	{
		class Project;
	}

	/// Base class for any object in the world. This class is abstract and shouldn't be initialized.
	class GameObject : public std::enable_shared_from_this<GameObject>
	{
		friend io::Writer &operator << (io::Writer &w, GameObject const &object);
		friend io::Reader &operator >> (io::Reader &r, GameObject &object);

	public:

		/// Fired when the object was added to a world instance and spawned.
		boost::signals2::signal<void()> spawned;
		/// Fired when the object was removed from a world instance and despawned.
		/// WARNING: DO NOT DESTROY THE OBJECT HERE, AS MORE SIGNALS MAY BE CONNECTED WHICH WANT TO BE EXECUTED,
		boost::signals2::signal<void(GameObject &)> despawned;
		/// Fired when the object should be destroyed. The object should be destroyed after this call.
		std::function<void(GameObject &)> destroy;
		/// Fired when the object moved, but before it's tile changed. Note that this will trigger a tile change.
		boost::signals2::signal<void(GameObject &, const math::Vector3 &, float)> moved;
		/// Fired when a tile change is pending for this object, after it has been moved. Note that at this time,
		/// the object does not belong to any tile and it's position already points to the new tile.
		/// First parameter is a reference of the old tile, second references the new tile.
		boost::signals2::signal<void(VisibilityTile &, VisibilityTile &)> tileChangePending;

	public:

		/// Default constructor.
		/// @param project Reference to the data project which is used to lookup entries.
		explicit GameObject(proto::Project &project);
		/// Destructor provided because of inheritance.
		virtual ~GameObject();

		/// Initializes this object, setting up all basic fields etc.
		virtual void initialize();

		/// Gets the number of values of this object.
		UInt32 getValueCount() const {
			return m_values.size();
		}

		/// Gets given byte value from the field list.
		/// @param index The searched field index.
		/// @param offset The byte offset, ranging from 0-3 (1 field = 32 bits)
		UInt8 getByteValue(UInt16 index, UInt8 offset) const;
		/// Gets given unsigned 16 bit value from the field list.
		/// @param index The searched field index.
		/// @param offset The byte offset, ranging from 0-1 (1 field = 32 bits)
		UInt16 getUInt16Value(UInt16 index, UInt8 offset) const;
		/// Gets given signed 32 bit value from the field list.
		/// @param index The searched field index.
		Int32 getInt32Value(UInt16 index) const;
		/// Gets given unsigned 32 bit value from the field list.
		/// @param index The searched field index.
		UInt32 getUInt32Value(UInt16 index) const;
		/// Gets given unsigned 64 bit value from the field list, by combining two fields (index and index+1).
		/// @param index The searched field index. Has to be < field_count - 1
		UInt64 getUInt64Value(UInt16 index) const;
		/// Gets given float value from the field list.
		/// @param index The searched field index.
		float getFloatValue(UInt16 index) const;
		/// Sets a byte value in the field list.
		/// @param index The field index to update.
		/// @param offset The byte offset, ranging from 0-3 (1 field = 32 bits)
		/// @param value The new value to set.
		void setByteValue(UInt16 index, UInt8 offset, UInt8 value);
		/// Sets an unsigned 16 bit value in the field list.
		/// @param index The field index to update.
		/// @param offset The byte offset, ranging from 0-1 (1 field = 32 bits)
		/// @param value The new value to set.
		void setUInt16Value(UInt16 index, UInt8 offset, UInt16 value);
		/// Sets an unsigned 32 bit value in the field list.
		/// @param index The field index to update.
		/// @param value The new value to set.
		void setUInt32Value(UInt16 index, UInt32 value);
		/// Sets a signed 32 bit value in the field list.
		/// @param index The field index to update.
		/// @param value The new value to set.
		void setInt32Value(UInt16 index, Int32 value);
		/// Sets an unsigned 64 bit value in the field list. Actually affects two fields (index and index+1).
		/// @param index The field index to update. Has to be < field_count - 1
		/// @param value The new value to set.
		void setUInt64Value(UInt16 index, UInt64 value);
		/// Sets a 32 bit float value in the field list.
		/// @param index The field index to update.
		/// @param value The new value to set.
		void setFloatValue(UInt16 index, float value);
		/// Adds a flag to a 32 bit field.
		/// @param index The field index to update.
		/// @param flag The flags to add.
		void addFlag(UInt16 index, UInt32 flag);
		/// Removes a flag from a 32 bit field.
		/// @param index The field index to update.
		/// @param flag The flags to add.
		void removeFlag(UInt16 index, UInt32 flag);
		/// 
		void markForUpdate();
		/// Marks a field as changed even if it's value ramains the same. Useful for dynamic fields which have
		/// values depending on the receiver.
		void forceFieldUpdate(UInt16 index);
		/// Updates the objects guid. Should be used only for initializing this object. Effectively just
		/// a shortcut to setUInt64Value.
		/// TODO: Maybe put this into the initialize method.
		void setGuid(UInt64 guid) {
			setUInt64Value(object_fields::Guid, guid);
		}
		/// Gets the objects guid. Just a shortcut to getUInt64Value.
		UInt64 getGuid() const {
			return getUInt64Value(object_fields::Guid);
		}
		/// Determines whether players can accept a specific quest from this object.
		/// This method mainly exists so that lesser casts have to be done.
		virtual bool providesQuest(UInt32 questId) const {
			return false;
		}
		/// Determines whether this object rewards players for completing a specific quest.
		virtual bool endsQuest(UInt32 questId) const {
			return false;
		}
		/// Determines whether this object is creature.
		bool isCreature() const {
			return getTypeId() == object_type::Unit;
		}
		/// Determines whether this object is a world object like a chest for example.
		bool isWorldObject() const {
			return getTypeId() == object_type::GameObject;
		}
		/// Determines whether this object is a player character.
		bool isGameCharacter() const {
			return getTypeId() == object_type::Character;
		}
		/// Writes creation blocks for this object.
		/// @param out_blocks The block list to add spawn blocks to.
		/// @param creation Whether the creation blocks should be added. If this value is true, all fields will
		///                 be included. Otherwise, only updated values will be included.
		virtual void writeCreateObjectBlocks(std::vector<std::vector<char>> &out_blocks, bool creation = true) const;
		/// Writes the value update block of this game object.
		/// @param writer The writer object used to write the values.
		/// @param creation Set to true if this is the value update packet used at object creation.
		///	       At object creation, all values which aren't equal to zero will be written, not just the
		///	       updated ones.
		virtual void writeValueUpdateBlock(io::Writer &writer, GameCharacter &receiver, bool creation = true) const;
		/// Writes a field value to a binary writer. This method is used because some fields have dynamic values, which
		/// depend on the receiver.
		/// @param writer The writer to write to.
		/// @param receiver The character of the player who will receive the update packet.
		/// @param index Index of the field to write.
		void writeUpdateValue(io::Writer &writer, GameCharacter &receiver, UInt16 index) const;
		/// Gets the targets location.
		const math::Vector3 &getLocation() const {
			return m_position;
		}
		/// Gets the targets orientation (yaw value in radians).
		float getOrientation() const {
			return m_o;
		}
		/// Gets the map id of this object.
		UInt32 getMapId() const {
			return m_mapId;
		}
		/// Gets the unit's tile index in the world grid.
		bool getTileIndex(TileIndex2D &out_index) const;
		/// Moves the object to the given position on it's map id.
		virtual void relocate(math::Vector3 position, float o, bool fire = true);
		/// Updates the orientation of this object.
		void setOrientation(float o);
		/// Updates the map id of this object.
		void setMapId(UInt32 mapId);
		/// Gets the object type id value.
		virtual ObjectType getTypeId() const {
			return object_type::Object;
		}
		/// Determines whether the object has been updated.
		bool wasUpdated() const {
			return m_updated;
		}
		/// Marks all changed values of this object as unchanged.
		void clearUpdateMask();
		/// Calculates the distance of this object to another object.
		/// @param other The destination object, whose location will be used.
		/// @param use3D If true, the distance will be calculated using 3d coordinates. Otherwise,
		///              only 2d coordinates are used (x,y).
		float getDistanceTo(const GameObject &other, bool use3D = true) const;
		/// Calculates the distance of this object to a specific location.
		/// @param position The destination location.
		/// @param use3D If true, the distance will be caluclated using 3d coordinates. Otherwise,
		///              only 2d coordinates are used (x,y).
		float getDistanceTo(const math::Vector3 &position, bool use3D = true) const;
		/// Gets the angle (in radians) which can be used to make this object look at another
		/// object using the setOrientation method.
		/// @param other The target object to look at.
		/// @returns Angle in radians to make this object look at another object.
		float getAngle(GameObject &other) const;
		/// Gets the angle (in radians) to make this object look at a given position (in 2d).
		/// 
		float getAngle(float x, float y) const;

		const MovementInfo &getMovementInfo() {
			return m_movementInfo;
		}
		void setMovementInfo(const MovementInfo &info) {
			m_movementInfo = info;
		}

		/// Gets the world instance of this object. May be nullptr, if the object is
		/// not in any world.
		WorldInstance *getWorldInstance() {
			return m_worldInstance;
		}
		/// Sets the world instance of this object. nullptr is valid here, if the object
		/// is not in any world.
		void setWorldInstance(WorldInstance *instance);

		///
		bool isInArc(float arcRadian, float x, float y) const;
		/// Gets the data project.
		proto::Project &getProject() const {
			return m_project;
		}

		/// Determines whether this object can be seen by a target at all.
		/// This is used for spirit healers which are only visible to dead units.
		virtual bool canSpawnForCharacter(GameCharacter &target);

	protected:

		void onWorldInstanceDestroyed();

	protected:

		proto::Project &m_project;		// TODO: Maybe move this, but right now, it's comfortable to use this
		std::vector<UInt32> m_values;
		std::vector<UInt32> m_valueBitset;
		UInt32 m_mapId;
		math::Vector3 m_position;
		math::Vector3 m_lastFiredPosition;	// This is needed for move signal
		float m_o;
		UInt32 m_objectType;
		UInt32 m_objectTypeId;
		bool m_updated;
		MovementInfo m_movementInfo;
		WorldInstance *m_worldInstance;
		boost::signals2::scoped_connection m_worldInstanceDestroyed;
	};

	io::Writer &operator << (io::Writer &w, GameObject const &object);
	io::Reader &operator >> (io::Reader &r, GameObject &object);

	void createUpdateBlocks(GameObject &object, GameCharacter &receiver, std::vector<std::vector<char>> &out_blocks);
}
