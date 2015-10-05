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

#include "game_object.h"
#include "game_character.h"
#include "game_creature.h"
#include "log/default_log_levels.h"
#include "binary_io/vector_sink.h"
#include "common/clock.h"
#include "visibility_tile.h"
#include "world_instance.h"
#include <cassert>

namespace wowpp
{
	GameObject::GameObject()
		: m_mapId(0)
		, m_x(0.0f)
		, m_y(0.0f)
		, m_z(0.0f)
		, m_o(0.0f)
		, m_objectType(0x01)
		, m_objectTypeId(0x01)
		, m_worldInstance(nullptr)
	{
		m_values.resize(object_fields::ObjectFieldCount, 0);
		m_valueBitset.resize((object_fields::ObjectFieldCount + 31) / 32, 0);
	}

	GameObject::~GameObject()
	{
	}

	void GameObject::initialize()
	{
		// TODO
	}

	wowpp::UInt8 GameObject::getByteValue(UInt16 index, UInt8 offset) const
	{
		assert(offset < 4);
		assert(index < m_values.size());
		return *((reinterpret_cast<const UInt8*>(&m_values[index])) + offset);
	}

	wowpp::UInt16 GameObject::getUInt16Value(UInt16 index, UInt8 offset) const
	{
		assert(offset < 2);
		assert(index < m_values.size());
		return *((reinterpret_cast<const UInt16*>(&m_values[index])) + offset);
	}

	wowpp::Int32 GameObject::getInt32Value(UInt16 index) const
	{
		assert(index < m_values.size());
		return *(reinterpret_cast<const Int32*>(&m_values[index]));
	}

	wowpp::UInt32 GameObject::getUInt32Value(UInt16 index) const
	{
		assert(index < m_values.size());
		return m_values[index];
	}

	wowpp::UInt64 GameObject::getUInt64Value(UInt16 index) const
	{
		assert(index + 1 < m_values.size());
		return *(reinterpret_cast<const UInt64*>(&m_values[index]));
	}

	float GameObject::getFloatValue(UInt16 index) const
	{
		assert(index < m_values.size());
		return *(reinterpret_cast<const float*>(&m_values[index]));
	}

	void GameObject::setByteValue(UInt16 index, UInt8 offset, UInt8 value)
	{
		assert(index < m_values.size());
		assert(offset < 4);

		if (UInt8(m_values[index] >> (offset * 8)) != value)
		{
			m_values[index] &= ~UInt32(UInt32(0xFF) << (offset * 8));
			m_values[index] |= UInt32(UInt32(value) << (offset * 8));

			UInt16 bitIndex = index >> 3;

			// Mark bit as changed
			UInt8 &changed = (reinterpret_cast<UInt8*>(&m_valueBitset[0]))[bitIndex];
			changed |= 1 << (index & 0x7);

			m_updated = true;
		}
	}

	void GameObject::setUInt16Value(UInt16 index, UInt8 offset, UInt16 value)
	{
		assert(index < m_values.size());
		assert(offset < 2);

		if (UInt16(m_values[index] >> (offset * 16)) != value)
		{
			m_values[index] &= ~UInt32(UInt32(0xFFFF) << (offset * 16));
			m_values[index] |= UInt32(UInt32(value) << (offset * 16));

			UInt16 bitIndex = index >> 3;

			// Mark bit as changed
			UInt8 &changed = (reinterpret_cast<UInt8*>(&m_valueBitset[0]))[bitIndex];
			changed |= 1 << (index & 0x7);

			m_updated = true;
		}
	}

	void GameObject::setUInt32Value(UInt16 index, UInt32 value)
	{
		assert(index < m_values.size());

		if (m_values[index] != value)
		{
			// Update value
			m_values[index] = value;

			UInt16 bitIndex = index >> 3;

			// Mark bit as changed
			UInt8 &changed = (reinterpret_cast<UInt8*>(&m_valueBitset[0]))[bitIndex];
			changed |= 1 << (index & 0x7);

			m_updated = true;
		}
	}

