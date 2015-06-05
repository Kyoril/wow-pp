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

#include "game_object.h"
#include "log/default_log_levels.h"
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
			m_values[index] = *(reinterpret_cast<UInt32*>(&value));
			m_values[index + 1] = *(reinterpret_cast<UInt32*>(&value) + 1);
			
			// Mark bits as changed
			UInt8 &loChanged = (reinterpret_cast<UInt8*>(&m_valueBitset[0]))[index >> 3];
			loChanged |= 1 << (index & 0x7);
			UInt8 &hiChanged = (reinterpret_cast<UInt8*>(&m_valueBitset[0]))[(index + 1) >> 3];
			hiChanged |= 1 << (index & 0x7);

			m_updated = true;
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

	void GameObject::setMapId(UInt32 mapId)
	{
		m_mapId = mapId;
	}

	void GameObject::writeValueUpdateBlock(io::Writer &writer, bool creation /*= true*/) const
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
					writer
						<< io::write<NetUInt32>(m_values[i]);
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
					writer
						<< io::write<NetUInt32>(m_values[i]);
				}
			}
		}
	}

	void GameObject::clearUpdateMask()
	{
		std::fill(m_valueBitset.begin(), m_valueBitset.end(), 0);
		m_updated = false;
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

}
