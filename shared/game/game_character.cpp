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

#include "game_character.h"
#include <log/default_log_levels.h>
#include "proto_data/project.h"
#include "game_item.h"
#include "common/utilities.h"
#include "game_creature.h"
#include "defines.h"

namespace wowpp
{
	GameCharacter::GameCharacter(
		proto::Project &project,
		TimerQueue &timers)
		: GameUnit(project, timers)
		, m_name("UNKNOWN")
		, m_zoneIndex(0)
		, m_weaponProficiency(0)
		, m_armorProficiency(0)
		, m_comboTarget(0)
		, m_comboPoints(0)
		, m_healthRegBase(0.0f)
		, m_manaRegBase(0.0f)
		, m_groupUpdateFlags(group_update_flags::None)
		, m_canBlock(false)
		, m_canParry(false)
		, m_canDualWield(false)
	{
		// Resize values field
		m_values.resize(character_fields::CharacterFieldCount);
		m_valueBitset.resize((character_fields::CharacterFieldCount + 31) / 32);

		m_objectType |= type_mask::Player;
	}

	GameCharacter::~GameCharacter()
	{
	}

	void GameCharacter::initialize()
	{
		GameUnit::initialize();

		setUInt32Value(object_fields::Type, 25);					//OBJECT_FIELD_TYPE				(TODO: Flags)
		setUInt32Value(character_fields::CharacterFlags, 0x40000);

		setFloatValue(unit_fields::BoundingRadius, 0.388999998569489f);
		setFloatValue(unit_fields::CombatReach, 1.5f);
		setByteValue(unit_fields::Bytes2, 1, 0x08 | 0x20);
		setUInt32Value(unit_fields::Bytes2, 0);
		setUInt32Value(unit_fields::UnitFlags, 0x08);
		setFloatValue(unit_fields::ModCastSpeed, 1.0f);

		// -1
		setInt32Value(character_fields::WatchedFactionIndex, -1);
		setUInt32Value(character_fields::CharacterPoints_2, 2);
		setUInt32Value(character_fields::ModHealingDonePos, 0);
		for (UInt8 i = 0; i < 7; ++i)
		{
			setUInt32Value(character_fields::ModDamageDoneNeg + i, 0);
			setUInt32Value(character_fields::ModDamageDonePos + i, 0);
			setFloatValue(character_fields::ModDamageDonePct + i, 1.00f);
		}

		//reset attack power, damage and attack speed fields
		setUInt32Value(unit_fields::BaseAttackTime, 2000);
		setUInt32Value(unit_fields::BaseAttackTime + 1, 2000); // offhand attack time
		setUInt32Value(unit_fields::RangedAttackTime, 2000);

		setFloatValue(unit_fields::MinDamage, 0.0f);
		setFloatValue(unit_fields::MaxDamage, 0.0f);
		setFloatValue(unit_fields::MinOffHandDamage, 0.0f);
		setFloatValue(unit_fields::MaxOffHandDamage, 0.0f);
		setFloatValue(unit_fields::MinRangedDamage, 0.0f);
		setFloatValue(unit_fields::MaxRangedDamage, 0.0f);

		setInt32Value(unit_fields::AttackPower, 0);
		setInt32Value(unit_fields::AttackPowerMods, 0);
		setFloatValue(unit_fields::AttackPowerMultiplier, 0.0f);
		setInt32Value(unit_fields::RangedAttackPower, 0);
		setInt32Value(unit_fields::RangedAttackPowerMods, 0);
		setFloatValue(unit_fields::RangedAttackPowerMultiplier, 0.0f);

		// Base crit values (will be recalculated in UpdateAllStats() at loading and in _ApplyAllStatBonuses() at reset
		setFloatValue(character_fields::CritPercentage, 0.0f);
		setFloatValue(character_fields::OffHandCritPercentage, 0.0f);
		setFloatValue(character_fields::RangedCritPercentage, 0.0f);

		// Init spell schools (will be recalculated in UpdateAllStats() at loading and in _ApplyAllStatBonuses() at reset
		for (UInt8 i = 0; i < 7; ++i)
			setFloatValue(character_fields::SpellCritPercentage + i, 0.0f);

		setFloatValue(character_fields::ParryPercentage, 0.0f);
		setFloatValue(character_fields::BlockPercentage, 0.0f);
		setUInt32Value(character_fields::ShieldBlock, 0);

		// Flag for pvp
		addFlag(character_fields::CharacterFlags, 512);
		addFlag(unit_fields::UnitFlags, game::unit_flags::PvP);

		removeFlag(unit_fields::UnitFlags, game::unit_flags::NotAttackable);
		removeFlag(unit_fields::UnitFlags, game::unit_flags::NotAttackablePvP);

		// Dodge percentage
		setFloatValue(character_fields::DodgePercentage, 0.0f);
	}

	game::QuestStatus GameCharacter::getQuestStatus(UInt32 quest) const
	{
		// Check if we have a cached quest state
		auto it = m_quests.find(quest);
		if (it != m_quests.end())
		{
			if (it->second.status != game::quest_status::Available &&
				it->second.status != game::quest_status::Unavailable)
				return it->second.status;
		}

		// We don't have that quest cached, make a lookup
		const auto *entry = getProject().quests.getById(quest);
		if (!entry)
		{
			WLOG("Could not find quest " << quest);
			return game::quest_status::Unavailable;
		}

		// Check if the quest is available for us
		if (getLevel() < entry->minlevel())
		{
			return game::quest_status::Unavailable;
		}
		
		// Race/Class check
		const UInt32 charRaceBit = 1 << (getRace() - 1);
		const UInt32 charClassBit = 1 << (getClass() - 1);
		if (entry->requiredraces() &&
			(entry->requiredraces() & charRaceBit) == 0)
		{
			return game::quest_status::Unavailable;
		}
		if (entry->requiredclasses() &&
			(entry->requiredclasses() & charClassBit) == 0)
		{
			return game::quest_status::Unavailable;
		}

		// Quest chain checks
		if (entry->prevquestid())
		{
			if (getQuestStatus(entry->prevquestid()) != game::quest_status::Rewarded)
			{
				return game::quest_status::Unavailable;
			}
		}

		return game::quest_status::Available;
	}