	void GameObject::addFlag(UInt16 index, UInt32 flag)
	{
		assert(index < m_values.size());

		UInt32 newValue = m_values[index] | flag;
		if (newValue != m_values[index])
		{
			m_values[index] = newValue;

			UInt16 bitIndex = index >> 3;

			// Mark bit as changed
			UInt8 &changed = (reinterpret_cast<UInt8*>(&m_valueBitset[0]))[bitIndex];
			changed |= 1 << (index & 0x7);

			m_updated = true;
		}
	}

	void GameObject::removeFlag(UInt16 index, UInt32 flag)
	{
		assert(index < m_values.size());

		UInt32 newValue = m_values[index] & ~flag;
		if (newValue != m_values[index])
		{
			m_values[index] = newValue;
			UInt16 bitIndex = index >> 3;

			// Mark bit as changed
			UInt8 &changed = (reinterpret_cast<UInt8*>(&m_valueBitset[0]))[bitIndex];
			changed |= 1 << (index & 0x7);

			m_updated = true;
		}
	}

	void GameObject::setInt32Value(UInt16 index, Int32 value)
	{
		assert(index < m_values.size());
		
		Int32* tmp = reinterpret_cast<Int32*>(&m_values[index]);
		if (*tmp != value)
		{
			// Update value
			*tmp = value;

			UInt16 bitIndex = index >> 3;

			// Mark bit as changed
			UInt8 &changed = (reinterpret_cast<UInt8*>(&m_valueBitset[0]))[bitIndex];
			changed |= 1 << (index & 0x7);

			m_updated = true;
		}
	}

	void GameObject::setUInt64Value(UInt16 index, UInt64 value)
	{
		assert(index + 1 < m_values.size());

		UInt64* tmp = reinterpret_cast<UInt64*>(&m_values[index]);
		if (*tmp != value)
		{
			setUInt32Value(index, *(reinterpret_cast<UInt32*>(&value)));
			setUInt32Value(index + 1, *(reinterpret_cast<UInt32*>(&value) + 1));
		}
	}

	void GameObject::setFloatValue(UInt16 index, float value)
	{
		assert(index < m_values.size());

		float* tmp = reinterpret_cast<float*>(&m_values[index]);
		if (*tmp != value)
		{
			// Update value
			*tmp = value;

			UInt16 bitIndex = index >> 3;

			// Mark bit as changed
			UInt8 &changed = (reinterpret_cast<UInt8*>(&m_valueBitset[0]))[bitIndex];
			changed |= 1 << (index & 0x7);

			m_updated = true;
		}
	}

	void GameObject::relocate(float x, float y, float z, float o)
	{
		float oldX = m_x, oldY = m_y, oldZ = m_z, oldO = m_o;

		m_x = x;
		m_y = y;
		m_z = z;
		m_o = o;

		moved(*this, oldX, oldY, oldZ, oldO);
	}

	void GameObject::setOrientation(float o)
	{
		float oldO = m_o;

		m_o = o;

		moved(*this, m_x, m_y, m_z, oldO);
	}

	void GameObject::setMapId(UInt32 mapId)
	{
		m_mapId = mapId;
	}

	void GameObject::writeCreateObjectBlocks(std::vector<std::vector<char>> &out_blocks, bool creation /*= true*/) const
	{
		// TODO
	}

	void GameObject::writeUpdateValue(io::Writer &writer, GameCharacter &receiver, UInt16 index) const
	{
		if (getTypeId() == object_type::Unit)
		{
			switch (index)
			{
			case unit_fields::DynamicFlags:
			{
				const GameCreature *creature = static_cast<const GameCreature*>(this);
				if (!creature->isLootRecipient(receiver))
				{
					writer << io::write<NetUInt32>(m_values[index] & ~game::unit_dynamic_flags::Lootable);
				}
				else
				{
					writer << io::write<NetUInt32>(m_values[index] & ~game::unit_dynamic_flags::OtherTagger);
				}
				break;
			}
			default:
				writer << io::write<NetUInt32>(m_values[index]);
				break;
			}
		}
		else
		{
			writer << io::write<NetUInt32>(m_values[index]);
		}
	}

