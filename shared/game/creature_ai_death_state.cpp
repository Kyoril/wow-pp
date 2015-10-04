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

#include "creature_ai_death_state.h"
#include "creature_ai.h"
#include "game_creature.h"
#include "data/trigger_entry.h"
#include "loot_instance.h"
#include "log/default_log_levels.h"
#include "common/make_unique.h"

namespace wowpp
{
	CreatureAIDeathState::CreatureAIDeathState(CreatureAI &ai)
		: CreatureAIState(ai)
	{
	}

	CreatureAIDeathState::~CreatureAIDeathState()
	{

	}

	void CreatureAIDeathState::onEnter()
	{
		auto &controlled = getControlled();

		// Raise OnKilled trigger
		auto it = controlled.getEntry().triggersByEvent.find(trigger_event::OnKilled);
		if (it != controlled.getEntry().triggersByEvent.end())
		{
			for (const auto *trigger : it->second)
			{
				trigger->execute(*trigger, &controlled);
			}
		}

		// Reward all loot recipients
		if (controlled.isTagged())
		{
			// Reward all loot recipients if we can still find them (no disconnect etc.)
			std::vector<GameCharacter*> lootRecipients;
			controlled.forEachLootRecipient([&lootRecipients](GameCharacter &character)
			{
				lootRecipients.push_back(&character);
			});

			// Reward the killer with experience points
			const float t =
				(controlled.getEntry().maxLevel != controlled.getEntry().minLevel) ?
				(controlled.getLevel() - controlled.getEntry().minLevel) / (controlled.getEntry().maxLevel - controlled.getEntry().minLevel) :
				0.0f;

			// Base XP for equal level
			UInt32 xp = interpolate(controlled.getEntry().xpMin, controlled.getEntry().xpMax, t);

			// TODO: Level adjustment factor
			
			// Group xp modifier
			float groupModifier = 1.0f;
			if (lootRecipients.size() == 3)
			{
				groupModifier = 1.166f;
			}
			else if (lootRecipients.size() == 4)
			{
				groupModifier = 1.3f;
			}
			else if (lootRecipients.size() == 5)
			{
				groupModifier = 1.4f;
			}

			xp = (xp / lootRecipients.size()) * groupModifier;
			for (auto *character : lootRecipients)
			{
				character->rewardExperience(&controlled, xp);
			}

			// Make creature lootable
			auto &entry = controlled.getEntry();

			// Loot callback functions
			auto onCleared = [&controlled]()
			{
				controlled.removeFlag(unit_fields::DynamicFlags, game::unit_dynamic_flags::Lootable);
			};

			// Generate loot
			auto loot = make_unique<LootInstance>(controlled.getGuid(), entry.unitLootEntry, entry.minLootGold, entry.maxLootGold);
			if (!loot->isEmpty())
			{
				m_onLootCleared = loot->cleared.connect([&controlled]()
				{
					controlled.removeFlag(unit_fields::DynamicFlags, game::unit_dynamic_flags::Lootable);
				});

				controlled.setUnitLoot(std::move(loot));
				controlled.addFlag(unit_fields::DynamicFlags, game::unit_dynamic_flags::Lootable);
			}
		}
	}

	void CreatureAIDeathState::onLeave()
	{

	}

}
