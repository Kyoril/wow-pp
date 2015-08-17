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

#include "game_world_object.h"
#include "log/default_log_levels.h"
#include "binary_io/vector_sink.h"
#include "common/clock.h"
#include "visibility_tile.h"
#include "world_instance.h"
#include <cassert>

namespace wowpp
{
	WorldObject::WorldObject(
		TimerQueue &timers,
		const ObjectEntry &entry)
		: GameObject()
		, m_timers(timers)
		, m_entry(entry)
	{
		m_values.resize(world_object_fields::WorldObjectFieldCount, 0);
		m_valueBitset.resize((world_object_fields::WorldObjectFieldCount + 31) / 32, 0);
	}

	WorldObject::~WorldObject()
	{
	}

	void WorldObject::initialize()
	{
		GameObject::initialize();

		setUInt32Value(world_object_fields::TypeID, m_entry.type);
		setUInt32Value(world_object_fields::DisplayId, m_entry.displayID);
		setUInt32Value(world_object_fields::AnimProgress, 100);
		setUInt32Value(world_object_fields::State, 1);
		setUInt32Value(world_object_fields::Faction, m_entry.factionID);
		setUInt32Value(world_object_fields::Flags, m_entry.flags);
	}

	void WorldObject::writeCreateObjectBlocks(std::vector<std::vector<char>> &out_blocks, bool creation /*= true*/) const
	{
		// TODO
	}

	void WorldObject::relocate(float x, float y, float z, float o)
	{
		setFloatValue(world_object_fields::PosX, x);
		setFloatValue(world_object_fields::PosY, y);
		setFloatValue(world_object_fields::PosZ, z);
		setFloatValue(world_object_fields::Facing, o);
		setFloatValue(world_object_fields::Rotation + 2, sin(o / 2.0f));
		setFloatValue(world_object_fields::Rotation + 3, cos(o / 2.0f));

		GameObject::relocate(x, y, z, o);
	}


	io::Writer & operator<<(io::Writer &w, WorldObject const& object)
	{
		// Write the bitset and values
		return w
			<< reinterpret_cast<GameObject const&>(object);
	}

	io::Reader & operator>>(io::Reader &r, WorldObject& object)
	{
		// Read the bitset and values
		return r
			>> reinterpret_cast<GameObject &>(object);
	}
}
