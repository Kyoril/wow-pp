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
#include "game_world_object.h"
#include "log/default_log_levels.h"
#include "binary_io/vector_sink.h"
#include "common/clock.h"
#include "visibility_tile.h"
#include "world_instance.h"
#include "proto_data/project.h"
#include "game_character.h"
#include "common/make_unique.h"

namespace wowpp
{
	WorldObject::WorldObject(
	    proto::Project &project,
	    TimerQueue &timers,
	    const proto::ObjectEntry &entry)
		: GameObject(project)
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

		setUInt32Value(object_fields::Type, 33);
		setUInt32Value(object_fields::Entry, m_entry.id());

		setUInt32Value(world_object_fields::TypeID, m_entry.type());
		setUInt32Value(world_object_fields::DisplayId, m_entry.displayid());
		setUInt32Value(world_object_fields::AnimProgress, 100);
		setUInt32Value(world_object_fields::State, 1);
		setUInt32Value(world_object_fields::Faction, m_entry.factionid());
		setUInt32Value(world_object_fields::Flags, m_entry.flags());

		float o = getOrientation();
		math::Vector3 location(getLocation());

		setFloatValue(world_object_fields::PosX, location.x);
		setFloatValue(world_object_fields::PosY, location.y);
		setFloatValue(world_object_fields::PosZ, location.z);
		setFloatValue(world_object_fields::Facing, o);
		setFloatValue(world_object_fields::Rotation + 2, sin(o / 2.0f));
		setFloatValue(world_object_fields::Rotation + 3, cos(o / 2.0f));

		// Generate object loot
		generateObjectLoot();
	}

	void WorldObject::writeCreateObjectBlocks(std::vector<std::vector<char>> &out_blocks, bool creation /*= true*/) const
	{
		// TODO
	}

	bool WorldObject::providesQuest(UInt32 questId) const
	{
		auto &entry = getEntry();
		for (const auto &id : entry.quests())
		{
			if (id == questId) {
				return true;
			}
		}

		return false;
	}

	bool WorldObject::endsQuest(UInt32 questId) const
	{
		auto &entry = getEntry();
		for (const auto &id : entry.end_quests())
		{
			if (id == questId) {
				return true;
			}
		}

		return false;
	}

	game::QuestgiverStatus WorldObject::getQuestgiverStatus(const GameCharacter &character) const
	{
		game::QuestgiverStatus result = game::questgiver_status::None;
		for (const auto &quest : getEntry().end_quests())
		{
			auto questStatus = character.getQuestStatus(quest);
			if (questStatus == game::quest_status::Complete)
			{
				return game::questgiver_status::Reward;
			}
			else if (questStatus == game::quest_status::Incomplete)
			{
				result = game::questgiver_status::Incomplete;
			}
		}

		for (const auto &quest : getEntry().quests())
		{
			auto questStatus = character.getQuestStatus(quest);
			if (questStatus == game::quest_status::Available)
			{
				return game::questgiver_status::Available;
			}
		}
		return result;
	}

	bool WorldObject::isQuestObject(const GameCharacter &character) const
	{
		switch (m_entry.type())
		{
		case world_object_type::Chest:
			{
				if (!m_objectLoot)
				{
					return false;
				}

				for (UInt32 i = 0; i < m_objectLoot->getItemCount(); ++i)
				{
					auto *item = m_objectLoot->getLootDefinition(i);
					if (item &&
					        !item->isLooted)
					{
						if (character.needsQuestItem(item->definition.item()))
						{
							return true;
						}
					}
				}

				return false;
			}
		case world_object_type::Goober:
			{
				return character.getQuestStatus(m_entry.data(1)) == game::QuestStatus::Incomplete;
			}
		}

		return false;
	}

	void WorldObject::raiseTrigger(trigger_event::Type e)
	{
		for (const auto &triggerId : m_entry.triggers())
		{
			const auto *triggerEntry = getProject().triggers.getById(triggerId);
			if (triggerEntry)
			{
				for (const auto &triggerEvent : triggerEntry->events())
				{
					if (triggerEvent == e)
					{
						objectTrigger(std::cref(*triggerEntry), std::ref(*this));
					}
				}
			}
		}
	}

	void WorldObject::generateObjectLoot()
	{
		auto lootEntryId = m_entry.objectlootentry();
		if (lootEntryId)
		{
			const auto *lootEntry = getProject().objectLoot.getById(lootEntryId);
			if (lootEntry)
			{
				// TODO: make a way so we don't need loot recipients for game objects as this is completely crap
				std::vector<GameCharacter *> lootRecipients;
				m_objectLoot = make_unique<LootInstance>(
				                   getProject().items, getGuid(), lootEntry, 0, 0, std::cref(lootRecipients));
				if (m_objectLoot)
				{
					m_onLootCleared = m_objectLoot->cleared.connect([this]()
					{
						// Remove this object from the world (despawn it)
						// REMEMBER: THIS WILL MOST LIKELY DESTROY THIS INSTANCE
						auto *world = getWorldInstance();
						if (world)
						{
							// Despawn this object
							world->removeGameObject(*this);
						}
					});
				}
			}
		}
	}

	void WorldObject::relocate(math::Vector3 position, float o, bool fire/* = false*/)
	{
		setFloatValue(world_object_fields::PosX, position.x);
		setFloatValue(world_object_fields::PosY, position.y);
		setFloatValue(world_object_fields::PosZ, position.z);
		setFloatValue(world_object_fields::Facing, o);
		setFloatValue(world_object_fields::Rotation + 2, sin(o / 2.0f));
		setFloatValue(world_object_fields::Rotation + 3, cos(o / 2.0f));

		GameObject::relocate(position, o, fire);
	}


	io::Writer &operator<<(io::Writer &w, WorldObject const &object)
	{
		// Write the bitset and values
		return w
		       << reinterpret_cast<GameObject const &>(object);
	}

	io::Reader &operator>>(io::Reader &r, WorldObject &object)
	{
		// Read the bitset and values
		return r
		       >> reinterpret_cast<GameObject &>(object);
	}
}