	void GameObject::writeValueUpdateBlock(io::Writer &writer, GameCharacter &receiver, bool creation /*= true*/) const
	{
		// Number of UInt32 blocks used to represent all values as one bit
		UInt8 blockCount = m_valueBitset.size();
		writer
			<< io::write<NetUInt8>(blockCount);

		if (creation)
		{
			// Create a new bitset which has set all bits to one where the field value
			// isn't equal to 0, since this will be for a CREATE_OBJECT block (spawn) and not
			// for an UPDATE_OBJECT block
			std::vector<UInt32> creationBits(m_valueBitset.size(), 0);
			for (size_t i = 0; i < m_values.size(); ++i)
			{
				if (m_values[i])
				{
					// Calculate bit index
					UInt16 bitIndex = i >> 3;

					// Mark bit as changed
					UInt8 &changed = (reinterpret_cast<UInt8*>(&creationBits[0]))[bitIndex];
					changed |= 1 << (i & 0x7);
				}
			}

			// Write only the changed bits
			writer
				<< io::write_range(creationBits);

			// Write all values != zero
			for (size_t i = 0; i < m_values.size(); ++i)
			{
				if (m_values[i] != 0)
				{
					writeUpdateValue(writer, receiver, i);
				}
			}
		}
		else
		{
			// Write only the changed bits
			writer
				<< io::write_range(m_valueBitset);

			// Write all values marked as changed
			for (size_t i = 0; i < m_values.size(); ++i)
			{
				const UInt8 &changed = (reinterpret_cast<const UInt8*>(&m_valueBitset[0]))[i >> 3];
				if ((changed & (1 << (i & 0x7))) != 0)
				{
					writeUpdateValue(writer, receiver, i);
				}
			}
		}
	}

	void GameObject::clearUpdateMask()
	{
		std::fill(m_valueBitset.begin(), m_valueBitset.end(), 0);
		m_updated = false;
	}

	float GameObject::getDistanceTo(GameObject &other, bool use3D/* = true*/) const
	{
		if (&other == this)
			return 0.0f;

		float o;
		game::Position position;
		other.getLocation(position[0], position[1], position[2], o);

		return getDistanceTo(position, use3D);
	}

	float GameObject::getDistanceTo(const game::Position &position, bool use3D /*= true*/) const
	{
		if (use3D)
			return (sqrtf(((position[0] - m_x) * (position[0] - m_x)) + ((position[1] - m_y) * (position[1] - m_y)) + ((position[2] - m_z) * (position[2] - m_z))));
		else
			return (sqrtf(((position[0] - m_x) * (position[0] - m_x)) + ((position[1] - m_y) * (position[1] - m_y))));
	}

	void GameObject::onWorldInstanceDestroyed()
	{
		// We are no longer part of the old world instance
		setWorldInstance(nullptr);
	}

	void GameObject::setWorldInstance(WorldInstance *instance)
	{
		m_worldInstanceDestroyed.disconnect();

		// Use new instance
		m_worldInstance = instance;

		// Watch for destruction of the new world instance
		if (m_worldInstance)
		{
			m_worldInstanceDestroyed = m_worldInstance->willBeDestroyed.connect(
				std::bind(&GameObject::onWorldInstanceDestroyed, this));
		}
	}

	bool GameObject::getTileIndex(TileIndex2D &out_index) const
	{
		// This object has to be in a world instance
		if (!m_worldInstance)
		{
			return false;
		}

		// Try to resolve the objects position
		return m_worldInstance->getGrid().getTilePosition(m_x, m_y, m_z, out_index[0], out_index[1]);
	}

