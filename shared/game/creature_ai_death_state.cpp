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
#include "loot_instance.h"
#include "log/default_log_levels.h"
#include "common/make_unique.h"
#include "proto_data/project.h"
#include "proto_data/trigger_helper.h"

namespace wowpp
{
	CreatureAIDeathState::CreatureAIDeathState(CreatureAI &ai)
		: CreatureAIState(ai)
	{
	}

	CreatureAIDeathState::~CreatureAIDeathState()
	{

	}

	namespace xp
	{
		static UInt32 getGrayLevel(UInt32 level)
		{
			if (level <= 5)
				return 0;

			if (level <= 39)
				return level - 5 - level / 10;

			if (level <= 59)
				return level - 1 - level / 5;

			return level - 9;
		}

		static UInt32 getZeroDifference(UInt32 level)
		{
			if (level < 8) return 5;
			if (level < 10) return 6;
			if (level < 12) return 7;
			if (level < 16) return 8;
			if (level < 20) return 9;
			if (level < 30) return 11;
			if (level < 40) return 12;
			if (level < 45) return 13;
			if (level < 50) return 14;
			if (level < 55) return 15;
			if (level < 60) return 16;
			return 17;
		}

		static UInt32 getXpDiff(UInt32 level)
		{
			if (level < 29)
				return 0;
			if (level == 29)
				return 1;
			if (level == 30)
				return 3;
			if (level == 31)
				return 6;

			return 5 * (level - 30);
		}

		static float getGroupXpRate(UInt32 count, bool raid)
		{
			if (raid)
			{
				return 1.0f;
			}
			else
			{
				if (count <= 2)
					return 1.0f;
				if (count == 3)
					return 1.166f;
				if (count == 4)
					return 1.3f;

				return 1.4f;
			}
		}
	}

	void CreatureAIDeathState::onEnter()
	{
		auto &controlled = getControlled();
		controlled.raiseTrigger(trigger_event::OnKilled);

		// Decide whether to despawn based on unit type
		auto &entry = controlled.getEntry();
		const bool isElite = (entry.rank() > 0 && entry.rank() < 4);
		const bool isRare = (entry.rank() == 4);

		// Calculate despawn delay for rare mobs and elite mobs
		GameTime despawnDelay = constants::OneSecond * 30;
		if (isElite || isRare) despawnDelay = constants::OneMinute * 3;

		// Reward all loot recipients
		if (controlled.isTagged())
		{
			// Reward all loot recipients if we can still find them (no disconnect etc.)
			UInt32 sum_lvl = 0;
			GameCharacter *maxLevelChar = nullptr;
			std::vector<GameCharacter*> lootRecipients;
			controlled.forEachLootRecipient([&sum_lvl, &maxLevelChar, &controlled, &lootRecipients](GameCharacter &character)
			{
				if (character.isAlive())
					sum_lvl += character.getLevel();

				const UInt32 greyLevel = xp::getGrayLevel(character.getLevel());
				if (controlled.getLevel() > greyLevel)
				{
					if (!maxLevelChar || maxLevelChar->getLevel() < character.getLevel())
					{
						maxLevelChar = &character;
					}
				}

				lootRecipients.push_back(&character);
			});

			// Reward the killer with experience points
			const float t =
				(controlled.getEntry().maxlevel() != controlled.getEntry().minlevel()) ?
				(controlled.getLevel() - controlled.getEntry().minlevel()) / (controlled.getEntry().maxlevel() - controlled.getEntry().minlevel()) :
				0.0f;

			// Base XP for equal level
			UInt32 xp = interpolate(controlled.getEntry().minlevelxp(), controlled.getEntry().maxlevelxp(), t);
			if (!maxLevelChar)
			{
				xp = 0;
			}
			else
			{
				if (controlled.getLevel() >= maxLevelChar->getLevel())
				{
					UInt32 levelDiff = controlled.getLevel() - maxLevelChar->getLevel();
					if (levelDiff > 4) levelDiff = 4;
					xp = ((maxLevelChar->getLevel() * 5 + xp) * (20 + levelDiff) / 10 + 1) / 2;
				}
				else
				{
					UInt32 grayLevel = xp::getGrayLevel(maxLevelChar->getLevel());
					if (controlled.getLevel() > grayLevel)
					{
						UInt32 zd = xp::getZeroDifference(maxLevelChar->getLevel());
						xp = (maxLevelChar->getLevel() * 5 + xp) * (zd + controlled.getLevel() - maxLevelChar->getLevel()) / zd;
					}
					else
					{
						xp = 0;
					}
				}
			}
						
			// Group xp modifier
			float groupModifier = xp::getGroupXpRate(lootRecipients.size(), false);
			for (auto *character : lootRecipients)
			{
				const UInt32 greyLevel = xp::getGrayLevel(character->getLevel());
				if (controlled.getLevel() <= greyLevel)
					continue;

				float rate = groupModifier * static_cast<float>(character->getLevel()) / sum_lvl;

				// Only alive characters will receive loot
				if (!character->isAlive())
				{
					continue;
				}

				character->rewardExperience(&controlled, xp * rate);
			}

			const auto *lootEntry = controlled.getProject().unitLoot.getById(entry.unitlootentry());

			// Generate loot
			auto loot = make_unique<LootInstance>(controlled.getProject().items, controlled.getGuid(), lootEntry, entry.minlootgold(), entry.maxlootgold());
			if (!loot->isEmpty())
			{
				// 3 Minutes of despawn delay if creature still has loot
				despawnDelay = constants::OneMinute * 3;

				m_onLootCleared = loot->cleared.connect([&controlled]()
				{
					controlled.removeFlag(unit_fields::DynamicFlags, game::unit_dynamic_flags::Lootable);

					// 30 more seconds until despawn from now on
					controlled.triggerDespawnTimer(constants::OneSecond * 30);
				});

				controlled.setUnitLoot(std::move(loot));
				controlled.addFlag(unit_fields::DynamicFlags, game::unit_dynamic_flags::Lootable);
			}
		}

		// Despawn in 30 seconds
		controlled.triggerDespawnTimer(despawnDelay);
	}

	void CreatureAIDeathState::onLeave()
	{

	}

}