	bool GameCharacter::acceptQuest(UInt32 quest)
	{
		auto status = getQuestStatus(quest);
		if (status != game::quest_status::Available)
		{
			// We can't take that quest, maybe because we already completed it or already have it
			return false;
		}

		// Find next free quest log
		for (UInt32 i = 0; i < 25; ++i)
		{
			auto logId = getUInt32Value(character_fields::QuestLog1_1 + i * 4);
			if (logId == 0 || logId == quest)
			{
				// Take that quest
				auto &data = m_quests[quest];
				data.status = game::quest_status::Incomplete;

				// Set quest log
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 0, quest);
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 1, 0);
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 2, 0);
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 3, 0);

				// Complete if no requirements
				const auto *questEntry = getProject().quests.getById(quest);
				if (questEntry)
				{
					if (questEntry->requirements_size() == 0)
					{
						data.status = game::quest_status::Complete;
						addFlag(character_fields::QuestLog1_1 + i * 4 + 1, game::quest_status::Complete);
					}
				}

				questDataChanged(quest, data);
				return true;
			}
		}

		// No free quest slot found
		return false;
	}

	bool GameCharacter::abandonQuest(UInt32 quest)
	{
		// Find next free quest log
		for (UInt32 i = 0; i < 25; ++i)
		{
			auto logId = getUInt32Value(character_fields::QuestLog1_1 + i * 4);
			if (logId == quest)
			{
				// Take that quest
				auto &data = m_quests[quest];
				data.status = game::quest_status::Available;
				data.creatures.fill(0);
				data.objects.fill(0);
				data.items.fill(0);

				// Reset quest log
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 0, 0);
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 1, 0);
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 2, 0);
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 3, 0);

				questDataChanged(quest, data);
				return true;
			}
		}

		return false;
	}

	namespace
	{
		static UInt32 getQuestXP(UInt32 playerLevel, const proto::QuestEntry &quest)
		{
			if (quest.rewardmoneymaxlevel())
			{
				UInt32 qLevel = quest.questlevel() > 0 ? static_cast<UInt32>(quest.questlevel()) : 0;
				float fullxp = 0;
				if (qLevel >= 65)
					fullxp = quest.rewardmoneymaxlevel() / 6.0f;
				else if (qLevel == 64)
					fullxp = quest.rewardmoneymaxlevel() / 4.8f;
				else if (qLevel == 63)
					fullxp = quest.rewardmoneymaxlevel() / 3.6f;
				else if (qLevel == 62)
					fullxp = quest.rewardmoneymaxlevel() / 2.4f;
				else if (qLevel == 61)
					fullxp = quest.rewardmoneymaxlevel() / 1.2f;
				else if (qLevel > 0 && qLevel <= 60)
					fullxp = quest.rewardmoneymaxlevel() / 0.6f;

				if (playerLevel <= qLevel + 5)
					return UInt32(ceilf(fullxp));
				else if (playerLevel == qLevel + 6)
					return UInt32(ceilf(fullxp * 0.8f));
				else if (playerLevel == qLevel + 7)
					return UInt32(ceilf(fullxp * 0.6f));
				else if (playerLevel == qLevel + 8)
					return UInt32(ceilf(fullxp * 0.4f));
				else if (playerLevel == qLevel + 9)
					return UInt32(ceilf(fullxp * 0.2f));
				else
					return UInt32(ceilf(fullxp * 0.1f));
			}

			return 0;
		}
	}

	bool GameCharacter::rewardQuest(UInt32 quest, std::function<void(UInt32)> callback)
	{
		// Reward experience
		const auto *entry = m_project.quests.getById(quest);
		if (!entry)
			return false;

		auto it = m_quests.find(quest);
		if (it == m_quests.end())
		{
			return false;
		}
		if (it->second.status != game::quest_status::Complete)
		{
			return false;
		}

		UInt32 xp = getQuestXP(getLevel(), *entry);
		if (xp > 0)
		{
			rewardExperience(nullptr, xp);
		}

		UInt32 money = entry->rewardmoney() +
			(getLevel() >= 70 ? entry->rewardmoneymaxlevel() : 0);
		if (money > 0)
		{
			setUInt32Value(character_fields::Coinage,
				getUInt32Value(character_fields::Coinage) + money);
		}

		for (UInt32 i = 0; i < 25; ++i)
		{
			auto logId = getUInt32Value(character_fields::QuestLog1_1 + i * 4);
			if (logId == quest)
			{
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 0, 0);
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 1, 0);
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 2, 0);
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 3, 0);
				break;
			}
		}

		// Quest was rewarded
		it->second.status = game::quest_status::Rewarded;
		questDataChanged(quest, it->second);

		// Call callback function
		if (callback) callback(xp);

		return true;
	}

	void GameCharacter::onQuestKillCredit(GameCreature & killed)
	{
		// Check all quests in the quest log
		for (UInt32 i = 0; i < 25; ++i)
		{
			auto logId = getUInt32Value(character_fields::QuestLog1_1 + i * 4);
			if (logId == 0)
				continue;

			// Verify quest state
			auto it = m_quests.find(logId);
			if (it == m_quests.end())
				continue;

			if (it->second.status != game::quest_status::Incomplete)
				continue;

			// Find quest
			const auto *quest = getProject().quests.getById(logId);
			if (!quest)
				continue;

			ILOG("KILL_CREDIT FOR CREATURE " << killed.getName());

			// Counter needed so that the right field is used
			UInt8 reqIndex = 0;
			for (const auto &req : quest->requirements())
			{
				if (req.creatureid() == killed.getEntry().id())
				{
					ILOG("FOUND CREATURE QUEST AT INDEX " << UInt32(reqIndex));

					// Get current counter
					UInt8 counter = getByteValue(character_fields::QuestLog1_1 + i * 4 + 2, reqIndex);
					if (counter < req.creaturecount())
					{
						ILOG("COUNTER NOT FULL");

						// Increment and update counter
						setByteValue(character_fields::QuestLog1_1 + i * 4 + 2, reqIndex, ++counter);
						it->second.creatures[reqIndex]++;

						// Fire signal to update UI
						questKillCredit(*quest, killed.getGuid(), killed.getEntry().id(), counter, req.creaturecount());

						// Check if this completed the quest
						if (fulfillsQuestRequirements(*quest))
						{
							// Complete quest
							it->second.status = game::quest_status::Complete;
							addFlag(character_fields::QuestLog1_1 + i * 4 + 1, game::quest_status::Complete);
						}

						// Save quest progress
						questDataChanged(logId, it->second);
					}

					// We can stop here
					break;
				}

				reqIndex++;
			}
		}
	}

	bool GameCharacter::fulfillsQuestRequirements(const proto::QuestEntry & entry) const
	{
		if (entry.requirements_size() == 0)
			return true;

		auto it = m_quests.find(entry.id());
		if (it == m_quests.end())
			return false;

		UInt32 counter = 0;
		for (const auto &req : entry.requirements())
		{
			if (req.creatureid() != 0)
			{
				if (it->second.creatures[counter] < req.creaturecount())
					return false;
			}
			else if (req.objectid() != 0)
			{
				if (it->second.objects[counter] < req.objectcount())
					return false;
			}
			else if (req.itemid() != 0)
			{
				// TODO
				return false;
			}

			counter++;
		}

		return true;
	}

	bool GameCharacter::isQuestlogFull() const
	{
		for (int i = 0; i < 25; ++i)
		{
			if (getUInt32Value(character_fields::QuestLog1_1 + i * 4 + 0) == 0)
				return false;
		}

		return true;
	}

	void GameCharacter::setQuestData(UInt32 quest, const QuestStatusData & data)
	{
		m_quests[quest] = data;

		const auto *entry = getProject().quests.getById(quest);
		if (!entry)
			return;

		if (data.status == game::quest_status::Incomplete ||
			data.status == game::quest_status::Complete ||
			data.status == game::quest_status::Failed)
		{
			for (UInt32 i = 0; i < 25; ++i)
			{
				auto logId = getUInt32Value(character_fields::QuestLog1_1 + i * 4);
				if (logId == 0 || logId == quest)
				{
					setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 0, quest);
					setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 1, 0);
					if (data.status == game::quest_status::Complete) addFlag(character_fields::QuestLog1_1 + i * 4 + 1, game::quest_status::Complete);
					setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 2, 0);
					UInt8 offset = 0;
					for (auto &req : entry->requirements())
					{
						if (req.creatureid())
						{
							setByteValue(character_fields::QuestLog1_1 + i * 4 + 2, offset, data.creatures[offset]);
						}
						else if (req.objectid())
						{
							setByteValue(character_fields::QuestLog1_1 + i * 4 + 2, offset, data.objects[offset]);
						}

						offset++;
					}
					setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 3, 0);
					if (data.status == game::quest_status::Complete)
					{
						addFlag(character_fields::QuestLog1_1 + i * 4 + 1, game::quest_status::Complete);
					}
					break;
				}
			}
		}
	}

	void GameCharacter::levelChanged(const proto::LevelEntry &levelInfo)
	{
		// Superclass
		GameUnit::levelChanged(levelInfo);

		// One talent point per level
		updateTalentPoints();

		// Update xp to next level
		setUInt32Value(character_fields::NextLevelXp, levelInfo.nextlevelxp());
		
		// Try to find base values
		if (getClassEntry())
		{
			int levelIndex = levelInfo.id() - 1;
			if (levelIndex < getClassEntry()->levelbasevalues_size())
			{
				// Update base health and mana
				setUInt32Value(unit_fields::BaseHealth, getClassEntry()->levelbasevalues(levelIndex).health());
				setUInt32Value(unit_fields::BaseMana, getClassEntry()->levelbasevalues(levelIndex).mana());
			}
			else
			{
				DLOG("Couldn't find level entry for level " << levelInfo.id());
			}
		}
		else
		{
			DLOG("Couldn't find class entry!");
		}

		m_healthRegBase = 0.0f;
		m_manaRegBase = 0.0f;

		const auto &raceStatIt = levelInfo.stats().find(getRace());
		if (raceStatIt != levelInfo.stats().end())
		{
			const auto &classStatIt = raceStatIt->second.stats().find(getClass());
			if (classStatIt != raceStatIt->second.stats().end())
			{
				m_healthRegBase = classStatIt->second.regenhp();
				m_manaRegBase = classStatIt->second.regenmp();
			}
		}

		// Update all stats
		updateAllStats();

		// Maximize health
		setUInt32Value(unit_fields::Health, getUInt32Value(unit_fields::MaxHealth));

		// Maximize mana and energy
		setUInt32Value(unit_fields::Power1, getUInt32Value(unit_fields::MaxPower1));
		setUInt32Value(unit_fields::Power3, getUInt32Value(unit_fields::MaxPower3));
	}

	void GameCharacter::setName(const String &name)
	{
		m_name = name;
	}

	void GameCharacter::addItem(std::shared_ptr<GameItem> item, UInt16 slot)
	{
		if (slot < player_equipment_slots::End)
		{
			applyItemStats(*item, true);
		}

		// Check if that item already exists
		auto it = m_itemSlots.find(slot);
		if (it == m_itemSlots.end())
		{
			// Set new item guid
			setUInt64Value(character_fields::InvSlotHead + (slot * 2), item->getGuid());
			item->setUInt64Value(item_fields::Contained, getGuid());
			item->setUInt64Value(item_fields::Owner, getGuid());
			
			// If this item was equipped, make it visible for other players
			if (slot < player_equipment_slots::End)
			{
				setUInt32Value(character_fields::VisibleItem1_0 + (slot * 16), item->getEntry().id());
				setUInt64Value(character_fields::VisibleItem1_CREATOR + (slot * 16), item->getUInt64Value(item_fields::Creator));

				// TODO: Apply Enchantment Slots

				// TODO: Apply random property settings

			}

			// Store new item
			m_itemSlots[slot] = std::move(item);
		}
		else
		{
			// Item already exists, increase the stack number, old item will be deleted
			auto &playerItem = it->second;

			// Increase stack
			const UInt32 existingStackCount = playerItem->getUInt32Value(item_fields::StackCount);
			const UInt32 additionalStackCount = item->getUInt32Value(item_fields::StackCount);

			// Apply stack count
			playerItem->setUInt32Value(item_fields::StackCount, existingStackCount + additionalStackCount);
		}

		// Equipment changed
		if (slot < player_equipment_slots::End)
		{
			updateAllStats();
		}
	}

	void GameCharacter::addSpell(const proto::SpellEntry &spell)
	{
		if (hasSpell(spell.id()))
		{
			return;
		}

		// Evaluate parry and block spells
		for (int i = 0; i < spell.effects_size(); ++i)
		{
			const auto &effect = spell.effects(i);
			if (effect.type() == game::spell_effects::Parry)
			{
				m_canParry = true;
			}
			else if (effect.type() == game::spell_effects::Block)
			{
				m_canBlock = true;
			}
			else if (effect.type() == game::spell_effects::DualWield)
			{
				m_canDualWield = true;
			}
		}

		m_spells.push_back(&spell);
		
		// Add dependent skills
		for (int i = 0; i < spell.skillsonlearnspell_size(); ++i)
		{
			const auto *skill = m_project.skills.getById(spell.skillsonlearnspell(i));
			if (!skill)
				continue;

			// Add dependent skill
			addSkill(*skill);
		}

		// Talent point update
		if (spell.talentcost() > 0)
		{
			updateTalentPoints();
		}
	}

	bool GameCharacter::removeSpell(const proto::SpellEntry &spell)
	{
		auto it = std::find(m_spells.begin(), m_spells.end(), &spell);
		if (it == m_spells.end())
		{
			return false;
		}

		it = m_spells.erase(it);

		// Evaluate parry and block spells
		for (int i = 0; i < spell.effects_size(); ++i)
		{
			const auto &effect = spell.effects(i);
			if (effect.type() == game::spell_effects::Parry)
			{
				m_canParry = false;
			}
			else if (effect.type() == game::spell_effects::Block)
			{
				m_canBlock = false;
			}
			else if (effect.type() == game::spell_effects::DualWield)
			{
				m_canDualWield = false;
			}
		}

		// Remove spell aura effects
		getAuras().removeAllAurasDueToSpell(spell.id());

		// Remove dependent skills
		for (int i = 0; i < spell.skillsonlearnspell_size(); ++i)
		{
			// Remove dependent skill
			removeSkill(spell.skillsonlearnspell(i));
		}

		// Talent point update
		if (spell.talentcost() > 0)
		{
			updateTalentPoints();
		}

		return true;
	}

	bool GameCharacter::hasSpell(UInt32 spellId) const
	{
		for (const auto *spell : m_spells)
		{
			if (spell->id() == spellId)
			{
				return true;
			}
		}

		return false;
	}

	void GameCharacter::addSkill(const proto::SkillEntry &skill)
	{
		if (hasSkill(skill.id()))
		{
			return;
		}

		UInt16 current = 1;
		UInt16 max = 1;

		switch (skill.category())
		{
			case game::skill_category::Languages:
			{
				current = 300;
				max = 300;
				break;
			}

			case game::skill_category::Armor:
			{
				current = 1;
				max = 1;
				break;
			}

			case game::skill_category::Weapon:
			case game::skill_category::Class:
			{
				max = getLevel() * 5;
				break;
			}

			case game::skill_category::Secondary:
			case game::skill_category::Profession:
			{
				current = 1;
				max = 1;
				break;
			}

			case game::skill_category::NotDisplayed:
			case game::skill_category::Attributes:
			{
				// Invisible / Not added
				return;
			}

			default:
			{
				WLOG("Unsupported skill category: " << skill.category());
				return;
			}
		}

		// Find the next usable skill slot
		const UInt32 maxSkills = 127;
		for (UInt32 i = 0; i < maxSkills; ++i)
		{
			// Unit field values
			const UInt32 skillIndex = character_fields::SkillInfo1_1 + (i * 3);

			// Get current skill
			UInt32 skillValue = getUInt32Value(skillIndex);
			if (skillValue == 0 || skillValue == skill.id())
			{
				// Update values
				const UInt32 minMaxValue = UInt32(UInt16(current) | (UInt32(max) << 16));
				setUInt32Value(skillIndex, skill.id());
				setUInt32Value(skillIndex + 1, minMaxValue);
				m_skills.push_back(&skill);

				// TODO: Learn dependant spells if any
				return;
			}
		}

		ELOG("Maximum number of skill for character reached!");
	}

	bool GameCharacter::hasSkill(UInt32 skillId) const
	{
		for (const auto *skill : m_skills)
		{
			if (skill->id() == skillId)
			{
				return true;
			}
		}

		return false;
	}

	void GameCharacter::removeSkill(UInt32 skillId)
	{
		// Find the next usable skill slot
		const UInt32 maxSkills = 127;
		for (UInt32 i = 0; i < maxSkills; ++i)
		{
			// Unit field values
			const UInt32 skillIndex = character_fields::SkillInfo1_1 + (i * 3);

			// Get current skill
			if (getUInt32Value(skillIndex) == skillId)
			{
				setUInt32Value(skillIndex, 0);
				setUInt32Value(skillIndex + 1, 0);
				setUInt32Value(skillIndex + 2, 0);
			}
		}

		// Remove skill from the list
		auto it = m_skills.begin();
		while (it != m_skills.end())
		{
			if ((*it)->id() == skillId)
			{
				it = m_skills.erase(it);
				break;
			}

			++it;
		}
	}

	void GameCharacter::setSkillValue(UInt32 skillId, UInt16 current, UInt16 maximum)
	{
		// Find the next usable skill slot
		const UInt32 maxSkills = 127;
		for (UInt32 i = 0; i < maxSkills; ++i)
		{
			// Unit field values
			const UInt32 skillIndex = character_fields::SkillInfo1_1 + (i * 3);

			// Get current skill
			if (getUInt32Value(skillIndex) == skillId)
			{
				const UInt32 minMaxValue = UInt32(UInt16(current) | (UInt32(maximum) << 16));
				setUInt32Value(skillIndex + 1, minMaxValue);
				break;
			}
		}
	}

	bool GameCharacter::isValidItemPos(UInt8 bag, UInt8 slot) const
	{
		// Has a bag been specified?
		if (bag == 0)
			return true;

		// Any bag
		if (bag == 255)
		{
			if (slot == 255)
				return true;

			// equipment
			if (slot < player_equipment_slots::End)
				return true;

			// bag equip slots
			if (slot >= player_inventory_slots::Start && slot < player_inventory_slots::End)
				return true;

			// backpack slots
			if (slot >= player_item_slots::Start && slot < player_item_slots::End)
				return true;

			// keyring slots
			if (slot >= player_key_ring_slots::Start && slot < player_key_ring_slots::End)
				return true;

			// bank main slots
			if (slot >= player_bank_item_slots::Start && slot < player_bank_item_slots::End)
				return true;

			// bank bag slots
			if (slot >= player_bank_bag_slots::Start && slot < player_bank_bag_slots::End)
				return true;

			// Invalid
			return false;
		}

		// bag content slots
		if (bag >= player_inventory_slots::Start && bag < player_inventory_slots::End)
		{
			// TODO: Get bag at the specified position

			// Any slot
			if (slot == 255)
				return true;

			//return slot < pBag->GetBagSize();
			return false;
		}

		// bank bag content slots
		if (bag >= player_bank_bag_slots::Start && bag < player_bank_bag_slots::End)
		{
			// TODO: Get bag at the specified position

			// Any slot
			if (slot == 255)
				return true;

			//return slot < pBag->GetBagSize();
			return false;
		}

		return false;
	}

	namespace
	{
		game::weapon_prof::Type weaponProficiency(UInt32 subclass)
		{
			switch (subclass)
			{
				case game::item_subclass_weapon::Axe:
					return game::weapon_prof::OneHandAxe;
				case game::item_subclass_weapon::Axe2:
					return game::weapon_prof::TwoHandAxe;
				case game::item_subclass_weapon::Bow:
					return game::weapon_prof::Bow;
				case game::item_subclass_weapon::CrossBow:
					return game::weapon_prof::Crossbow;
				case game::item_subclass_weapon::Dagger:
					return game::weapon_prof::Dagger;
				case game::item_subclass_weapon::Fist:
					return game::weapon_prof::Fist;
				case game::item_subclass_weapon::Gun:
					return game::weapon_prof::Gun;
				case game::item_subclass_weapon::Mace:
					return game::weapon_prof::OneHandMace;
				case game::item_subclass_weapon::Mace2:
					return game::weapon_prof::TwoHandMace;
				case game::item_subclass_weapon::Polearm:
					return game::weapon_prof::Polearm;
				case game::item_subclass_weapon::Staff:
					return game::weapon_prof::Staff;
				case game::item_subclass_weapon::Sword:
					return game::weapon_prof::OneHandSword;
				case game::item_subclass_weapon::Sword2:
					return game::weapon_prof::TwoHandSword;
				case game::item_subclass_weapon::Thrown:
					return game::weapon_prof::Throw;
				case game::item_subclass_weapon::Wand:
					return game::weapon_prof::Wand;
			}

			return game::weapon_prof::None;
		}
		game::armor_prof::Type armorProficiency(UInt32 subclass)
		{
			switch (subclass)
			{
				case game::item_subclass_armor::Misc:
					return game::armor_prof::Common;
				case game::item_subclass_armor::Buckler:
				case game::item_subclass_armor::Shield:
					return game::armor_prof::Shield;
				case game::item_subclass_armor::Cloth:
					return game::armor_prof::Cloth;
				case game::item_subclass_armor::Leather:
					return game::armor_prof::Leather;
				case game::item_subclass_armor::Mail:
					return game::armor_prof::Mail;
				case game::item_subclass_armor::Plate:
					return game::armor_prof::Plate;
			}

			return game::armor_prof::None;
		}
	}

	void GameCharacter::swapItem(UInt16 src, UInt16 dst)
	{
		UInt8 srcBag = src >> 8;
		UInt8 srcSlot = src & 0xFF;

		UInt8 dstBag = dst >> 8;
		UInt8 dstSlot = dst & 0xFF;

		GameItem *srcItem = getItemByPos(srcBag, srcSlot);
		GameItem *dstItem = getItemByPos(dstBag, dstSlot);

		// Check if we have a valid source item
		if (!srcItem)
		{
			inventoryChangeFailure(game::inventory_change_failure::ItemNotFound, srcItem, dstItem);
			return;
		}

		// Check if we are alive
		if (!isAlive())
		{
			inventoryChangeFailure(game::inventory_change_failure::YouAreDead, srcItem, dstItem);
			return;
		}

		// Check equipment
		if (dstSlot < player_inventory_slots::End)
		{
			auto armorProf = getArmorProficiency();
			auto weaponProf = getWeaponProficiency();

			if (srcItem->getEntry().itemclass() == game::item_class::Weapon)
			{
				if ((weaponProf & weaponProficiency(srcItem->getEntry().subclass())) == 0)
				{
					WLOG("CAN'T EQUIP: Armor prof mask = 0x" << std::hex << std::uppercase << weaponProf << "; Required: 0x" << std::hex << std::uppercase << weaponProficiency(srcItem->getEntry().subclass()));
					inventoryChangeFailure(game::inventory_change_failure::NoRequiredProficiency, srcItem, dstItem);
					return;
				}
			}
			else if (srcItem->getEntry().itemclass() == game::item_class::Armor)
			{
				if ((armorProf & armorProficiency(srcItem->getEntry().subclass())) == 0)
				{
					WLOG("CAN'T EQUIP: Armor prof mask = 0x" << std::hex << std::uppercase << armorProf << "; Required: 0x" << std::hex << std::uppercase << armorProficiency(srcItem->getEntry().subclass()));
					inventoryChangeFailure(game::inventory_change_failure::NoRequiredProficiency, srcItem, dstItem);
					return;
				}
			}

			auto srcInvType = srcItem->getEntry().inventorytype();
			auto result = game::inventory_change_failure::None;
			switch (dstSlot)
			{
				case player_equipment_slots::Head:
					if (srcInvType != game::inventory_type::Head)
						result = game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Body:
					if (srcInvType != game::inventory_type::Body)
						result = game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Chest:
					if (srcInvType != game::inventory_type::Chest && 
						srcInvType != game::inventory_type::Robe)
						result = game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Feet:
					if (srcInvType != game::inventory_type::Feet)
						result = game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Finger1:
				case player_equipment_slots::Finger2:
					if (srcInvType != game::inventory_type::Finger)
						result = game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Trinket1:
				case player_equipment_slots::Trinket2:
					if (srcInvType != game::inventory_type::Trinket)
						result = game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Hands:
					if (srcInvType != game::inventory_type::Hands)
						result = game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Legs:
					if (srcInvType != game::inventory_type::Legs)
						result = game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Mainhand:
					if (srcInvType != game::inventory_type::MainHandWeapon &&
						srcInvType != game::inventory_type::TwoHandedWeapon &&
						srcInvType != game::inventory_type::Weapon)
					{
						result = game::inventory_change_failure::ItemDoesNotGoToSlot;
					}
					else if(srcInvType == game::inventory_type::TwoHandedWeapon)
					{
						auto *offhand = getItemByPos(0xFF, player_equipment_slots::Offhand);
						if (offhand)
						{
							bool foundFreeSlot = false;

							// Check for free inventory slot to unequip shield
							for (UInt8 slot = player_inventory_pack_slots::Start; slot < player_inventory_pack_slots::End; ++slot)
							{
								auto *test = getItemByPos(0xFF, slot);
								if (!test)
								{
									// Swap shield
									swapItem(player_equipment_slots::Offhand | 0xFF00, slot | 0xFF00);
									if (getItemByPos(0xFF, player_equipment_slots::Offhand))
									{
										// Stop, something went wrong!
										return;
									}

									foundFreeSlot = true;
									break;
								}
							}

							if (!foundFreeSlot)
							{
								result = game::inventory_change_failure::InventoryFull;
							}
						}
					}
					break;
				case player_equipment_slots::Offhand:
					if (srcInvType != game::inventory_type::OffHandWeapon &&
						srcInvType != game::inventory_type::Shield &&
						srcInvType != game::inventory_type::Weapon)
					{
						WLOG("Invalid item for offhand slot");
						result = game::inventory_change_failure::ItemDoesNotGoToSlot;
					}
					else
					{
						if (srcInvType != game::inventory_type::Shield &&
							!canDualWield())
						{
							WLOG("Can't dual-wield yet");
							result = game::inventory_change_failure::CantDualWield;
							break;
						}

						auto *item = getItemByPos(player_inventory_slots::Bag_0, player_equipment_slots::Mainhand);
						if (item &&
							item->getEntry().inventorytype() == game::inventory_type::TwoHandedWeapon)
						{
							// Can't equip offhand weapon when 2H weapon is equipped
							result = game::inventory_change_failure::CantEquipWithTwoHanded;
							break;
						}
					}
					break;
				case player_equipment_slots::Ranged:
					if (srcInvType != game::inventory_type::Ranged)
						result = game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Shoulders:
					if (srcInvType != game::inventory_type::Shoulders)
						result = game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Tabard:
					if (srcInvType != game::inventory_type::Tabard)
						result = game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Waist:
					if (srcInvType != game::inventory_type::Waist)
						result = game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Wrists:
					if (srcInvType != game::inventory_type::Wrists)
						result = game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
			}

			if (result != game::inventory_change_failure::None)
			{
				inventoryChangeFailure(result, srcItem, dstItem);
				return;
			}
		}

		// TODO: Check if source item is in still consisted as loot

		// TODO: Validate source item

		// TODO: Validate dest item

		bool updateStats = false;

		// Detect case
		if (!dstItem)
		{
			// Move items (little test)
			setUInt64Value(character_fields::InvSlotHead + (srcSlot * 2), 0);
			setUInt64Value(character_fields::InvSlotHead + (dstSlot * 2), srcItem->getGuid());

			if (srcSlot < player_equipment_slots::End)
			{
				setUInt32Value(character_fields::VisibleItem1_0 + (srcSlot * 16), 0);
				setUInt64Value(character_fields::VisibleItem1_CREATOR + (srcSlot * 16), 0);
				applyItemStats(*srcItem, false);
				updateStats = true;
			}
			if (dstSlot < player_equipment_slots::End)
			{
				setUInt32Value(character_fields::VisibleItem1_0 + (dstSlot * 16), srcItem->getEntry().id());
				setUInt64Value(character_fields::VisibleItem1_CREATOR + (dstSlot * 16), srcItem->getUInt64Value(item_fields::Creator));
				applyItemStats(*srcItem, true);
				updateStats = true;
			}

			auto srcIt = m_itemSlots.find(srcSlot);
			std::swap(m_itemSlots[dstSlot], srcIt->second);
			m_itemSlots.erase(srcIt);
		}
		else
		{
			setUInt64Value(character_fields::InvSlotHead + (srcSlot * 2), dstItem->getGuid());
			setUInt64Value(character_fields::InvSlotHead + (dstSlot * 2), srcItem->getGuid());

			if (srcSlot < player_equipment_slots::End)
			{
				setUInt32Value(character_fields::VisibleItem1_0 + (srcSlot * 16), dstItem->getEntry().id());
				setUInt64Value(character_fields::VisibleItem1_CREATOR + (srcSlot * 16), dstItem->getUInt64Value(item_fields::Creator));
				applyItemStats(*srcItem, false);
				updateStats = true;
			}
			if (dstSlot < player_equipment_slots::End)
			{
				setUInt32Value(character_fields::VisibleItem1_0 + (dstSlot * 16), srcItem->getEntry().id());
				setUInt64Value(character_fields::VisibleItem1_CREATOR + (dstSlot * 16), srcItem->getUInt64Value(item_fields::Creator));
				applyItemStats(*dstItem, false);
				applyItemStats(*srcItem, true);
				updateStats = true;
			}

			std::swap(m_itemSlots[srcSlot], m_itemSlots[dstSlot]);
		}

		if (updateStats)
			updateAllStats();
	}

	GameItem * GameCharacter::getItemByPos(UInt8 bag, UInt8 slot) const
	{
		if (bag == player_inventory_slots::Bag_0)
		{
			if (slot < player_bank_item_slots::End || (slot >= player_key_ring_slots::Start && slot < player_key_ring_slots::End))
			{
				auto it = m_itemSlots.find(slot);
				if (it != m_itemSlots.end())
				{
					return it->second.get();
				}
			}
		}
		else
		{
			// Is this a valid bag slot?
			const bool isBagSlot = (
				(bag >= player_inventory_slots::Start && bag < player_inventory_slots::End) ||
				(bag >= player_bank_bag_slots::Start && bag < player_bank_bag_slots::End)
				);

			if (isBagSlot)
			{
				// TODO: Get item from bag
			}
		}

		return nullptr;
	}

	void GameCharacter::updateArmor()
	{
		float baseArmor = getModifierValue(unit_mods::Armor, unit_mod_type::BaseValue);

		// Apply equipment
		for (UInt8 i = player_equipment_slots::Start; i < player_equipment_slots::End; ++i)
		{
			auto it = m_itemSlots.find(i);
			if (it != m_itemSlots.end())
			{
				// Add armor value from item
				baseArmor += it->second->getEntry().armor();
			}
		}

		const float basePct = getModifierValue(unit_mods::Armor, unit_mod_type::BasePct);
		const float totalArmor = getModifierValue(unit_mods::Armor, unit_mod_type::TotalValue);
		const float totalPct = getModifierValue(unit_mods::Armor, unit_mod_type::TotalPct);

		// Add armor from agility
		baseArmor += getUInt32Value(unit_fields::Stat1) * 2;
		float value = ((baseArmor * basePct) + totalArmor) * totalPct;

		setUInt32Value(unit_fields::Resistances, value);
		setUInt32Value(unit_fields::ResistancesBuffModsPositive, totalArmor);
	}

	void GameCharacter::updateDamage()
	{
		float atkPower = 0.0f;
		float level = float(getLevel());

		// Melee Attack Power
		{
			// Update attack power
			switch (getClass())
			{
			case game::char_class::Warrior:
				atkPower = level * 3.0f + getUInt32Value(unit_fields::Stat0) * 2.0f - 20.0f;
				break;
			case game::char_class::Paladin:
				atkPower = level * 3.0f + getUInt32Value(unit_fields::Stat0) * 2.0f - 20.0f;
				break;
			case game::char_class::Rogue:
				atkPower = level * 2.0f + getUInt32Value(unit_fields::Stat0) + getUInt32Value(unit_fields::Stat1) - 20.0f;
				break;
			case game::char_class::Hunter:
				atkPower = level * 2.0f + getUInt32Value(unit_fields::Stat0) + getUInt32Value(unit_fields::Stat1) - 20.0f;
				break;
			case game::char_class::Shaman:
				atkPower = level * 2.0f + getUInt32Value(unit_fields::Stat0) * 2.0f - 20.0f;
				break;
			case game::char_class::Druid:
				switch (getByteValue(unit_fields::Bytes2, 3))
				{
				case 1:		// Cat
					atkPower = level * 2.0f + getUInt32Value(unit_fields::Stat0) * 2.0f + getUInt32Value(unit_fields::Stat1) - 20.0f;
					break;
				case 5:
				case 8:
					atkPower = level * 3.0f + getUInt32Value(unit_fields::Stat0) * 2.0f - 20.0f;
					break;
				case 31:
					atkPower = level * 1.5f + getUInt32Value(unit_fields::Stat0) * 2.0f - 20.0f;
					break;
				default:
					atkPower = getUInt32Value(unit_fields::Stat0) * 2.0f - 20.0f;
					break;
				}
				break;
			case game::char_class::Mage:
				atkPower = getUInt32Value(unit_fields::Stat0) - 10.0f;
				break;
			case game::char_class::Priest:
				atkPower = getUInt32Value(unit_fields::Stat0) - 10.0f;
				break;
			case game::char_class::Warlock:
				atkPower = getUInt32Value(unit_fields::Stat0) - 10.0f;
				break;
			}

			setModifierValue(unit_mods::AttackPower, unit_mod_type::BaseValue, atkPower);
			float base_attPower = atkPower * getModifierValue(unit_mods::AttackPower, unit_mod_type::BasePct);
			atkPower = base_attPower + getModifierValue(unit_mods::AttackPower, unit_mod_type::TotalValue) * getModifierValue(unit_mods::AttackPower, unit_mod_type::TotalPct);
			setInt32Value(unit_fields::AttackPower, UInt32(atkPower));
		}
		
		// Ranged Attack power
		{
			switch (getClass())
			{
			case game::char_class::Hunter:
				atkPower = level * 2.0f + getUInt32Value(unit_fields::Stat1) - 10.0f;
				break;
			case game::char_class::Rogue:
				atkPower = level + getUInt32Value(unit_fields::Stat1) - 10.0f;
				break;
			case game::char_class::Warrior:
				atkPower = level + getUInt32Value(unit_fields::Stat1) - 10.0f;
				break;
			case game::char_class::Druid:
				// TODO shape shift
				atkPower = getUInt32Value(unit_fields::Stat1) - 10.0f;
				break;
			default:
				atkPower = getUInt32Value(unit_fields::Stat1) - 10.0f;
				break;
			}

			setInt32Value(unit_fields::RangedAttackPower, UInt32(atkPower));
		}

		float minDamage = 1.0f;
		float maxDamage = 2.0f;
		UInt32 attackTime = 2000;

		// TODO: Check druid form etc.

		UInt8 form = getByteValue(unit_fields::Bytes2, 3);
		if (form == 1 || form == 5 || form == 8)
		{
			attackTime = (form == 1 ? 1000 : 2500);
		}
		else
		{
			// Check if we are wearing a weapon in our main hand
			auto it = m_itemSlots.find(player_equipment_slots::Mainhand);
			if (it != m_itemSlots.end())
			{
				// Get weapon damage values
				const auto &entry = it->second->getEntry();
				if (entry.damage(0).mindmg() != 0.0f) minDamage = entry.damage(0).mindmg();
				if (entry.damage(0).maxdmg() != 0.0f) maxDamage = entry.damage(0).maxdmg();
				if (entry.delay() != 0) attackTime = entry.delay();
			}
		}

		const float att_speed = attackTime / 1000.0f;
		const float base_value = getUInt32Value(unit_fields::AttackPower) / 14.0f * att_speed;

		switch (form)
		{
			case 1:
			case 5:
			case 8:
				minDamage = (level > 60 ? 60 : level) * 0.85f * att_speed;
				maxDamage = (level > 60 ? 60 : level) * 1.25f * att_speed;
				break;
			default:
				break;
		}

		setFloatValue(unit_fields::MinDamage, base_value + minDamage);
		setFloatValue(unit_fields::MaxDamage, base_value + maxDamage);
		setUInt32Value(unit_fields::BaseAttackTime, attackTime);
	}

	void GameCharacter::addComboPoints(UInt64 target, UInt8 points)
	{
		if (target == 0 || points == 0)
		{
			// Reset
			m_comboTarget = 0;
			m_comboPoints = 0;
		}
		else
		{
			if (target == m_comboTarget)
			{
				if (m_comboPoints < 5)
				{
					// Add new combo points
					m_comboPoints += points;
				}
				else
				{
					// Nothing to do here since combo points are already maxed out
					return;
				}
			}
			else
			{
				// Set new combo points
				m_comboTarget = target;
				m_comboPoints = std::min<UInt8>(5, points);
			}
		}

		// Notify about combo point change
		comboPointsChanged();
	}

	void GameCharacter::applyItemStats(GameItem & item, bool apply)
	{
		if (item.getEntry().durability() == 0 ||
			item.getUInt32Value(item_fields::Durability) > 0)
		{
			// Apply values
			for (int i = 0; i < item.getEntry().stats_size(); ++i)
			{
				const auto &entry = item.getEntry().stats(i);
				if (entry.value() != 0)
				{
					switch (entry.type())
					{
						case 0:		// Mana
							updateModifierValue(unit_mods::Mana, unit_mod_type::TotalValue, entry.value(), apply);
							break;
						case 1:		// Health
							updateModifierValue(unit_mods::Health, unit_mod_type::TotalValue, entry.value(), apply);
							break;
						case 3:		// Agility
							updateModifierValue(unit_mods::StatAgility, unit_mod_type::TotalValue, entry.value(), apply);
							break;
						case 4:		// Strength
							updateModifierValue(unit_mods::StatStrength, unit_mod_type::TotalValue, entry.value(), apply);
							break;
						case 5:		// Intellect
							updateModifierValue(unit_mods::StatIntellect, unit_mod_type::TotalValue, entry.value(), apply);
							break;
						case 6:		// Spirit
							updateModifierValue(unit_mods::StatSpirit, unit_mod_type::TotalValue, entry.value(), apply);
							break;
						case 7:		// Stamina
							updateModifierValue(unit_mods::StatStamina, unit_mod_type::TotalValue, entry.value(), apply);
							break;
						default:
							break;
					}
				}
			}
		}
	}

	void GameCharacter::updateManaRegen()
	{
		const float intellect = getUInt32Value(unit_fields::Stat3);
		const float spirit = getUInt32Value(unit_fields::Stat4) * m_manaRegBase;
		const float regen = sqrtf(intellect) * spirit;

		float modManaRegenInterrupt = 0.0f;	//TODO
		setFloatValue(character_fields::ModManaRegenInterrupt, regen * modManaRegenInterrupt);
		setFloatValue(character_fields::ModManaRegen, regen);
	}

	void GameCharacter::rewardExperience(GameUnit *victim, UInt32 experience)
	{
		// Get current experience points
		UInt32 currentXP = getUInt32Value(character_fields::Xp);
		UInt32 nextLevel = getUInt32Value(character_fields::NextLevelXp);
		UInt32 level = getLevel();

		// Nothing to do here
		if (nextLevel == 0)
			return;

		UInt32 newXP = currentXP + experience;
		while (newXP >= nextLevel && nextLevel > 0)
		{
			// Calculate new XP amount
			newXP -= nextLevel;
			
			// Level up!
			setLevel(level + 1);
			nextLevel = getUInt32Value(character_fields::NextLevelXp);
		}

		// Update amount of XP
		experienceGained(victim ? victim->getGuid() : 0, experience, 0);
		setUInt32Value(character_fields::Xp, newXP);
	}

	void GameCharacter::modifyGroupUpdateFlags(GroupUpdateFlags flags, bool apply)
	{
		if (apply)
		{
			m_groupUpdateFlags = static_cast<GroupUpdateFlags>(m_groupUpdateFlags | flags);
		}
		else
		{
			m_groupUpdateFlags = static_cast<GroupUpdateFlags>(m_groupUpdateFlags & ~flags);
		}
	}

	bool GameCharacter::canBlock() const
	{
		if (!m_canBlock)
		{
			return false;
		}

		const UInt16 slot = static_cast<UInt16>(player_equipment_slots::Offhand);
		auto it = m_itemSlots.find(slot);
		if (it == m_itemSlots.end())
		{
			return false;
		}

		auto item = it->second;
		if (!item)
		{
			return false;
		}

		if (item->getEntry().inventorytype() != game::inventory_type::Shield)
		{
			return false;
		}

		return true;
	}

	bool GameCharacter::canParry() const
	{
		if (!m_canParry)
		{
			return false;
		}

		const UInt16 slot = static_cast<UInt16>(player_equipment_slots::Mainhand);
		auto it = m_itemSlots.find(slot);
		return (it != m_itemSlots.end());
	}

	bool GameCharacter::canDodge() const
	{
		return true;
	}

	bool GameCharacter::canDualWield() const
	{
		return m_canDualWield;
	}

	void GameCharacter::regenerateHealth()
	{
		float healthRegRate = 1.0f;
		if (getLevel() < 15)
		{
			healthRegRate *= (2.066f - (getLevel() * 0.066f));
		}

		// TODO: When polymorphed, regenerate health quickly

		// TODO: One missing value
		float spirit = static_cast<float>(getUInt32Value(unit_fields::Stat4));
		float baseSpirit = spirit;
		if (baseSpirit > 50.0f) baseSpirit = 50.0f;
		float moreSpirit = spirit - baseSpirit;
		float spiritRegen = baseSpirit * m_healthRegBase + moreSpirit * m_healthRegBase;

		float addHealth = spiritRegen * healthRegRate;
		UInt8 standState = getByteValue(unit_fields::Bytes1, 0);
		if (standState != unit_stand_state::Stand &&
			standState != unit_stand_state::Dead)
		{
			// 50% more regeneration when not standing
			addHealth *= 1.5f;
		}

		if (addHealth < 0.0f) 
			return;

		heal(static_cast<UInt32>(addHealth), nullptr, true);
	}

	void GameCharacter::classUpdated()
	{
		// Super class
		GameUnit::classUpdated();

		// Update class-based effects
		initClassEffects();
	}

	void GameCharacter::updateTalentPoints()
	{
		auto level = getLevel();

		UInt32 talentPoints = 0;
		if (level >= 10)
		{
			// This is the maximum number of talent points available at the current character level
			talentPoints = level - 9;

			// Now iterate through every learned spell and reduce the amount of talent points
			for (auto &spell : m_spells)
			{
				talentPoints -= spell->talentcost();
			}
		}

		setUInt32Value(character_fields::CharacterPoints_1, talentPoints);
	}
	
	void GameCharacter::initClassEffects()
	{
		m_doneMeleeAttack.disconnect();

		switch(getClass())
		{
			case game::char_class::Warrior:
				// Hard coded overpower proc for warrior: Blizzard implemented this with combo points
				// When the target dodges, the warrior simply gets a combo point.
				// Since overpower uses all combo points (just like all finishing moves for rogues and ferals),
				// it doesn't matter if we add more than one combo point to the target.
				// Hard coded: TODO proper implementation
				m_doneMeleeAttack = doneMeleeAttack.connect(
					[this](GameUnit *victim, game::VictimState victimState) {
						if (victim && victimState == game::victim_state::Dodge)
						{
							addComboPoints(victim->getGuid(), 1);
						}
					});
				break;
			default:
				break;
		}
	}

	game::InventoryChangeFailure GameCharacter::canStoreItem(UInt8 bag, UInt8 slot, ItemPosCountVector &dest, const proto::ItemEntry &item, UInt32 count, bool swap, UInt32 *noSpaceCount /*= nullptr*/) const
	{
		// No specific bag, find first free slot
		if (bag == 0xFF && slot == 0xFF)
		{
			// Find items
			for (auto &itemInst : m_itemSlots)
			{
				if (itemInst.second->getEntry().id() == item.id())
				{
					// Check item stack
					UInt32 stackCount = itemInst.second->getUInt32Value(item_fields::StackCount);
					if (stackCount < item.maxstack())
					{
						UInt32 delta = limit<UInt32>(item.maxstack() - stackCount, 0, count);
						count -= delta;
						dest.emplace_back(ItemPosCount(itemInst.first, delta));
						
						if (count == 0)
							return game::inventory_change_failure::Okay;
					}
				}
			}

			// Iterate through items by slot
			for (UInt8 i = player_inventory_pack_slots::Start; i < player_inventory_pack_slots::End; ++i)
			{
				auto it = m_itemSlots.find(i);
				if (it == m_itemSlots.end())
				{
					// Found an empty slot!
					dest.emplace_back(ItemPosCount(i, count));
					return game::inventory_change_failure::Okay;
				}
			}

			return game::inventory_change_failure::InventoryFull;
		}

		return game::inventory_change_failure::InternalBagError;
	}

	void GameCharacter::removeItem(UInt8 bag, UInt8 slot, UInt8 count)
	{
		if (bag == 0xFF)
		{
			auto it = m_itemSlots.find(slot);
			if (it != m_itemSlots.end())
			{
				UInt32 stackCount = it->second->getUInt32Value(item_fields::StackCount);
				if (stackCount > count && count > 0)
				{
					stackCount -= count;
					it->second->setUInt32Value(item_fields::StackCount, stackCount);

					// TODO: Update item instance

					return;
				}

				setUInt64Value(character_fields::InvSlotHead + (slot * 2), 0);
				m_itemSlots.erase(it);

				if (slot < player_equipment_slots::End)
				{
					setUInt32Value(character_fields::VisibleItem1_0 + (slot * 16), 0);
					setUInt64Value(character_fields::VisibleItem1_CREATOR + (slot * 16), 0);
					updateAllStats();
				}
			}
		}
	}

	void GameCharacter::getHome(UInt32 &out_map, math::Vector3 &out_pos, float &out_rot) const
	{
		out_map = m_homeMap;
		out_pos = m_homePos;
		out_rot = m_homeRotation;
	}

	void GameCharacter::setHome(UInt32 map, const math::Vector3 &position, float rot)
	{
		// Save new home position
		m_homeMap = map;
		m_homePos = position;
		m_homeRotation = rot;

		// Raise signal
		homeChanged();
	}

	GameItem * GameCharacter::getItemByGUID(UInt64 guid, UInt8 &out_bag, UInt8 &out_slot)
	{
		for (auto &item : m_itemSlots)
		{
			if (item.second->getGuid() == guid)
			{
				out_bag = 0xFF;
				out_slot = static_cast<UInt8>(item.first);
				return item.second.get();
			}
		}

		return nullptr;
	}

	bool GameCharacter::hasMainHandWeapon() const
	{
		auto *item = getItemByPos(0xFF, player_equipment_slots::Mainhand);
		return (item != nullptr);
	}

	bool GameCharacter::hasOffHandWeapon() const
	{
		auto *item = getItemByPos(0xFF, player_equipment_slots::Offhand);
		return (item && item->getEntry().inventorytype() != game::inventory_type::Shield);
	}

	io::Writer & operator<<(io::Writer &w, GameCharacter const& object)
	{
		w
			<< reinterpret_cast<GameUnit const&>(object)
			<< io::write_dynamic_range<NetUInt8>(object.m_name)
			<< io::write<NetUInt32>(object.m_zoneIndex)
			<< io::write<float>(object.m_healthRegBase)
			<< io::write<float>(object.m_manaRegBase)
			<< io::write<NetUInt64>(object.m_groupId)
			<< io::write<NetUInt32>(object.m_homeMap)
			<< io::write<float>(object.m_homePos[0])
			<< io::write<float>(object.m_homePos[1])
			<< io::write<float>(object.m_homePos[2])
			<< io::write<float>(object.m_homeRotation)
			<< io::write<NetUInt16>(object.m_quests.size());
		for (const auto &pair : object.m_quests)
		{
			w
				<< io::write<NetUInt32>(pair.first)
				<< io::write<NetUInt8>(pair.second.status)
				<< io::write<NetUInt64>(pair.second.expiration)
				<< io::write<NetUInt8>(pair.second.explored)
				<< io::write_range(pair.second.creatures)
				<< io::write_range(pair.second.objects)
				<< io::write_range(pair.second.items);
		}

		return w;
	}

	io::Reader & operator>>(io::Reader &r, GameCharacter& object)
	{
		object.initialize();
		r
			>> reinterpret_cast<GameUnit&>(object)
			>> io::read_container<NetUInt8>(object.m_name)
			>> io::read<NetUInt32>(object.m_zoneIndex)
			>> io::read<float>(object.m_healthRegBase)
			>> io::read<float>(object.m_manaRegBase)
			>> io::read<NetUInt64>(object.m_groupId)
			>> io::read<NetUInt32>(object.m_homeMap)
			>> io::read<float>(object.m_homePos[0])
			>> io::read<float>(object.m_homePos[1])
			>> io::read<float>(object.m_homePos[2])
			>> io::read<float>(object.m_homeRotation);
		UInt16 questCount = 0;
		r
			>> io::read<NetUInt16>(questCount);
		object.m_quests.clear();
		for (UInt16 i = 0; i < questCount; ++i)
		{
			UInt32 questId = 0;
			r
				>> io::read<NetUInt32>(questId);
			auto &questData = object.m_quests[questId];
			r
				>> io::read<NetUInt8>(questData.status)
				>> io::read<NetUInt64>(questData.expiration)
				>> io::read<NetUInt8>(questData.explored)
				>> io::read_range(questData.creatures)
				>> io::read_range(questData.objects)
				>> io::read_range(questData.items);
		}

		// Reset all auras
		for (UInt32 i = 0; i < 56; ++i)
		{
			object.setUInt32Value(unit_fields::Aura + i, 0);
		}

		// Update all stats
		object.setLevel(object.getLevel());

		return r;
	}
}
