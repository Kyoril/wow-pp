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

#include "pch.h"
#include "game_dyn_object.h"
#include "log/default_log_levels.h"
#include "binary_io/vector_sink.h"
#include "common/clock.h"
#include "visibility_tile.h"
#include "world_instance.h"
#include "universe.h"
#include "proto_data/project.h"
#include "game_character.h"
#include "common/make_unique.h"

namespace wowpp
{
	DynObject::DynObject(
	    proto::Project &project,
	    TimerQueue &timers,
		GameUnit &caster,
		const proto::SpellEntry &entry,
		const proto::SpellEffect &effect)
		: GameObject(project)
		, m_timers(timers)
		, m_caster(caster)
		, m_entry(entry)
		, m_effect(effect)
		, m_despawnTimer(timers)
	{
		m_values.resize(dyn_object_fields::DynObjectFieldCount, 0);
		m_valueBitset.resize((dyn_object_fields::DynObjectFieldCount + 31) / 32, 0);

		m_onDespawn = m_despawnTimer.ended.connect([this]()
		{
			m_caster.removeDynamicObject(getGuid());
		});
	}

	DynObject::~DynObject()
	{
	}

	void DynObject::initialize()
	{
		GameObject::initialize();

		setUInt32Value(object_fields::Type, 65);
		setUInt32Value(object_fields::Entry, m_entry.id());
		setFloatValue(object_fields::ScaleX, 1.0f);

		auto &loc = getLocation();
		setUInt64Value(dyn_object_fields::Caster, m_caster.getGuid());
		setUInt32Value(dyn_object_fields::Bytes, 0x00000001);
		setUInt32Value(dyn_object_fields::SpellId, m_entry.id());
		setFloatValue(dyn_object_fields::Radius, m_effect.radius());
		setFloatValue(dyn_object_fields::PosX, loc.x);
		setFloatValue(dyn_object_fields::PosY, loc.y);
		setFloatValue(dyn_object_fields::PosZ, loc.z);
		setFloatValue(dyn_object_fields::Facing, 0.0f);
		setUInt32Value(dyn_object_fields::CastTime, TimeStamp());

		// TODO: Also need to toggle despawn timer?
	}

	void DynObject::relocate(const math::Vector3 & position, float o, bool fire)
	{
		GameObject::relocate(position, o, fire);

		setFloatValue(dyn_object_fields::PosX, position.x);
		setFloatValue(dyn_object_fields::PosY, position.y);
		setFloatValue(dyn_object_fields::PosZ, position.z);
		setFloatValue(dyn_object_fields::Facing, o);
	}

	void DynObject::writeCreateObjectBlocks(std::vector<std::vector<char>> &out_blocks, bool creation /*= true*/) const
	{
		// TODO
	}

	void DynObject::triggerDespawnTimer(UInt64 duration)
	{
		m_despawnTimer.setEnd(getCurrentTime() + duration);
	}

	io::Writer &operator<<(io::Writer &w, DynObject const &object)
	{
		// Write the bitset and values
		return w
		       << reinterpret_cast<GameObject const &>(object);
	}

	io::Reader &operator>>(io::Reader &r, DynObject &object)
	{
		// Read the bitset and values
		return r
		       >> reinterpret_cast<GameObject &>(object);
	}
}