	bool GameObject::isInArc(float arcRadian, float x, float y) const
	{
		if (x == m_x && y == m_y)
			return true;

		const float PI = 3.1415927f;
		float arc = arcRadian;

		// move arc to range 0.. 2*pi
		while (arc >= 2.0f * PI)
			arc -= 2.0f * PI;
		while (arc < 0)
			arc += 2.0f * PI;

		float angle = getAngle(x, y);
		angle -= m_o;

		// move angle to range -pi ... +pi
		while (angle > PI)
			angle -= 2.0f * PI;
		while (angle < -PI)
			angle += 2.0f * PI;

		float lborder = -1 * (arc / 2.0f);                     // in range -pi..0
		float rborder = (arc / 2.0f);                           // in range 0..pi
		return ((angle >= lborder) && (angle <= rborder));
	}

	float GameObject::getAngle(GameObject &other) const
	{
		return getAngle(other.m_x, other.m_y);
	}

	float GameObject::getAngle(float x, float y) const
	{
		float dx = x - m_x;
		float dy = y - m_y;

		float ang = ::atan2(dy, dx);
		ang = (ang >= 0) ? ang : 2 * 3.1415927f + ang;
		return ang;
	}

	io::Writer & operator<<(io::Writer &w, GameObject const& object)
	{
		// Write the bitset and values
		return w
			<< io::write_dynamic_range<NetUInt8>(object.m_valueBitset)
			<< io::write_dynamic_range<NetUInt16>(object.m_values)
			<< io::write<NetUInt32>(object.m_mapId)
			<< io::write<float>(object.m_x)
			<< io::write<float>(object.m_y)
			<< io::write<float>(object.m_z)
			<< io::write<float>(object.m_o);
	}

	io::Reader & operator>>(io::Reader &r, GameObject& object)
	{
		// Read the bitset and values
		return r
			>> io::read_container<NetUInt8>(object.m_valueBitset)
			>> io::read_container<NetUInt16>(object.m_values)
			>> io::read<NetUInt32>(object.m_mapId)
			>> io::read<float>(object.m_x)
			>> io::read<float>(object.m_y)
			>> io::read<float>(object.m_z)
			>> io::read<float>(object.m_o);
	}

	void createUpdateBlocks(GameObject &object, GameCharacter &receiver, std::vector<std::vector<char>> &out_blocks)
	{
		float x, y, z, o;
		object.getLocation(x, y, z, o);

		// Write create object packet
		std::vector<char> createBlock;
		io::VectorSink sink(createBlock);
		io::Writer writer(sink);
		{
			UInt8 updateType = 0x02;						// Update type (0x02 = CREATE_OBJECT)
			if (object.getTypeId() == object_type::Character ||
				object.getTypeId() == object_type::Corpse ||
				object.getTypeId() == object_type::DynamicObject ||
				object.getTypeId() == object_type::Container)
			{
				updateType = 0x03;		// CREATE_OBJECT_2
			}
			if (object.getTypeId() == object_type::GameObject)
			{
				UInt32 objType = object.getUInt32Value(object_fields::ObjectFieldCount + 0x0F);
				switch (objType)
				{
				case 6:
				case 16:
				case 24:
				case 26:
					updateType = 0x03;		// CREATE_OBJECT_2
					break;
				default:
					break;
				}
			}
			UInt8 updateFlags = 0x10 | 0x20 | 0x40;			// UPDATEFLAG_ALL | UPDATEFLAG_LIVING | UPDATEFLAG_HAS_POSITION
			UInt8 objectTypeId = object.getTypeId();		// 
			if (objectTypeId == object_type::GameObject)
			{
				updateFlags = 8 | 16 | 64;
			}

			// Header with object guid and type
			UInt64 guid = object.getGuid();
			writer
				<< io::write<NetUInt8>(updateType);

			UInt64 guidCopy = guid;
			UInt8 packGUID[8 + 1];
			packGUID[0] = 0;
			size_t size = 1;
			for (UInt8 i = 0; guidCopy != 0; ++i)
			{
				if (guidCopy & 0xFF)
				{
					packGUID[0] |= UInt8(1 << i);
					packGUID[size] = UInt8(guidCopy & 0xFF);
					++size;
				}

				guidCopy >>= 8;
			}
			writer.sink().write((const char*)&packGUID[0], size);
			writer
				<< io::write<NetUInt8>(objectTypeId)
				<< io::write<NetUInt8>(updateFlags);
			
			// Write movement update
			auto &movement = object.getMovementInfo();
			UInt32 moveFlags = movement.moveFlags;
			if (updateFlags & 0x20)
			{
				writer
					<< io::write<NetUInt32>(moveFlags)
					<< io::write<NetUInt8>(0x00)
					<< io::write<NetUInt32>(getCurrentTime());
			}

			if (updateFlags & 0x40)
			{
				// Position & Rotation
				// TODO: Calculate position and rotation from the movement info using interpolation
				writer
					<< io::write<float>(x)
					<< io::write<float>(y)
					<< io::write<float>(z)
					<< io::write<float>(o);
			}

			if (updateFlags & 0x20)
			{
				// Transport
				if (moveFlags & (game::movement_flags::OnTransport))
				{
					//TODO
					WLOG("TODO");
				}

				// Pitch info
				if (moveFlags & (game::movement_flags::Swimming | game::movement_flags::Flying2))
				{
					if (object.getTypeId() == object_type::Character)
					{
						writer
							<< io::write<float>(movement.pitch);
					}
					else
					{
						writer
							<< io::write<float>(0.0f);
					}
				}

				// Fall time
				if (object.getTypeId() == object_type::Character)
				{
					writer
						<< io::write<NetUInt32>(movement.fallTime);
				}
				else
				{
					writer
						<< io::write<NetUInt32>(0);
				}

				// Fall information
				if (moveFlags & game::movement_flags::Falling)
				{
					if (object.getTypeId() == object_type::Character)
					{
						writer
							<< io::write<float>(movement.jumpVelocity)
							<< io::write<float>(movement.jumpSinAngle)
							<< io::write<float>(movement.jumpCosAngle)
							<< io::write<float>(movement.jumpXYSpeed);
					}
					else
					{
						writer
							<< io::write<float>(0.0f)
							<< io::write<float>(0.0f)
							<< io::write<float>(0.0f)
							<< io::write<float>(0.0f);
					}
				}

				// Elevation information
				if (moveFlags & game::movement_flags::SplineElevation)
				{
					if (object.getTypeId() == object_type::Character)
					{
						writer
							<< io::write<float>(movement.unknown1);
					}
					else
					{
						writer
							<< io::write<float>(0.0f);
					}
				}

				// TODO: Speed values
				writer
					<< io::write<float>(2.5f)				// Walk
					<< io::write<float>(7.0f)				// Run
					<< io::write<float>(4.5f)				// Backwards
					<< io::write<NetUInt32>(0x40971c71)		// Swim
					<< io::write<NetUInt32>(0x40200000)		// Swim Backwards
					<< io::write<float>(7.0f)				// Fly
					<< io::write<float>(4.5f)				// Fly Backwards
					<< io::write<float>(3.1415927);			// Turn (radians / sec: PI)
			}

			// Lower-GUID update?
			if (updateFlags & 0x08)
			{
				writer
					<< io::write<NetUInt32>(0x00);// guidLowerPart(guid));
			}

			// High-GUID update?
			if (updateFlags & 0x10)
			{
				/*switch (objectTypeId)
				{
				case object_type::Object:
				case object_type::Item:
				case object_type::Container:
				case object_type::GameObject:
				case object_type::DynamicObject:
				case object_type::Corpse:
					writer
						<< io::write<NetUInt32>((guid >> 48) & 0x0000FFFF);
					break;
				default:*/
					writer
						<< io::write<NetUInt32>(0);
				//}
			}

			// Write values update
			object.writeValueUpdateBlock(writer, receiver, true);
		}

		// Add block
		out_blocks.push_back(createBlock);
	}
}
