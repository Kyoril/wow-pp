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
#include "game_character.h"
#include "log/default_log_levels.h"
#include "proto_data/project.h"
#include "game_item.h"
#include "common/utilities.h"
#include "game_creature.h"
#include "unit_mover.h"
#include "inventory.h"
#include "defines.h"
#include "each_tile_in_sight.h"
#include "each_tile_in_region.h"
#include "game_world_object.h"
#include "binary_io/vector_sink.h"

namespace wowpp
{
	static constexpr UInt8 MaxQuestsOnLog = 25;
	static constexpr UInt8 MaxSkills = 127;

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
		, m_inventory(*this)
		, m_lastPvPCombat(0) 
		, m_resurrectGuid(0)
		, m_resurrectMap(0)
		, m_resurrectHealth(0)
		, m_resurrectMana(0)
		, m_restType(rest_type::None)
		, m_restTrigger(nullptr)
	{
		// Resize values field
		m_values.resize(character_fields::CharacterFieldCount, 0);
		m_valueBitset.resize((character_fields::CharacterFieldCount + 31) / 32, 0);

		m_objectType |= type_mask::Player;

		// Players can charge up on terrain
		getMover().setTerrainMovement(true);

		// Reset time values
		m_playedTime.fill(0);
	}

	GameCharacter::~GameCharacter()
	{
	}

	void GameCharacter::initialize()
	{
		// Reset base combat rating and modifiers
		m_combatRatings.fill(0);
		for (auto &group : m_baseCRMod)
		{
			group[base_mod_type::Flat] = 0.0f;
			group[base_mod_type::Percentage] = 1.0f;
		}

		GameUnit::initialize();

		setUInt32Value(object_fields::Type, 25);					//OBJECT_FIELD_TYPE				(TODO: Flags)
		setUInt32Value(character_fields::CharacterFlags, 0x40000);
		setUInt32Value(character_fields::MaxLevel, 70);	// TODO: Base this on expansion level

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
		for (UInt8 i = game::spell_school::Normal; i < game::spell_school::End; ++i)
		{
			setUInt32Value(character_fields::ModDamageDoneNeg + i, 0);
			setUInt32Value(character_fields::ModDamageDonePos + i, 0);
			setFloatValue(character_fields::ModDamageDonePct + i, 1.0f);
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
		setUInt16Value(unit_fields::AttackPowerMods, 0, 0);
		setUInt16Value(unit_fields::AttackPowerMods, 1, 0);
		setFloatValue(unit_fields::AttackPowerMultiplier, 0.0f);
		setInt32Value(unit_fields::RangedAttackPower, 0);
		setInt32Value(unit_fields::RangedAttackPowerMods, 0);
		setFloatValue(unit_fields::RangedAttackPowerMultiplier, 0.0f);

		// Base crit values (will be recalculated in UpdateAllStats() at loading and in _ApplyAllStatBonuses() at reset
		setFloatValue(character_fields::CritPercentage, 0.0f);
		setFloatValue(character_fields::OffHandCritPercentage, 0.0f);
		setFloatValue(character_fields::RangedCritPercentage, 0.0f);

		// Init spell schools (will be recalculated in UpdateAllStats() at loading and in _ApplyAllStatBonuses() at reset
		for (UInt8 i = game::spell_school::Normal; i < game::spell_school::End; ++i) {
			setFloatValue(character_fields::SpellCritPercentage + i, 0.0f);
		}

		setFloatValue(character_fields::ParryPercentage, 0.0f);
		setFloatValue(character_fields::BlockPercentage, 0.0f);
		setUInt32Value(character_fields::ShieldBlock, 0);

		// Flag for pvp
		addFlag(unit_fields::UnitFlags, game::unit_flags::PvP);
		addFlag(unit_fields::UnitFlags, game::unit_flags::PvPMode);
		addFlag(character_fields::CharacterFlags, game::char_flags::PvP);
		removeFlag(unit_fields::UnitFlags, game::unit_flags::NotAttackable);
		removeFlag(unit_fields::UnitFlags, game::unit_flags::NotAttackablePvP);


		// Dodge percentage
		setFloatValue(character_fields::DodgePercentage, 0.0f);

		// Reset threat modifiers
		m_threatModifier.fill(1.0f);

		// On login, the character's state should be falling
		m_movementInfo.moveFlags = game::movement_flags::Falling;
	}

	game::QuestStatus GameCharacter::getQuestStatus(UInt32 quest) const
	{
		// Check if we have a cached quest state
		auto it = m_quests.find(quest);
		if (it != m_quests.end())
		{
			if (it->second.status != game::quest_status::Available &&
			        it->second.status != game::quest_status::Unavailable) {
				return it->second.status;
			}
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

		// Check skill
		if (entry->requiredskill() != 0)
		{
			UInt16 current = 0, max = 0;
			if (!getSkillValue(entry->requiredskill(), current, max))
			{
				return game::quest_status::Unavailable;
			}

			if (entry->requiredskillval() > 0 &&
				current < entry->requiredskillval())
			{
				return game::quest_status::Unavailable;
			}
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

		const auto *questEntry = getProject().quests.getById(quest);
		if (!questEntry)
		{
			return false;
		}

		const auto *srcItem = getProject().items.getById(questEntry->srcitemid());
		if (questEntry->srcitemid() && !srcItem)
		{
			return false;
		}

		// Find next free quest log
		for (UInt8 i = 0; i < MaxQuestsOnLog; ++i)
		{
			auto logId = getUInt32Value(character_fields::QuestLog1_1 + i * 4);
			if (logId == 0 || logId == quest)
			{
				// Grant quest source item if possible
				if (srcItem)
				{
					std::map<UInt16, UInt16> addedBySlot;
					auto result = m_inventory.createItems(*srcItem, questEntry->srcitemcount(), &addedBySlot);

					if (result != game::inventory_change_failure::Okay)
					{
						inventoryChangeFailure(result, nullptr, nullptr);
						return false;
					}
					else
					{
						// Notify the player about this
						for (auto &pair : addedBySlot)
						{
							itemAdded(pair.first, pair.second, false, false);
						}
					}
				}

				// Take that quest
				auto &data = m_quests[quest];
				data.status = game::quest_status::Incomplete;

				if (questEntry->srcspell())
				{
					// TODO: Maybe we should make the quest giver cast the spell, if it's a unit
					SpellTargetMap targetMap;
					targetMap.m_unitTarget = getGuid();
					targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
					castSpell(std::move(targetMap), questEntry->srcspell(), { 0, 0, 0 }, 0, true);
				}

				// Quest timer
				UInt32 questTimer = 0;
				if (questEntry->timelimit() > 0)
				{
					questTimer = time(nullptr) + questEntry->timelimit();
					data.expiration = questTimer;
				}

				if (questEntry->timelimit() > 0)
				{

				}

				// Set quest log
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 0, quest);
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 1, 0);
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 2, 0);
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 3, questTimer);

				// Event quests might require interacting with a quest object as well
				bool updateQuestObjects = (questEntry->flags() && game::quest_flags::Exploration);

				// Complete if no requirements
				if (fulfillsQuestRequirements(*questEntry))
				{
					data.status = game::quest_status::Complete;
					addFlag(character_fields::QuestLog1_1 + i * 4 + 1, game::quest_status::Complete);
				}
				else
				{
					// Mark all related quest items as needed
					for (auto &req : questEntry->requirements())
					{
						if (req.itemid())
						{
							m_requiredQuestItems[req.itemid()]++;
							updateQuestObjects = true;
						}
						if (req.sourceid())
						{
							m_requiredQuestItems[req.sourceid()]++;
							updateQuestObjects = true;
						}
					}
				}

				questDataChanged(quest, data);
				if (updateQuestObjects)
				{
					updateNearbyQuestObjects();
				}

				return true;
			}
		}

		// No free quest slot found
		return false;
	}

	bool GameCharacter::abandonQuest(UInt32 quest)
	{
		// Find next free quest log
		for (UInt8 i = 0; i < MaxQuestsOnLog; ++i)
		{
			auto logId = getUInt32Value(character_fields::QuestLog1_1 + i * 4);
			if (logId == quest)
			{
				auto &data = m_quests[quest];
				data.status = game::quest_status::Available;
				data.creatures.fill(0);
				data.objects.fill(0);
				data.items.fill(0);

				bool updateQuestObjects = false;

				// Mark all related quest items as needed
				const auto *entry = getProject().quests.getById(quest);
				if (entry)
				{
					// Remove source items of this quest (if any)
					if (entry->srcitemid())
					{
						const auto *itemEntry = getProject().items.getById(entry->srcitemid());
						if (itemEntry)
						{
							// 0 means: remove ALL of this item
							m_inventory.removeItems(*itemEntry, 0);
						}
					}

					// Remove quest requirement items (if any)
					for (auto &req : entry->requirements())
					{
						if (req.itemid())
						{
							const auto *itemEntry = getProject().items.getById(req.itemid());
							if (itemEntry &&
								itemEntry->itemclass() == game::item_class::Quest)
							{
								// 0 means: remove ALL of this item
								m_inventory.removeItems(*itemEntry, 0);
							}

							m_requiredQuestItems[req.itemid()]--;
							updateQuestObjects = true;
						}
						if (req.sourceid())
						{
							const auto *itemEntry = getProject().items.getById(req.sourceid());
							if (itemEntry &&
								itemEntry->itemclass() == game::item_class::Quest)
							{
								// 0 means: remove ALL of this item
								m_inventory.removeItems(*itemEntry, 0);
							}

							m_requiredQuestItems[req.sourceid()]--;
							updateQuestObjects = true;
						}
					}
				}

				// Reset quest log
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 0, 0);
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 1, 0);
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 2, 0);
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 3, 0);

				questDataChanged(quest, data);
				if (updateQuestObjects)
				{
					updateNearbyQuestObjects();
				}

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
				if (qLevel >= 65) {
					fullxp = quest.rewardmoneymaxlevel() / 6.0f;
				}
				else if (qLevel == 64) {
					fullxp = quest.rewardmoneymaxlevel() / 4.8f;
				}
				else if (qLevel == 63) {
					fullxp = quest.rewardmoneymaxlevel() / 3.6f;
				}
				else if (qLevel == 62) {
					fullxp = quest.rewardmoneymaxlevel() / 2.4f;
				}
				else if (qLevel == 61) {
					fullxp = quest.rewardmoneymaxlevel() / 1.2f;
				}
				else if (qLevel > 0 && qLevel <= 60) {
					fullxp = quest.rewardmoneymaxlevel() / 0.6f;
				}

				if (playerLevel <= qLevel + 5) {
					return UInt32(ceilf(fullxp));
				}
				else if (playerLevel == qLevel + 6) {
					return UInt32(ceilf(fullxp * 0.8f));
				}
				else if (playerLevel == qLevel + 7) {
					return UInt32(ceilf(fullxp * 0.6f));
				}
				else if (playerLevel == qLevel + 8) {
					return UInt32(ceilf(fullxp * 0.4f));
				}
				else if (playerLevel == qLevel + 9) {
					return UInt32(ceilf(fullxp * 0.2f));
				}
				else {
					return UInt32(ceilf(fullxp * 0.1f));
				}
			}

			return 0;
		}
	}

	bool GameCharacter::completeQuest(UInt32 quest)
	{
		// Check all quests in the quest log
		for (UInt8 i = 0; i < MaxQuestsOnLog; ++i)
		{
			auto logId = getUInt32Value(character_fields::QuestLog1_1 + i * 4);
			if (logId != quest)
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

			if (!it->second.explored)
			{
				if (quest->flags() & game::quest_flags::Exploration)
				{
					it->second.explored = true;
				}
			}

			// Counter needed so that the right field is used
			UInt8 reqIndex = 0;
			for (const auto &req : quest->requirements())
			{
				if (req.creatureid() != 0)
				{
					// Get current counter
					UInt8 counter = getByteValue(character_fields::QuestLog1_1 + i * 4 + 2, reqIndex);
					if (counter < req.creaturecount())
					{
						// Increment and update counter
						setByteValue(character_fields::QuestLog1_1 + i * 4 + 2, reqIndex, ++counter);
						it->second.creatures[reqIndex]++;
					}
				}
				else if (req.objectid() != 0)
				{
					// Get current counter
					UInt8 counter = getByteValue(character_fields::QuestLog1_1 + i * 4 + 2, reqIndex);
					if (counter < req.objectcount())
					{
						// Increment and update counter
						setByteValue(character_fields::QuestLog1_1 + i * 4 + 2, reqIndex, ++counter);
						it->second.creatures[reqIndex]++;
					}
				}

				reqIndex++;
			}

			// Complete quest
			it->second.status = game::quest_status::Complete;
			addFlag(character_fields::QuestLog1_1 + i * 4 + 1, game::quest_status::Complete);
			
			// Save quest progress
			questDataChanged(logId, it->second);

			return true;
		}

		return false;
	}

	bool GameCharacter::failQuest(UInt32 questId)
	{
		for (UInt8 i = 0; i < MaxQuestsOnLog; ++i)
		{
			auto logId = getUInt32Value(character_fields::QuestLog1_1 + i * 4);
			if (logId != questId)
				continue;

			// Verify quest state
			auto it = m_quests.find(logId);
			if (it == m_quests.end())
				continue;

			// Failed already or not acceptable?
			if (it->second.status == game::quest_status::Failed ||
				it->second.status == game::quest_status::Unavailable)
				continue;
			
			it->second.status = game::quest_status::Failed;

			// Find quest
			const auto *quest = getProject().quests.getById(logId);
			if (!quest)
				continue;

			// Update required item cache
			for (const auto &req : quest->requirements())
			{
				if (req.itemid() != 0)
				{
					// We needed this quest items and now no longer need them
					if (m_inventory.getItemCount(req.itemid()) < req.itemcount())
					{
						m_requiredQuestItems[req.itemid()]--;
					}
				}
			}

			// Update quest state
			setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 1, game::quest_status::Failed);

			// Reset timer
			if (getUInt32Value(character_fields::QuestLog1_1 + i * 4 + 1) > 0)
			{
				setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 3, 1);
			}

			questDataChanged(questId, it->second);
			return true;
		}

		return false;
	}

	bool GameCharacter::rewardQuest(UInt32 quest, UInt8 rewardChoice, std::function<void(UInt32)> callback)
	{
		// Reward experience
		const auto *entry = m_project.quests.getById(quest);
		if (!entry) {
			return false;
		}

		auto it = m_quests.find(quest);
		if (it == m_quests.end())
		{
			return false;
		}
		if (it->second.status != game::quest_status::Complete)
		{
			return false;
		}

		// Gather all rewarded items
		std::map<const proto::ItemEntry *, UInt16> rewardedItems;
		{
			if (entry->rewarditemschoice_size() > 0)
			{
				// Validate reward index
				if (rewardChoice >= entry->rewarditemschoice_size())
				{
					return false;
				}

				const auto *item = getProject().items.getById(
				                       entry->rewarditemschoice(rewardChoice).itemid());
				if (!item)
				{
					return false;
				}

				// Check if the player can store the item
				rewardedItems[item] += entry->rewarditemschoice(rewardChoice).count();
			}
			for (auto &rew : entry->rewarditems())
			{
				const auto *item = getProject().items.getById(rew.itemid());
				if (!item)
				{
					return false;
				}

				rewardedItems[item] += rew.count();
			}
		}

		// First loop to check if the items can be stored
		for (auto &pair : rewardedItems)
		{
			auto result = m_inventory.canStoreItems(*pair.first, pair.second);
			if (result != game::inventory_change_failure::Okay)
			{
				inventoryChangeFailure(result, nullptr, nullptr);
				return false;
			}
		}

		// Try to remove all required quest items
		for (const auto &req : entry->requirements())
		{
			if (req.itemid())
			{
				const auto *itemEntry = m_project.items.getById(req.itemid());
				if (!itemEntry)
				{
					return false;
				}

				auto result = m_inventory.removeItems(*itemEntry, req.itemcount());
				if (result != game::inventory_change_failure::Okay)
				{
					inventoryChangeFailure(result, nullptr, nullptr);
					return false;
				}
			}
		}

		// Second loop needed to actually create the items
		for (auto &pair : rewardedItems)
		{
			auto result = m_inventory.createItems(*pair.first, pair.second);
			if (result != game::inventory_change_failure::Okay)
			{
				inventoryChangeFailure(result, nullptr, nullptr);
				return false;
			}
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

		for (const auto &req : entry->requirements())
		{
			if (req.itemid()) {
				m_requiredQuestItems[req.itemid()]--;
			}
		}

		// Remove source items of this quest (if any)
		if (entry->srcitemid())
		{
			const auto *itemEntry = getProject().items.getById(entry->srcitemid());
			if (itemEntry)
			{
				// 0 means: remove ALL of this item
				m_inventory.removeItems(*itemEntry, 0);
			}
		}

		for (UInt8 i = 0; i < MaxQuestsOnLog; ++i)
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
		if (callback) {
			callback(xp);
		}

		return true;
	}

	void GameCharacter::onQuestKillCredit(UInt64 unitGuid, const proto::UnitEntry &entry)
	{
		const UInt32 creditEntry = (entry.killcredit() != 0) ? entry.killcredit() : entry.id();

		// Check all quests in the quest log
		bool updateQuestObjects = false;
		for (UInt8 i = 0; i < MaxQuestsOnLog; ++i)
		{
			auto logId = getUInt32Value(character_fields::QuestLog1_1 + i * 4);
			if (logId == 0) {
				continue;
			}

			// Verify quest state
			auto it = m_quests.find(logId);
			if (it == m_quests.end()) {
				continue;
			}

			if (it->second.status != game::quest_status::Incomplete) {
				continue;
			}

			// Find quest
			const auto *quest = getProject().quests.getById(logId);
			if (!quest) {
				continue;
			}

			// Counter needed so that the right field is used
			UInt8 reqIndex = 0;
			for (const auto &req : quest->requirements())
			{
				if (req.creatureid() == creditEntry)
				{
					// Get current counter
					UInt8 counter = getByteValue(character_fields::QuestLog1_1 + i * 4 + 2, reqIndex);
					if (counter < req.creaturecount())
					{
						// Increment and update counter
						setByteValue(character_fields::QuestLog1_1 + i * 4 + 2, reqIndex, ++counter);
						it->second.creatures[reqIndex]++;

						// Fire signal to update UI
						questKillCredit(*quest, unitGuid, creditEntry, counter, req.creaturecount());

						// Check if this completed the quest
						if (fulfillsQuestRequirements(*quest))
						{
							for (const auto &req : quest->requirements())
							{
								if (req.itemid())
								{
									m_requiredQuestItems[req.itemid()]--;
									updateQuestObjects = true;
								}
							}

							// Complete quest
							it->second.status = game::quest_status::Complete;
							addFlag(character_fields::QuestLog1_1 + i * 4 + 1, game::quest_status::Complete);
						}

						// Save quest progress
						questDataChanged(logId, it->second);
					}

					// Continue with next quest, as multiple quests could require the same
					// creature kill credit
					continue;
				}

				reqIndex++;
			}
		}

		if (updateQuestObjects)
		{
			updateNearbyQuestObjects();
		}
	}

	bool GameCharacter::fulfillsQuestRequirements(const proto::QuestEntry &entry) const
	{
		// This is an event-driven quest which can not be simply completed. Some of these quests
		// don't have a requirement and as such can't be completed by such
		if (entry.flags() & game::quest_flags::PartyAccept)
		{
			return false;
		}

		// Check if the character has this quest entry
		auto it = m_quests.find(entry.id());
		if (it == m_quests.end())
		{
			return false;
		}


		// Check for exploration quests before checking for requirements, as most of these quests
		// don't have any requirement at all and thus would pass the requirement check.
		if (entry.flags() & game::quest_flags::Exploration)
		{
			// Check if this quest has been explored
			if (!it->second.explored)
			{
				return false;
			}
		}

		// Now check for quest requirements
		if (entry.requirements_size() == 0)
		{
			return true;
		}

		// Now check all available quest requirements
		UInt32 counter = 0;
		for (const auto &req : entry.requirements())
		{
			// Creature kill / spell cast required
			if (req.creatureid() != 0)
			{
				if (it->second.creatures[counter] < req.creaturecount()) {
					return false;
				}
			}

			// Object interaction / spell cast required
			if (req.objectid() != 0)
			{
				if (it->second.objects[counter] < req.objectcount()) {
					return false;
				}
			}

			// Item required
			if (req.itemid() != 0)
			{
				// Not enough items? (Don't worry, getItemCount is fast as it uses a cached value)
				auto itemCount = m_inventory.getItemCount(req.itemid());
				if (itemCount < req.itemcount())
				{
					return false;
				}
			}

			// Increase counter
			counter++;
		}

		return true;
	}

	bool GameCharacter::isQuestlogFull() const
	{
		for (UInt8 i = 0; i < MaxQuestsOnLog; ++i)
		{
			// If this quest log slot is empty, we can stop as there is at least one
			// free quest slot available
			if (getUInt32Value(character_fields::QuestLog1_1 + i * 4 + 0) == 0)
			{
				return false;
			}
		}

		// No free quest slots found
		return true;
	}

	void GameCharacter::onQuestExploration(UInt32 questId)
	{
		// If this is set to true, all nearby objects will be updated
		bool updateNearbyObjects = false;
		for (UInt8 i = 0; i < MaxQuestsOnLog; ++i)
		{
			// Check if there is a quest in that slot
			auto logId = getUInt32Value(character_fields::QuestLog1_1 + i * 4);
			if (logId == 0) {
				continue;
			}

			// Check if the player really has accepted that quest
			auto it = m_quests.find(logId);
			if (it == m_quests.end()) {
				continue;
			}

			// Check if the quest was already completed
			if (it->second.status != game::quest_status::Incomplete) {
				continue;
			}

			// Get quest template entry
			const auto *quest = getProject().quests.getById(logId);
			if (!quest) {
				continue;
			}

			if (!it->second.explored)
			{
				if (quest->flags() & game::quest_flags::Exploration)
				{
					it->second.explored = true;
					updateNearbyObjects = true;

					// Quest is fulfilled now
					if (fulfillsQuestRequirements(*quest))
					{
						it->second.status = game::quest_status::Complete;
						addFlag(character_fields::QuestLog1_1 + i * 4 + 1, game::quest_status::Complete);
					}

					questDataChanged(logId, it->second);
				}
			}
		}

		// Finally, update all nearby objects if needed to. We do this after we checked all
		// quests, so that we will update these objects only ONCE
		if (updateNearbyObjects)
		{
			updateNearbyQuestObjects();
		}
	}

	void GameCharacter::onQuestItemAddedCredit(const proto::ItemEntry &entry, UInt32 amount)
	{
		// If this is set to true, all nearby objects will be updated
		bool updateNearbyObjects = false;
		for (UInt8 i = 0; i < MaxQuestsOnLog; ++i)
		{
			// Check if there is a quest in that slot
			auto logId = getUInt32Value(character_fields::QuestLog1_1 + i * 4);
			if (logId == 0) {
				continue;
			}

			// Check if the player really has accepted that quest
			auto it = m_quests.find(logId);
			if (it == m_quests.end()) {
				continue;
			}

			// Check if the quest was already completed
			if (it->second.status != game::quest_status::Incomplete) {
				continue;
			}

			// Get quest template entry
			const auto *quest = getProject().quests.getById(logId);
			if (!quest) {
				continue;
			}

			// If this is set to true, all requirements of this quest will be reevaluated, which costs
			// some time. So this variable is only updated, if a quest requirement status changed between
			// Completed and Uncomplete to save performance.
			bool validateQuest = false;

			// Check every quest entry requirement
			for (const auto &req : quest->requirements())
			{
				if (req.itemid() == entry.id())
				{
					if (m_inventory.getItemCount(entry.id()) >= req.itemcount())
					{
						m_requiredQuestItems[req.itemid()]--;
						updateNearbyObjects = true;
						validateQuest = true;
					}
				}
				else if (req.sourceid() == entry.id())
				{
					if (m_inventory.getItemCount(entry.id()) >= req.sourcecount())
					{
						m_requiredQuestItems[req.sourceid()]--;
						updateNearbyObjects = true;
						validateQuest = true;
					}
				}
			}

			// Check if the quest requirements need to be reevaluated
			if (validateQuest)
			{
				// Quest is fulfilled now
				if (fulfillsQuestRequirements(*quest))
				{
					it->second.status = game::quest_status::Complete;
					addFlag(character_fields::QuestLog1_1 + i * 4 + 1, game::quest_status::Complete);
					questDataChanged(logId, it->second);
				}
			}
		}

		// Finally, update all nearby objects if needed to. We do this after we checked all
		// quests, so that we will update these objects only ONCE
		if (updateNearbyObjects)
		{
			updateNearbyQuestObjects();
		}
	}

	void GameCharacter::onQuestItemRemovedCredit(const proto::ItemEntry &entry, UInt32 amount)
	{
		bool updateNearbyObjects = false;
		for (UInt8 i = 0; i < MaxQuestsOnLog; ++i)
		{
			auto logId = getUInt32Value(character_fields::QuestLog1_1 + i * 4);
			if (logId == 0) {
				continue;
			}

			// Verify quest state
			auto it = m_quests.find(logId);
			if (it == m_quests.end()) {
				continue;
			}

			if (it->second.status != game::quest_status::Complete) {
				continue;
			}

			// Find quest
			const auto *quest = getProject().quests.getById(logId);
			if (!quest) {
				continue;
			}

			bool validateQuest = false;

			for (const auto &req : quest->requirements())
			{
				if (req.itemid() == entry.id())
				{
					if (m_inventory.getItemCount(entry.id()) < req.itemcount())
					{
						m_requiredQuestItems[req.itemid()]++;
						updateNearbyObjects = true;
						validateQuest = true;
					}
				}
				if (req.sourceid() == entry.id())
				{
					if (m_inventory.getItemCount(entry.id()) < req.sourcecount())
					{
						m_requiredQuestItems[req.sourceid()]++;
						updateNearbyObjects = true;
						validateQuest = true;
					}
				}
			}

			if (validateQuest)
			{
				// Found it: Complete quest if completable
				if (!fulfillsQuestRequirements(*quest))
				{
					it->second.status = game::quest_status::Incomplete;
					removeFlag(character_fields::QuestLog1_1 + i * 4 + 1, game::quest_status::Complete);
					questDataChanged(logId, it->second);
				}
			}
		}

		if (updateNearbyObjects)
		{
			updateNearbyQuestObjects();
		}
	}

	void GameCharacter::onQuestSpellCastCredit(UInt32 spellId, GameObject & target)
	{
		bool updateNearbyObjects = false;
		for (UInt8 i = 0; i < MaxQuestsOnLog; ++i)
		{
			auto logId = getUInt32Value(character_fields::QuestLog1_1 + i * 4);
			if (logId == 0) {
				continue;
			}

			// Verify quest state
			auto it = m_quests.find(logId);
			if (it == m_quests.end()) {
				continue;
			}

			if (it->second.status != game::quest_status::Incomplete) {
				continue;
			}

			// Find quest
			const auto *quest = getProject().quests.getById(logId);
			if (!quest) {
				continue;
			}

			bool validateQuest = false;

			// Counter needed so that the correct field is used
			UInt8 reqIndex = 0;
			for (const auto &req : quest->requirements())
			{
				// Get current counter
				UInt8 counter = getByteValue(character_fields::QuestLog1_1 + i * 4 + 2, reqIndex);
				if (req.spellcast() == spellId)
				{
					if (req.creatureid() && counter < req.creaturecount() && target.isCreature())
					{
						if (reinterpret_cast<GameCreature&>(target).getEntry().id() == req.creatureid())
						{
							// Increment and update counter
							setByteValue(character_fields::QuestLog1_1 + i * 4 + 2, reqIndex, ++counter);
							it->second.creatures[reqIndex]++;

							// Fire signal to update UI
							questKillCredit(*quest, target.getGuid(), req.creatureid(), counter, req.creaturecount());

							validateQuest = true;
						}
					}
					else if (req.objectid() && counter < req.objectcount() && target.isWorldObject())
					{
						if (reinterpret_cast<WorldObject&>(target).getEntry().id() == req.objectid())
						{
							// Increment and update counter
							setByteValue(character_fields::QuestLog1_1 + i * 4 + 2, reqIndex, ++counter);
							it->second.objects[reqIndex]++;

							// Fire signal to update UI
							questKillCredit(*quest, target.getGuid(), req.objectid(), counter, req.objectcount());

							validateQuest = true;
						}
					}
				}

				reqIndex++;
			}

			if (validateQuest)
			{
				// Found it: Complete quest if completable
				if (fulfillsQuestRequirements(*quest))
				{
					it->second.status = game::quest_status::Complete;
					addFlag(character_fields::QuestLog1_1 + i * 4 + 1, game::quest_status::Complete);
				}

				// Save quest progress
				questDataChanged(logId, it->second);
			}
		}

		if (updateNearbyObjects)
		{
			updateNearbyQuestObjects();
		}
	}

	void GameCharacter::onQuestObjectCredit(UInt32 spellId, WorldObject & target)
	{
		// Check all quests in the quest log
		bool updateQuestObjects = false;
		for (UInt8 i = 0; i < MaxQuestsOnLog; ++i)
		{
			auto logId = getUInt32Value(character_fields::QuestLog1_1 + i * 4);
			if (logId == 0) {
				continue;
			}

			// Verify quest state
			auto it = m_quests.find(logId);
			if (it == m_quests.end()) {
				continue;
			}

			if (it->second.status != game::quest_status::Incomplete) {
				continue;
			}

			// Find quest
			const auto *quest = getProject().quests.getById(logId);
			if (!quest) {
				continue;
			}

			// Counter needed so that the right field is used
			UInt8 reqIndex = 0;
			for (const auto &req : quest->requirements())
			{
				if (req.objectid() == target.getEntry().id() &&
					(req.spellcast() == 0 || req.spellcast() == spellId))
				{
					// Get current counter
					UInt8 counter = getByteValue(character_fields::QuestLog1_1 + i * 4 + 2, reqIndex);
					if (counter < req.objectcount())
					{
						// Increment and update counter
						setByteValue(character_fields::QuestLog1_1 + i * 4 + 2, reqIndex, ++counter);
						it->second.objects[reqIndex]++;

						// Fire signal to update UI
						questKillCredit(*quest, target.getGuid(), (target.getEntry().id() | 0x80000000), counter, req.objectcount());

						// Check if this completed the quest
						if (fulfillsQuestRequirements(*quest))
						{
							for (const auto &req : quest->requirements())
							{
								if (req.itemid())
								{
									m_requiredQuestItems[req.itemid()]--;
								}
							}

							// Complete quest
							it->second.status = game::quest_status::Complete;
							addFlag(character_fields::QuestLog1_1 + i * 4 + 1, game::quest_status::Complete);
						}

						// Save quest progress
						questDataChanged(logId, it->second);
						updateQuestObjects = true;
					}

					// Continue with next quest, as multiple quests could require the same
					// creature kill credit
					break;
				}

				reqIndex++;
			}
		}

		if (updateQuestObjects)
		{
			updateNearbyQuestObjects();
		}
	}

	bool GameCharacter::needsQuestItem(UInt32 itemId) const
	{
		auto it = m_requiredQuestItems.find(itemId);
		return (it != m_requiredQuestItems.end() ? it->second > 0 : false);
	}

	void GameCharacter::modifySpellMod(SpellModifier &mod, bool apply)
	{
		for (UInt8 eff = 0; eff < 64; ++eff)
		{
			UInt64 mask = UInt64(1) << eff;
			if (mod.mask & mask)
			{
				Int32 val = 0;
				for (auto it = m_spellModsByOp[mod.op].begin(); it != m_spellModsByOp[mod.op].end(); ++it)
				{
					if (it->type == mod.type && it->mask & mask)
					{
						val += it->value;
					}
				}

				val += apply ? mod.value : -(mod.value);
				spellModChanged(mod.type, eff, mod.op, val);
			}
		}

		if (apply)
		{
			m_spellModsByOp[mod.op].push_back(mod);
		}
		else
		{
			for (auto it = m_spellModsByOp[mod.op].begin(); it != m_spellModsByOp[mod.op].end(); ++it)
			{
				if (it->mask == mod.mask &&
					it->value == mod.value &&
					it->type == mod.type &&
					it->op == mod.op
					)
				{
					it = m_spellModsByOp[mod.op].erase(it);
					break;
				}
			}
		}
	}

	Int32 GameCharacter::getTotalSpellMods(SpellModType type, SpellModOp op, UInt32 spellId) const
	{
		const auto *spell = getProject().spells.getById(spellId);
		if (!spell)
			return 0;

		// Get spell modifier by op list
		auto list = m_spellModsByOp.find(op);
		if (list == m_spellModsByOp.end())
		{
			return 0;
		}

		Int32 total = 0;
		for (const auto &mod : list->second)
		{
			if (mod.type != type)
			{
				continue;
			}

			UInt64 familyFlags = spell->familyflags();
			/*
			// Should this be like this? affectmask is for familyflags not class (eg. Improve Gouge affectmask's 8, rogue family is 8 so it affects everything)
			if (!familyFlags)
			{
				familyFlags = spell->family();
			}
			*/
			if (familyFlags & mod.mask)
			{
				total += mod.value;
			}
		}

		return total;
	}

	void GameCharacter::modifyThreatModifier(UInt32 schoolMask, float modifier, bool apply)
	{
		for (UInt32 i = 0; i < m_threatModifier.size(); ++i)
		{
			if (schoolMask & (1 << i))
			{
				m_threatModifier[i] *= (apply ? (1.0f + modifier) : 1.0f / (1.0f + modifier));
			}
		}
	}

	void GameCharacter::applyThreatMod(UInt32 schoolMask, float & ref_threat)
	{
		for (UInt32 i = 0; i < m_threatModifier.size(); ++i)
		{
			// We only need to find the first valid school in case of multiple schools
			if (schoolMask & (1 << i))
			{
				ref_threat *= m_threatModifier[i];
				return;
			}
		}
	}

	void GameCharacter::setResurrectRequestData(UInt64 guid, UInt32 mapId, const math::Vector3 &location, UInt32 health, UInt32 mana)
	{
		m_resurrectGuid = guid;
		m_resurrectMap = mapId;
		m_resurrectLocation = location;
		m_resurrectHealth = health;
		m_resurrectMana = mana;
	}

	void GameCharacter::resurrectUsingRequestData()
	{
		if (isPlayerGUID(m_resurrectGuid))
		{
			teleport(m_resurrectMap, m_resurrectLocation, getOrientation());
		}

		// TODO: delay (not implemented yet)

		// TODO: proper resurrect maybe (resurrect on angel not implemented yet)
		revive(m_resurrectHealth, m_resurrectMana);

		setUInt32Value(unit_fields::Power4, getUInt32Value(unit_fields::MaxPower4));

		setResurrectRequestData(0, 0, math::Vector3(), 0, 0);
		// TODO: spawn corpse bones (not implemented yet)
	}

	void GameCharacter::updateRating(CombatRatingType combatRating)
	{
		setUInt32Value(character_fields::CombatRating_1 + combatRating, UInt32(m_combatRatings[combatRating]));

		switch (combatRating)
		{
		case combat_rating::Parry:
			updateParryPercentage();
			break;
		case combat_rating::Dodge:
			updateDodgePercentage();
			break;
		case combat_rating::CritMelee:
			updateCritChance(game::weapon_attack::BaseAttack);
			updateCritChance(game::weapon_attack::OffhandAttack);
			break;
		case combat_rating::CritRanged:
			updateCritChance(game::weapon_attack::RangedAttack);
			break;
		case combat_rating::CritSpell:
			updateAllSpellCritChances();
			break;
		}
	}

	void GameCharacter::applyCombatRatingMod(CombatRatingType combatRating, Int32 amount, bool apply)
	{
		m_combatRatings[combatRating] += apply ? amount : -amount;

		updateRating(combatRating);
	}

	void GameCharacter::updateAllRatings()
	{
		for (UInt8 cr = 0; cr < combat_rating::End; ++cr)
		{
			updateRating(static_cast<CombatRatingType>(cr));
		}
	}

	float GameCharacter::getRatingMultiplier(CombatRatingType combatRating) const
	{
		UInt32 level = getLevel();
		
		if (level > 100)
		{
			level = 100;
		}

		const auto *entry = getProject().combatRatings.getById(combatRating * 100 + level - 1);
		if (!entry)
		{
			ELOG("ERR");
			return 1.0f;
		}

		return 1.0f / entry->ratingsperlevel();
	}

	void GameCharacter::updateCritChance(game::WeaponAttack attackType)
	{
		BaseModGroup modGroup;
		UInt16 index;
		CombatRatingType combatRating;

		switch (attackType)
		{
			case game::weapon_attack::OffhandAttack:
				modGroup = base_mod_group::OffHandCritPercentage;
				index = character_fields::OffHandCritPercentage;
				combatRating = combat_rating::CritMelee;
				break;
			case game::weapon_attack::RangedAttack:
				modGroup = base_mod_group::RangedCritPercentage;
				index = character_fields::RangedCritPercentage;
				combatRating = combat_rating::CritRanged;
				break;
			default:
				modGroup = base_mod_group::CritPercentage;
				index = character_fields::CritPercentage;
				combatRating = combat_rating::CritMelee;
				break;
		}

		float value = getRatingBonusValue(combatRating) + getTotalPercentageModValue(modGroup);

		const Int32 weaponSkillVal = getWeaponSkillValue(attackType, *this);
		const Int32 maxWeaponSkillVal = getMaxWeaponSkillValueForLevel();
		value += (weaponSkillVal - maxWeaponSkillVal) * 0.04f;

		setFloatValue(index, value < 0.0f ? 0.0f : value);
	}

	void GameCharacter::updateDodgePercentage()
	{
		float value = getDodgeFromAgility();

		const Int32 defenseSkillVal = getDefenseSkillValue(*this);
		const Int32 maxWeaponSkillVal = getMaxWeaponSkillValueForLevel();
		value += (defenseSkillVal - maxWeaponSkillVal) * 0.04f;

		value += getAuras().getTotalBasePoints(game::aura_type::ModDodgePercent);

		value += getRatingBonusValue(combat_rating::Dodge);

		setFloatValue(character_fields::DodgePercentage, value < 0.0f ? 0.0f : value);
	}

	void GameCharacter::updateParryPercentage()
	{
		float value = 0.0f;

		if (canParry())
		{
			value = 5.0f;

			const Int32 defenseSkillVal = getDefenseSkillValue(*this);
			const Int32 maxWeaponSkillVal = getMaxWeaponSkillValueForLevel();
			value += (defenseSkillVal - maxWeaponSkillVal) * 0.04f;

			value += getAuras().getTotalBasePoints(game::aura_type::ModParryPercent);

			value += getRatingBonusValue(combat_rating::Parry);
		}

		setFloatValue(character_fields::ParryPercentage, value < 0.0f ? 0.0f : value);
	}

	void GameCharacter::updateAllCritChances()
	{
		float value = getMeleeCritFromAgility();

		m_baseCRMod[base_mod_group::CritPercentage][base_mod_type::Percentage] = value;
		m_baseCRMod[base_mod_group::OffHandCritPercentage][base_mod_type::Percentage] = value;
		m_baseCRMod[base_mod_group::RangedCritPercentage][base_mod_type::Percentage] = value;

		updateCritChance(game::weapon_attack::BaseAttack);
		updateCritChance(game::weapon_attack::OffhandAttack);
		updateCritChance(game::weapon_attack::RangedAttack);
	}

	void GameCharacter::updateSpellCritChance(game::SpellSchool spellSchool)
	{
		float crit = 0.0f;

		if (spellSchool != game::spell_school::Normal)
		{
			crit += getSpellCritFromIntellect();

			crit += getAuras().getTotalBasePoints(game::aura_type::ModSpellCritChance);

			// TODO: ModSpellCritChanceSchool aura

			crit += getRatingBonusValue(combat_rating::CritSpell);
		}

		setFloatValue(character_fields::SpellCritPercentage + spellSchool, crit);
	}

	void GameCharacter::updateAllSpellCritChances()
	{
		for (UInt8 i = game::spell_school::Normal; i < game::spell_school::End; ++i)
		{
			updateSpellCritChance(game::SpellSchool(i));
		}
	}

	void GameCharacter::applyWeaponCritMod(std::shared_ptr<GameItem> item, game::WeaponAttack attackType, const proto::SpellEntry & spell, float amount, bool apply)
	{
		if (spell.itemclass() != -1)
		{
			BaseModGroup mod = base_mod_group::End;

			switch (attackType)
			{
				case game::weapon_attack::BaseAttack:
					mod = base_mod_group::CritPercentage;
					break;
				case game::weapon_attack::OffhandAttack:
					mod = base_mod_group::OffHandCritPercentage;
					break;
				case game::weapon_attack::RangedAttack:
					mod = base_mod_group::RangedCritPercentage;
					break;
				default:
					return;
			}

			if (item->isCompatibleWithSpell(spell))
			{
				handleBaseCRMod(mod, base_mod_type::Flat, amount, apply);
			}
		}
	}

	void GameCharacter::handleBaseCRMod(BaseModGroup modGroup, BaseModType modType, float amount, bool apply)
	{
		float value;

		switch (modType)
		{
			case base_mod_type::Flat:
				m_baseCRMod[modGroup][modType] += apply ? amount : -amount;
				break;
			case base_mod_type::Percentage:
				if (amount <= -100.0f)
				{
					amount = -200.0f;
				}

				value = (100.0f + amount) / 100.0f;
				m_baseCRMod[modGroup][modType] *= apply ? value : (1.0f / value);
				break;
			default:
				break;
		}

		switch (modGroup)
		{
			case base_mod_group::CritPercentage:
				updateCritChance(game::weapon_attack::BaseAttack);
				break;
			case base_mod_group::RangedCritPercentage:
				updateCritChance(game::weapon_attack::RangedAttack);
				break;
			case base_mod_group::OffHandCritPercentage:
				updateCritChance(game::weapon_attack::OffhandAttack);
				break;
			case base_mod_group::ShieldBlockValue:
				// TODO
				break;
			default:
				break;
		}
	}

	void GameCharacter::setPlayTime(PlayerTimeIndex index, UInt32 value)
	{
		m_playedTime[index] = value;
	}

	void GameCharacter::setRestType(RestType type, const proto::AreaTriggerEntry *trigger)
	{
		// Update rest type
		m_restType = type;

		// Update character flags and state
		if (type == rest_type::None)
		{
			// Remove the resting flag
			removeFlag(character_fields::CharacterFlags, game::char_flags::Resting);
			m_restTrigger = nullptr;
		}
		else
		{
			// Add the resting flag (if not set already)
			addFlag(character_fields::CharacterFlags, game::char_flags::Resting);
			m_restTrigger = trigger;
		}
	}

	bool GameCharacter::isInRestAreaTrigger() const
	{
		// Was a trigger set properly?
		if (!m_restTrigger)
			return false;

		return isInAreaTrigger(*m_restTrigger, 5.0f);
	}

	bool GameCharacter::isInAreaTrigger(const proto::AreaTriggerEntry & entry, float delta) const 
	{
		const math::Vector3 &position = getLocation();
		if (getMapId() != entry.map())
			return false;

		if (entry.radius() > 0)
		{
			// Sphere radius check
			const math::Vector3 pt(entry.x(), entry.y(), entry.z());
			const float distSq = (position - pt).squared_length();
			const float tolerated = entry.radius() + delta;
			if (distSq > tolerated * tolerated)
			{
				return false;
			}
		}
		else
		{
			// Box check

			// 2PI = 360, keep in mind that ingame orientation is counter-clockwise
			const double rotation = 2 * 3.1415927 - entry.box_o();
			const double sinVal = sin(rotation);
			const double cosVal = cos(rotation);

			float playerBoxDistX = position.x - entry.x();
			float playerBoxDistY = position.y - entry.y();

			float rotPlayerX = float(entry.x() + playerBoxDistX * cosVal - playerBoxDistY * sinVal);
			float rotPlayerY = float(entry.y() + playerBoxDistY * cosVal + playerBoxDistX * sinVal);

			// box edges are parallel to coordiante axis, so we can treat every dimension independently :D
			float dz = position.z - entry.z();
			float dx = rotPlayerX - entry.x();
			float dy = rotPlayerY - entry.y();
			if ((fabs(dx) > entry.box_x() / 2 + delta) ||
				(fabs(dy) > entry.box_y() / 2 + delta) ||
				(fabs(dz) > entry.box_z() / 2 + delta))
			{
				return false;
			}
		}

		return true;
	}

	game::FactionFlags GameCharacter::getBaseFlags(UInt32 faction) const
	{
		const auto *entry = getProject().factions.getById(faction);
		if (!entry)
			return game::faction_flags::None;

		const UInt32 classMask = 1 << (getClass() - 1);
		const UInt32 raceMask = 1 << (getRace() - 1);
		for (const auto &rep : entry->baserep())
		{
			if ((rep.racemask() == 0 || rep.racemask() & raceMask) &&
				(rep.classmask() == 0 || rep.classmask() & classMask))
			{
				return static_cast<game::FactionFlags>(rep.flags());
			}
		}

		return game::faction_flags::None;
	}

	Int32 GameCharacter::getBaseReputation(UInt32 faction) const
	{
		const auto *entry = getProject().factions.getById(faction);
		if (!entry)
			return 0;

		const UInt32 classMask = 1 << (getClass() - 1);
		const UInt32 raceMask = 1 << (getRace() - 1);
		for (const auto &rep : entry->baserep())
		{
			if ((rep.racemask() == 0 || rep.racemask() & raceMask) &&
				(rep.classmask() == 0 || rep.classmask() & classMask))
			{
				return rep.value();
			}
		}

		return 0;
	}

	Int32 GameCharacter::getReputation(UInt32 faction) const
	{
		// TODO
		return getBaseReputation(faction);
	}

	void GameCharacter::onKilled(GameUnit * killer)
	{
		GameUnit::onKilled(killer);

		bool updateQuestObjects = false;
		for (UInt8 i = 0; i < MaxQuestsOnLog; ++i)
		{
			auto logId = getUInt32Value(character_fields::QuestLog1_1 + i * 4);

			// Find quest
			const auto *quest = getProject().quests.getById(logId);
			if (!quest)
				continue;

			if (quest->flags() & game::quest_flags::StayAlive)
			{
				if (failQuest(logId))
				{
					updateQuestObjects = true;
				}
			}
		}

		if (updateQuestObjects)
		{
			updateNearbyQuestObjects();
		}
	}

	void GameCharacter::setQuestData(UInt32 quest, const QuestStatusData &data)
	{
		m_quests[quest] = data;

		const auto *entry = getProject().quests.getById(quest);
		if (!entry) {
			return;
		}

		if (data.status == game::quest_status::Incomplete ||
		    data.status == game::quest_status::Complete ||
		    data.status == game::quest_status::Failed)
		{
			for (UInt8 i = 0; i < MaxQuestsOnLog; ++i)
			{
				auto logId = getUInt32Value(character_fields::QuestLog1_1 + i * 4);
				if (logId == 0 || logId == quest)
				{
					setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 0, quest);
					setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 1, 0);
					setUInt32Value(character_fields::QuestLog1_1 + i * 4 + 3, static_cast<UInt32>(data.expiration));
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
					if (data.status == game::quest_status::Complete)
					{
						addFlag(character_fields::QuestLog1_1 + i * 4 + 1, game::quest_status::Complete);
					}
					else if (data.status == game::quest_status::Failed)
					{
						addFlag(character_fields::QuestLog1_1 + i * 4 + 1, game::quest_status::Failed);
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

		// Update maximum of all skills if needed
		for (const auto *skill : m_skills)
		{
			if (skill->category() == game::skill_category::Weapon)
			{
				UInt16 current = 0, max = 0;
				getSkillValue(skill->id(), current, max);

				max = static_cast<UInt16>(getLevel() * 5);
				setSkillValue(skill->id(), std::min(current, max), max);
			}
		}
	}

	void GameCharacter::setName(const String &name)
	{
		m_name = name;
	}

	void GameCharacter::setZone(UInt32 zoneIndex)
	{
		const auto *entry = getProject().zones.getById(zoneIndex);
		if (!entry)
		{
			WLOG("Unknown zone id " << zoneIndex);
			return;
		}

		m_zoneIndex = zoneIndex;

		// Get area of zone
		const proto::ZoneEntry *area = entry;
		if (entry->parentzone() != 0)
		{
			area = getProject().zones.getById(entry->parentzone());
		}

		if (area == nullptr)
		{
			WLOG("No area belongs to zone " << zoneIndex);
			return;
		}

		// Eventually update rest zone
		if (area->flags() & game::area_flags::Capital)
		{
			setRestType(rest_type::City, nullptr);
		}
		else if (m_restType == rest_type::City)
		{
			setRestType(rest_type::None, nullptr);
		}
	}

	bool GameCharacter::addSpell(const proto::SpellEntry &spell)
	{
		if (hasSpell(spell.id()))
			return false;

		// Check race requirement
		if (spell.racemask() != 0 && !(spell.racemask() & (1 << (getRace() - 1))))
			return false;

		// Check class requirement
		if (spell.classmask() != 0 && !(spell.classmask() & (1 << (getClass() - 1))))
			return false;

		// Evaluate parry and block spells
		for (int i = 0; i < spell.effects_size(); ++i)
		{
			const auto &effect = spell.effects(i);
			switch (effect.type())
			{
				case game::spell_effects::Parry:
					m_canParry = true;
					break;
				case game::spell_effects::Block:
					m_canBlock = true;
					break;
				case game::spell_effects::DualWield:
					m_canDualWield = true;
					break;
				default:
					break;
			}
		}

		m_spells.push_back(&spell);

		// Add dependent skills
		for (int i = 0; i < spell.skillsonlearnspell_size(); ++i)
		{
			const auto *skill = m_project.skills.getById(spell.skillsonlearnspell(i));
			if (!skill) {
				continue;
			}

			// Add dependent skill
			addSkill(*skill);
		}

		// Talent point update
		if (spell.talentcost() > 0)
		{
			updateTalentPoints();
		}

		// Fire signal
		spellLearned(spell);
		// If it is a trade skill...
		bool isTradeSkill = false;
		Int32 skillIndex = 0, skillPoint = 0;
		for (const auto &effect : spell.effects())
		{
			if (effect.type() == game::spell_effects::TradeSkill)
			{
				isTradeSkill = true;
			}
			else if (effect.type() == game::spell_effects::Skill)
			{
				skillIndex = effect.miscvaluea();
				skillPoint = effect.basepoints() + effect.basedice();
			}
		}

		if (skillIndex > 0)
		{
			const auto *skill = m_project.skills.getById(skillIndex);
			if (skill) {
				// Add dependent skill
				addSkill(*skill);
				UInt16 current = 1, max = 1;
				getSkillValue(skillIndex, current, max);

				// Riding skill is always maxed out
				if (skillIndex == 762)
					current = 75 * skillPoint;

				setSkillValue(skillIndex, current, 75 * skillPoint);
			}
		}

		return true;
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
				max = 75;
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
		for (UInt8 i = 0; i < MaxSkills; ++i)
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

				// Learn dependant spells if any
				for (const auto &spell : skill.spells())
				{
					const auto *spellEntry = getProject().spells.getById(spell);
					if (spellEntry)
					{
						// Spell may be learned
						addSpell(*spellEntry);
					}
				}

				return;
			}
		}

		ELOG("Maximum number of skill for character reached!");
	}

	bool GameCharacter::getSkillValue(UInt32 skillId, UInt16 & out_current, UInt16 & out_max) const
	{
		for (UInt8 i = 0; i < MaxSkills; ++i)
		{
			// Unit field values
			const UInt32 skillIndex = character_fields::SkillInfo1_1 + (i * 3);

			// Get current skill
			if (getUInt32Value(skillIndex) == skillId)
			{
				const UInt32 tmp = getUInt32Value(skillIndex + 1);
				out_current = UInt16(UInt32(tmp) & 0x0000FFFF);
				out_max = UInt16((UInt32(tmp) >> 16) & 0x0000FFFF);
				
				return true;
			}
		}

		return false;
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
		for (UInt8 i = 0; i < MaxSkills; ++i)
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

	void GameCharacter::updateWeaponSkill(UInt32 skillId)
	{
		// Check if we have that skill
		const proto::SkillEntry *weaponSkill = nullptr;
		for (const auto *skill : m_skills)
		{
			if (skill->id() == skillId)
			{
				weaponSkill = skill;
				break;
			}
		}

		if (!weaponSkill)
		{
			WLOG("Player doesn't know skill " << skillId);
			return;
		}

		if (weaponSkill->category() != game::skill_category::Weapon)
		{
			WLOG("Skill " << skillId << " - " << weaponSkill->name() << " is not a weapon skill");
			return;
		}

		// Get skill value
		UInt16 current = 0, max = 0;
		if (!getSkillValue(skillId, current, max))
		{
			WLOG("Could not get skill values for skill " << skillId);
			return;
		}

		// Something to increase?
		if (current == 0 || max == 0 || current >= max)
			return;

		// Intellect increases the chance to get a skill-levelup
		const UInt32 maxFactor = 512;
		const UInt32 minFactor = 50;
		const UInt32 bonusInt = getUInt32Value(unit_fields::Stat3) - getModifierValue(unit_mods::StatIntellect, unit_mod_type::BaseValue);
		const UInt32 levelInt = getLevel() * 5;

		// Calculate reduction factor in percent and clamp to 0..1
		float factor = static_cast<float>(bonusInt) / static_cast<float>(levelInt);	// 50 bonus int at level 20 would be 50 / 100
		factor = std::min(1.0f, std::max(0.0f, factor));

		// Perform a roll to see if we should increase the skill value
		std::uniform_int_distribution<UInt32> dist(0, maxFactor);
		const UInt32 roll = dist(randomGenerator);
		
		// Formular: current * (512 - (50 * 0..1)) < max * random(0,512)
		if (static_cast<UInt32>(current) * (maxFactor - (static_cast<float>(minFactor) * factor)) < static_cast<UInt32>(max) * roll)
		{
			// Increase skill value
			setSkillValue(skillId, current + 1, max);

			// Update combat ratings...
			updateAllRatings();
		}
	}

	void GameCharacter::setSkillValue(UInt32 skillId, UInt16 current, UInt16 maximum)
	{
		// Find the next usable skill slot
		for (UInt8 i = 0; i < MaxSkills; ++i)
		{
			// Unit field values
			const UInt32 skillIndex = character_fields::SkillInfo1_1 + (i * 3);

			// Get current skill
			if (getUInt32Value(skillIndex) == skillId)
			{
				const UInt32 minMaxValue = UInt32(UInt16(current) | (UInt32(maximum) << 16));
				setUInt32Value(skillIndex + 1, minMaxValue);
				return;
			}
		}

		WLOG("Character does not know skill " << skillId);
	}

	void GameCharacter::updateArmor()
	{
		float baseArmor = getModifierValue(unit_mods::Armor, unit_mod_type::BaseValue);

		// Apply equipment
		for (UInt8 i = player_equipment_slots::Start; i < player_equipment_slots::End; ++i)
		{
			auto item = m_inventory.getItemAtSlot(Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, i));
			if (item && (item->getEntry().durability() == 0 || item->getUInt32Value(item_fields::Durability) > 0))
			{
				// Add armor value from item
				baseArmor += item->getEntry().armor();
			}
		}

		const float basePct = getModifierValue(unit_mods::Armor, unit_mod_type::BasePct);
		const float totalArmor = getModifierValue(unit_mods::Armor, unit_mod_type::TotalValue);
		const float totalPct = getModifierValue(unit_mods::Armor, unit_mod_type::TotalPct);

		getAuras().forEachAuraOfType(game::aura_type::ModResistanceOfStatPercent, [&baseArmor, this](AuraEffect &aura) -> bool {
			baseArmor += getUInt32Value(unit_fields::Stat0 + aura.getEffect().miscvalueb()) * aura.getBasePoints() / 100.0f;
			return true;
		});

		float value = (baseArmor * basePct) + totalArmor;
		// Add armor from agility
		value += getUInt32Value(unit_fields::Stat1) * 2;
		value *= totalPct;

		setUInt32Value(unit_fields::Resistances, value);
		setUInt32Value(unit_fields::ResistancesBuffModsPositive, totalArmor > 0 ? UInt32(totalArmor) : 0);
		setUInt32Value(unit_fields::ResistancesBuffModsNegative, totalArmor < 0 ? UInt32(totalArmor) : 0);
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
				case game::shapeshift_form::Cat:
					atkPower = getModifierValue(unit_mods::AttackPower, unit_mod_type::BaseValue) + getUInt32Value(unit_fields::Stat0) * 2.0f + getUInt32Value(unit_fields::Stat1) - 20.0f;
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

			atkPower += getModifierValue(unit_mods::AttackPower, unit_mod_type::BaseValue);
			const float base_attPower = atkPower * getModifierValue(unit_mods::AttackPower, unit_mod_type::BasePct);
			const float attPowerMod = getModifierValue(unit_mods::AttackPower, unit_mod_type::TotalValue);
			const float attPowerMultiplier = getModifierValue(unit_mods::AttackPower, unit_mod_type::TotalPct) - 1.0f;	// In display, 0.0 = 100% (unmodified)
			setInt32Value(unit_fields::AttackPower, UInt32(base_attPower));
			setUInt16Value(unit_fields::AttackPowerMods, 0, attPowerMod > 0 ? UInt16(attPowerMod) : 0);
			setUInt16Value(unit_fields::AttackPowerMods, 1, attPowerMod < 0 ? UInt16(attPowerMod) : 0);
			setFloatValue(unit_fields::AttackPowerMultiplier, attPowerMultiplier);
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

			atkPower += getModifierValue(unit_mods::AttackPowerRanged, unit_mod_type::BaseValue);
			float base_attPower = atkPower * getModifierValue(unit_mods::AttackPowerRanged, unit_mod_type::BasePct);
			float attPowerMod = getModifierValue(unit_mods::AttackPowerRanged, unit_mod_type::TotalValue);
			float attPowerMultiplier = getModifierValue(unit_mods::AttackPowerRanged, unit_mod_type::TotalPct) - 1.0f;
			setInt32Value(unit_fields::RangedAttackPower, UInt32(base_attPower));
			setUInt16Value(unit_fields::RangedAttackPowerMods, 0, attPowerMod > 0 ? UInt16(attPowerMod) : 0);
			setUInt16Value(unit_fields::RangedAttackPowerMods, 1, attPowerMod < 0 ? UInt16(attPowerMod) : 0);
			setFloatValue(unit_fields::RangedAttackPowerMultiplier, attPowerMultiplier);
		}

		// Melee damage
		{
			float minDamage = 1.0f;
			float maxDamage = 2.0f;

			// TODO: Check druid form etc.

			UInt8 form = getByteValue(unit_fields::Bytes2, 3);
			if (!(form == game::shapeshift_form::Cat || form == game::shapeshift_form::Bear || form == game::shapeshift_form::DireBear))
			{
				// Check if we are wearing a weapon in our main hand
				auto item = m_inventory.getItemAtSlot(Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Mainhand));
				if (item)
				{
					// Get weapon damage values
					const auto &entry = item->getEntry();
					if (entry.damage(0).mindmg() != 0.0f) {
						minDamage = entry.damage(0).mindmg();
					}
					if (entry.damage(0).maxdmg() != 0.0f) {
						maxDamage = entry.damage(0).maxdmg();
					}
				}
			}

			const float att_speed = getUInt32Value(unit_fields::BaseAttackTime) / 1000.0f;
			const Int32 att_power = getInt32Value(unit_fields::AttackPower) + getUInt16Value(unit_fields::AttackPowerMods, 0) + getInt16Value(unit_fields::AttackPowerMods, 1);
			const float base_value = (att_power > 0 ? att_power : 0) / 14.0f * att_speed;

			switch (form)
			{
				case game::shapeshift_form::Cat:
				case game::shapeshift_form::Bear:
				case game::shapeshift_form::DireBear:
					minDamage = (level > 60 ? 60 : level) * 0.85f * att_speed;
					maxDamage = (level > 60 ? 60 : level) * 1.25f * att_speed;
					break;
				default:
					break;
			}

			setFloatValue(unit_fields::MinDamage, base_value + minDamage);
			setFloatValue(unit_fields::MaxDamage, base_value + maxDamage);
		}

		// Offhand damage
		{
			float minDamage = 1.0f;
			float maxDamage = 2.0f;

			// TODO: Check druid form etc.

			// Check if we are wearing a weapon in our main hand
			auto item = m_inventory.getItemAtSlot(Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Offhand));
			if (item)
			{
				// Get weapon damage values
				const auto &entry = item->getEntry();
				if (entry.damage(0).mindmg() != 0.0f) {
					minDamage = entry.damage(0).mindmg();
				}
				if (entry.damage(0).maxdmg() != 0.0f) {
					maxDamage = entry.damage(0).maxdmg();
				}
			}

			const float att_speed = getUInt32Value(unit_fields::BaseAttackTime + game::weapon_attack::OffhandAttack) / 1000.0f;
			const Int32 att_power = getInt32Value(unit_fields::AttackPower) + getUInt16Value(unit_fields::AttackPowerMods, 0) + getInt16Value(unit_fields::AttackPowerMods, 1);
			const float base_value = (att_power > 0 ? att_power : 0) / 14.0f * att_speed;

			setFloatValue(unit_fields::MinOffHandDamage, base_value + minDamage);
			setFloatValue(unit_fields::MaxOffHandDamage, base_value + maxDamage);
		}
		
		// Ranged damage
		{
			float minDamage = 1.0f;
			float maxDamage = 2.0f;

			// Check if we are wearing a weapon in the ranged slot
			auto item = m_inventory.getItemAtSlot(Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Ranged));
			if (item)
			{
				// Get weapon damage values
				const auto &entry = item->getEntry();
				if (entry.damage(0).mindmg() != 0.0f) {
					minDamage = entry.damage(0).mindmg();
				}
				if (entry.damage(0).maxdmg() != 0.0f) {
					maxDamage = entry.damage(0).maxdmg();
				}
			}

			setFloatValue(unit_fields::MinRangedDamage, minDamage);
			setFloatValue(unit_fields::MaxRangedDamage, maxDamage);
		}

		updateAttackSpeed();
		updateAllCritChances();
	}

	void GameCharacter::updateAttackSpeed()
	{
		// Melee attack speed
		{
			UInt32 attackTime = 2000;

			// TODO: Check druid form etc.

			UInt8 form = getByteValue(unit_fields::Bytes2, 3);
			if (form == game::shapeshift_form::Cat || form == game::shapeshift_form::Bear || form == game::shapeshift_form::DireBear)
			{
				attackTime = (form == game::shapeshift_form::Cat ? 1000 : 2500);
			}
			else
			{
				// Check if we are wearing a weapon in our main hand
				auto item = m_inventory.getItemAtSlot(Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Mainhand));
				if (item)
				{
					// Get weapon attack speed values
					const auto &entry = item->getEntry();
					if (entry.delay() != 0) 
					{
						attackTime = entry.delay();
					}
				}
			}

			const float att_speed = attackTime * getModifierValue(unit_mods::AttackSpeed, unit_mod_type::BasePct);

			setUInt32Value(unit_fields::BaseAttackTime, att_speed);
		}

		// Offhand attack speed
		{
			UInt32 attackTime = 2000;

			// TODO: Check druid form etc.

			// Check if we are wearing a weapon in our main hand
			auto item = m_inventory.getItemAtSlot(Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Offhand));
			if (item)
			{
				// Get weapon attack speed values
				const auto &entry = item->getEntry();
				if (entry.delay() != 0)
				{
					attackTime = entry.delay();
				}
			}

			const float att_speed = attackTime * getModifierValue(unit_mods::AttackSpeed, unit_mod_type::BasePct);

			setUInt32Value(unit_fields::BaseAttackTime + game::weapon_attack::OffhandAttack, att_speed);
		}

		// Ranged attack speed
		{
			UInt32 attackTime = 2000;

			// Check if we are wearing a weapon in the ranged slot
			auto item = m_inventory.getItemAtSlot(Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Ranged));
			if (item)
			{
				// Get weapon attack speed values
				const auto &entry = item->getEntry();
				if (entry.delay() != 0)
				{
					attackTime = entry.delay();
				}
			}

			const float att_speed = 
				(attackTime * getModifierValue(unit_mods::AttackSpeedRanged, unit_mod_type::BasePct) + 
					getModifierValue(unit_mods::AttackSpeedRanged, unit_mod_type::TotalValue)) * getModifierValue(unit_mods::AttackSpeedRanged, unit_mod_type::TotalPct);

			setUInt32Value(unit_fields::RangedAttackTime, att_speed);
		}
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

	void GameCharacter::applyItemStats(GameItem &item, bool apply)
	{
		if (item.getEntry().durability() == 0 || item.getUInt32Value(item_fields::Durability) > 0)
		{
			// Apply values
			for (int i = 0; i < item.getEntry().stats_size(); ++i)
			{
				const auto &entry = item.getEntry().stats(i);
				if (entry.value() != 0)
				{
					switch (entry.type())
					{
					case game::item_stat::Mana:
						updateModifierValue(unit_mods::Mana, unit_mod_type::TotalValue, entry.value(), apply);
						break;
					case game::item_stat::Health:
						updateModifierValue(unit_mods::Health, unit_mod_type::TotalValue, entry.value(), apply);
						break;
					case game::item_stat::Agility:
						updateModifierValue(unit_mods::StatAgility, unit_mod_type::TotalValue, entry.value(), apply);
						break;
					case game::item_stat::Strength:
						updateModifierValue(unit_mods::StatStrength, unit_mod_type::TotalValue, entry.value(), apply);
						break;
					case game::item_stat::Intellect:
						updateModifierValue(unit_mods::StatIntellect, unit_mod_type::TotalValue, entry.value(), apply);
						break;
					case game::item_stat::Spirit:
						updateModifierValue(unit_mods::StatSpirit, unit_mod_type::TotalValue, entry.value(), apply);
						break;
					case game::item_stat::Stamina:
						updateModifierValue(unit_mods::StatStamina, unit_mod_type::TotalValue, entry.value(), apply);
						break;
					case game::item_stat::DodgeRating:
						applyCombatRatingMod(combat_rating::Dodge, entry.value(), apply);
						break;
					case game::item_stat::ParryRating:
						applyCombatRatingMod(combat_rating::Parry, entry.value(), apply);
						break;
					case game::item_stat::CritMeleeRating:
						applyCombatRatingMod(combat_rating::CritMelee, entry.value(), apply);
						break;
					case game::item_stat::CritRangedRating:
						applyCombatRatingMod(combat_rating::CritRanged, entry.value(), apply);
						break;
					case game::item_stat::CritRating:
						applyCombatRatingMod(combat_rating::CritMelee, entry.value(), apply);
						applyCombatRatingMod(combat_rating::CritRanged, entry.value(), apply);
						break;
					case game::item_stat::CritSpellRating:
						applyCombatRatingMod(combat_rating::CritSpell, entry.value(), apply);
						break;
					default:
						break;
					}
				}
			}

			std::array<bool, 6> shouldUpdateResi;
			shouldUpdateResi.fill(false);

			if (item.getEntry().holyres() != 0)
			{
				updateModifierValue(unit_mods::ResistanceHoly, unit_mod_type::TotalValue, item.getEntry().holyres(), apply);
				shouldUpdateResi[0] = true;
			}
			if (item.getEntry().fireres() != 0)
			{
				updateModifierValue(unit_mods::ResistanceFire, unit_mod_type::TotalValue, item.getEntry().fireres(), apply);
				shouldUpdateResi[1] = true;
			}
			if (item.getEntry().natureres() != 0)
			{
				updateModifierValue(unit_mods::ResistanceNature, unit_mod_type::TotalValue, item.getEntry().natureres(), apply);
				shouldUpdateResi[2] = true;
			}
			if (item.getEntry().frostres() != 0)
			{
				updateModifierValue(unit_mods::ResistanceFrost, unit_mod_type::TotalValue, item.getEntry().frostres(), apply);
				shouldUpdateResi[3] = true;
			}
			if (item.getEntry().shadowres() != 0)
			{
				updateModifierValue(unit_mods::ResistanceShadow, unit_mod_type::TotalValue, item.getEntry().shadowres(), apply);
				shouldUpdateResi[4] = true;
			}
			if (item.getEntry().arcaneres() != 0)
			{
				updateModifierValue(unit_mods::ResistanceArcane, unit_mod_type::TotalValue, item.getEntry().arcaneres(), apply);
				shouldUpdateResi[5] = true;
			}

			for (UInt32 resistMod = 1; resistMod <= shouldUpdateResi.size(); ++resistMod)
			{
				if (shouldUpdateResi[resistMod - 1])
				{
					updateResistance(resistMod);
				}
			}

			if (apply)
			{
				SpellTargetMap targetMap;
				targetMap.m_unitTarget = getGuid();
				targetMap.m_targetMap = game::spell_cast_target_flags::Unit;

				for (auto &spell : item.getEntry().spells())
				{
					// Trigger == onEquip?
					if (spell.trigger() == game::item_spell_trigger::OnEquip)
					{
						castSpell(targetMap, spell.spell(), { 0, 0, 0 }, 0, true, item.getGuid());
					}
				}
			}
			else
			{
				getAuras().removeAllAurasDueToItem(item.getGuid());
			}

			updateArmor();
			updateDamage();
		}
	}

	void GameCharacter::updateManaRegen()
	{
		const float intellect = getUInt32Value(unit_fields::Stat3);
		const float spirit = getUInt32Value(unit_fields::Stat4) * m_manaRegBase;
		const float regen = sqrtf(intellect) * spirit;

		const float mp5Reg = getAuras().getTotalBasePoints(game::aura_type::ModPowerRegen) * 0.2f;

		const float modManaRegenInterrupt = getAuras().getTotalBasePoints(game::aura_type::ModManaRegenInterrupt) / 100.0f;
		setFloatValue(character_fields::ModManaRegenInterrupt, regen * modManaRegenInterrupt + mp5Reg);
		setFloatValue(character_fields::ModManaRegen, regen + mp5Reg);
	}

	void GameCharacter::rewardExperience(GameUnit *victim, UInt32 experience)
	{
		// Get current experience points
		UInt32 currentXP = getUInt32Value(character_fields::Xp);
		UInt32 nextLevel = getUInt32Value(character_fields::NextLevelXp);
		UInt32 level = getLevel();

		// Nothing to do here
		if (nextLevel == 0) {
			return;
		}

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

	bool GameCharacter::canDetectStealth(GameUnit & target) const
	{
		// Characters in the same party can see each other
		if (target.isGameCharacter())
		{
			if (reinterpret_cast<GameCharacter&>(target).getGroupId() == m_groupId)
				return true;
		}

		return GameUnit::canDetectStealth(target);
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

		const UInt16 slot = Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Offhand);
		auto item = m_inventory.getItemAtSlot(slot);
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

		return hasMainHandWeapon();
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
		if (baseSpirit > 50.0f) {
			baseSpirit = 50.0f;
		}
		float moreSpirit = spirit - baseSpirit;
		float spiritRegen = baseSpirit * m_healthRegBase + moreSpirit * m_healthRegBase;
		float addHealth = spiritRegen * healthRegRate;

		UInt8 standState = getByteValue(unit_fields::Bytes1, 0);
		if (standState != unit_stand_state::Stand &&
		        standState != unit_stand_state::Dead)
		{
			// 33% more regeneration when not standing
			addHealth *= 1.33f;
		}

		// No regeneration in combat
		if (isInCombat())
			addHealth = 0.0f;

		if (getAuras().hasAura(game::aura_type::ModRegen))
			addHealth += getAuras().getTotalBasePoints(game::aura_type::ModRegen) * 0.4f;

		// Apply regeneration which continues even in combat
		if (getAuras().hasAura(game::aura_type::ModHealthRegenInCombat))
			addHealth += (getAuras().getTotalBasePoints(game::aura_type::ModHealthRegenInCombat) * 0.4f);

		// Anything to regenerate here?
		if (addHealth < 0.0f)
			return;

		heal(static_cast<UInt32>(addHealth), nullptr, true);
	}

	void GameCharacter::onThreat(GameUnit & threatener, float amount)
	{
		// We don't threat ourself
		if (&threatener == this)
			return;

		// PvP combat?
		if (threatener.isGameCharacter())
		{
			// Mark as "in combat" and setup timer
			addFlag(unit_fields::UnitFlags, game::unit_flags::InCombat);

			// Threatener will also be marked as "in combat"
			threatener.addFlag(unit_fields::UnitFlags, game::unit_flags::InCombat);

			// Toggle pvp reset timer in the next 6 seconds
			GameTime pvpTime = getCurrentTime();
			m_lastPvPCombat = pvpTime;
			reinterpret_cast<GameCharacter&>(threatener).m_lastPvPCombat = pvpTime;
		}
	}

	void GameCharacter::onRegeneration()
	{
		GameUnit::onRegeneration();

		if (isInCombat() && !isInPvPCombat())
		{
			// If not in PvE-Combat, we are no longer in combat at all after this timer expires
			if (!hasAttackingUnits())
			{
				removeFlag(unit_fields::UnitFlags, game::unit_flags::InCombat);
			}
		}
	}

	UInt32 GameCharacter::getWeaponSkillValue(game::WeaponAttack attackType, const GameUnit &target) const
	{
		// Try to get the respective weapon
		std::shared_ptr<GameItem> item = m_inventory.getWeaponByAttackType(attackType, true, true);
		if (attackType != game::weapon_attack::BaseAttack && !item)
		{
			// Not sure about this one... needs research
			return 0;
		}

		// Feral druids don't use weapon skills so always return max here for calculations
		if (isInFeralForm())
		{
			return getMaxWeaponSkillValueForLevel();
		}

		// Get the weapon skill index to check
		const UInt32 skillId = item ? item->getEntry().skill() : static_cast<UInt32>(game::skill_type::Unarmed);

		// Request current weapon skill (or unarmed skill of no weapon)
		UInt16 maxSkillValue = 1, skillValue = 1;
		if (!getSkillValue(skillId, skillValue, maxSkillValue))
			return 1;

		// TODO: Use max skill value against players (pvp), real skill value otherwise
		UInt32 value = skillValue;//(target.isGameCharacter() ? maxSkillValue : skillValue);

		// Increase value based on combat ratings (this seems completely useless)
		CombatRatingType combatRating = combat_rating::WeaponSkill;
		value += getRatingBonusValue(combatRating);

		switch (attackType)
		{
			case game::weapon_attack::BaseAttack:
				combatRating = combat_rating::WeaponSkillMainhand;
				value += getRatingBonusValue(combatRating);
			case game::weapon_attack::OffhandAttack:
				combatRating = combat_rating::WeaponSkillOffhand;
				value += getRatingBonusValue(combatRating);
			case game::weapon_attack::RangedAttack:
				combatRating = combat_rating::WeaponSkillRanged;
				value += getRatingBonusValue(combatRating);
		}

		return value;
	}

	UInt32 GameCharacter::getDefenseSkillValue(const GameUnit &attacker) const 
	{
		// In case of failure of getSkillValue, initialize these
		UInt16 maxSkillValue = getMaxWeaponSkillValueForLevel(), skillValue = 1;
		getSkillValue(game::skill_type::Defense, skillValue, maxSkillValue);

		// TODO: Use max skill value against players (pvp), real skill value otherwise
		UInt32 value = skillValue;//(target.isGameCharacter() ? maxSkillValue : skillValue);

		// TODO: DefenseSkill value has to be implemented
		value += static_cast<UInt32>(getRatingBonusValue(combat_rating::DefenseSkill));

		return value;
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
		Int32 level = static_cast<Int32>(getLevel());
		ASSERT(level > 0);

		Int32 talentPoints = 0;
		if (level >= 10)
		{
			// This is the maximum number of talent points available at the current character level
			talentPoints = level - 9;
			if (talentPoints > 0)
			{
				// Now iterate through every learned spell and reduce the amount of talent points
				for (auto &spell : m_spells)
				{
					talentPoints -= spell->talentcost();
				}

				if (talentPoints < 0)
					talentPoints = 0;
			}
		}

		setInt32Value(character_fields::CharacterPoints_1, talentPoints);
	}

	void GameCharacter::initClassEffects()
	{
		m_doneMeleeAttack.disconnect();

		switch (getClass())
		{
		case game::char_class::Warrior:
			// Hard coded overpower proc for warrior: Blizzard implemented this with combo points
			// When the target dodges, the warrior simply gets a combo point.
			// Since overpower uses all combo points (just like all finishing moves for rogues and ferals),
			// it doesn't matter if we add more than one combo point to the target.
			// Hard coded: TODO proper implementation
			m_doneMeleeAttack = doneMeleeAttack.connect(
			[this](GameUnit * victim, game::VictimState victimState) {
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

	void GameCharacter::updateNearbyQuestObjects()
	{
		TileIndex2D tile;
		if (!getTileIndex(tile))
		{
			// Not in a world instance
			WLOG("Character is not in a world instance");
			return;
		}

		// m_worldInstance is valid because we got a tile index
		std::vector<std::vector<char>> objectUpdateBlocks;
		forEachTileInSight(
		    m_worldInstance->getGrid(),
		    tile,
		    [this, &objectUpdateBlocks](const VisibilityTile & tile)
		{
			for (auto &object : tile.getGameObjects())
			{
				if (object->isWorldObject())
				{
					auto loot = reinterpret_cast<WorldObject *>(object)->getObjectLoot();
					bool isPotentialQuestObject = (loot && !loot->isEmpty());

					// Check if object has loot or is a potential quest object
					if (!isPotentialQuestObject)
					{
						const auto& entry = reinterpret_cast<WorldObject *>(object)->getEntry();
						if (entry.type() == world_object_type::Goober)
						{
							const UInt32 questData = entry.data_size() > 1 ? entry.data(1) : 0;
							isPotentialQuestObject = (questData != 0);
						}
					}

					if (isPotentialQuestObject)
					{
						// Force update of DynamicFlags field (TODO: This update only needs to be sent to OUR client,
						// as every client will have a different DynamicFlags field)
						object->forceFieldUpdate(world_object_fields::DynFlags);
					}
				}
			}
		});
	}

	float GameCharacter::getMeleeCritFromAgility()
	{
		UInt32 level = getLevel();
		UInt32 charClass = getClass();

		if (level > 100)
		{
			level = 100;
		}

		const auto *critEntry = getProject().meleeCritChance.getById((charClass - 1) * 100 + level - 1);
		ASSERT(critEntry);

		const float critBase = critEntry->basechanceperlevel();
		const float critRatio = critEntry->chanceperlevel();

		return (critBase + getUInt32Value(unit_fields::Stat0 + unit_mods::StatAgility) * critRatio) * 100.0f;
	}

	float GameCharacter::getDodgeFromAgility()
	{
		UInt32 level = getLevel();
		UInt32 charClass = getClass();

		if (level > 100)
		{
			level = 100;
		}

		const auto &project = getProject();
		const auto *dodgeEntry = project.meleeCritChance.getById((charClass - 1) * 100 + level - 1);
		ASSERT(dodgeEntry);

		const auto * classDodge = project.dodgeChance.getById(charClass - 1);
		ASSERT(classDodge);

		const float baseDodge = classDodge->basedodge();
		const float critToDodge = classDodge->crittododge();

		return baseDodge + 
			getUInt32Value(unit_fields::Stat0 + unit_mods::StatAgility) * 
			dodgeEntry->chanceperlevel() * 
			critToDodge *
			100.0f;
	}

	float GameCharacter::getSpellCritFromIntellect()
	{
		UInt32 level = getLevel();
		UInt32 charClass = getClass();

		if (level > 100)
		{
			level = 100;
		}

		const auto *critEntry = getProject().spellCritChance.getById((charClass - 1) * 100 + level - 1);
		ASSERT(critEntry);

		const float critBase = critEntry->basechanceperlevel();
		const float critRatio = critEntry->chanceperlevel();
		return (critBase + getUInt32Value(unit_fields::Stat0 + unit_mods::StatIntellect) * critRatio) * 100.0f;
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

	bool GameCharacter::hasMainHandWeapon() const
	{
		auto item = getMainHandWeapon();
		return item != nullptr;
	}

	bool GameCharacter::hasOffHandWeapon() const
	{
		auto item = getOffHandWeapon();
		return (item && item->getEntry().inventorytype() != game::inventory_type::Shield && item->getEntry().inventorytype() != game::inventory_type::Holdable);
	}

	std::shared_ptr<GameItem> GameCharacter::getMainHandWeapon() const
	{
		return m_inventory.getItemAtSlot(Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Mainhand));
	}

	std::shared_ptr<GameItem> GameCharacter::getOffHandWeapon() const
	{
		return m_inventory.getItemAtSlot(Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Offhand));
	}

	io::Writer &operator<<(io::Writer &w, GameCharacter const &object)
	{
		// Write super class data
		w << reinterpret_cast<GameUnit const &>(object);

		// Write character-specific values
		w
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
			<< object.m_inventory
			<< io::write<NetUInt64>(object.m_groupId);

		// Write spell data
		w << io::write<NetUInt16>(object.m_spells.size());
		for (const auto &spell : object.m_spells)
		{
			w << io::write<NetUInt32>(spell->id());
		}

		// Write quest data
		w << io::write<NetUInt16>(object.m_quests.size());
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

		// Write play-time
		w << io::write_range(object.m_playedTime);

		// Write aura data
		const auto& auraData = object.getAuraData();

		w << io::write<NetUInt16>(auraData.size());
		for (const auto& data : auraData)
		{
			w
				<< io::write<NetUInt64>(data.casterGuid)
				<< io::write<NetUInt64>(data.itemGuid)
				<< io::write<NetUInt32>(data.spell)
				<< io::write<NetUInt32>(data.stackCount)
				<< io::write<NetUInt32>(data.remainingCharges)
				<< io::write_range(data.basePoints)
				<< io::write_range(data.periodicTime)
				<< io::write<NetUInt32>(data.maxDuration)
				<< io::write<NetUInt32>(data.remainingTime)
				<< io::write<NetUInt32>(data.effectIndexMask)
				;
		}

		return w;
	}

	io::Reader &operator>>(io::Reader &r, GameCharacter &object)
	{
		object.initialize();
		
		// Read super class values
		r >> reinterpret_cast<GameUnit &>(object);

		// Read character specific values
		r
			>> io::read_container<NetUInt8>(object.m_name)
			>> io::read<NetUInt32>(object.m_zoneIndex)
			>> io::read<float>(object.m_healthRegBase)
			>> io::read<float>(object.m_manaRegBase)
			>> io::read<NetUInt64>(object.m_groupId)
			>> io::read<NetUInt32>(object.m_homeMap)
			>> io::read<float>(object.m_homePos[0])
			>> io::read<float>(object.m_homePos[1])
			>> io::read<float>(object.m_homePos[2])
			>> io::read<float>(object.m_homeRotation)
			>> object.m_inventory
			>> io::read<NetUInt64>(object.m_groupId);

		// Add skills to the list of known skills
		for (UInt8 i = 0; i < MaxSkills; ++i)
		{
			const UInt16 skillIndex = character_fields::SkillInfo1_1 + (i * 3);
			const UInt32 skillId = object.getUInt32Value(skillIndex);
			if (skillId != 0 && !object.hasSkill(skillId))
			{
				const auto *skill = object.getProject().skills.getById(skillId);
				if (skill)
				{
					object.m_skills.push_back(skill);
				}
			}
		}

		// Read spell values
		UInt16 spellCount = 0;
		r >> io::read<NetUInt16>(spellCount);
		object.m_spells.clear();
		for (UInt16 i = 0; i < spellCount; ++i)
		{
			UInt32 spellId = 0;
			if (!(r >> io::read<NetUInt32>(spellId)))
				return r;

			// Add character spell
			const auto *spell = object.getProject().spells.getById(spellId);
			if (spell)
			{
				object.addSpell(*spell);
			}
		}

		// Read quest values
		UInt16 questCount = 0;
		r >> io::read<NetUInt16>(questCount);
		object.m_quests.clear();
		for (UInt16 i = 0; i < questCount; ++i)
		{
			UInt32 questId = 0;
			r >> io::read<NetUInt32>(questId);
			auto &questData = object.m_quests[questId];
			if (!(r
				>> io::read<NetUInt8>(questData.status)
				>> io::read<NetUInt64>(questData.expiration)
				>> io::read<NetUInt8>(questData.explored)
				>> io::read_range(questData.creatures)
				>> io::read_range(questData.objects)
				>> io::read_range(questData.items)))
			{
				return r;
			}

			// Quest item cache update
			if (questData.status == game::quest_status::Incomplete)
			{
				const auto *quest = object.getProject().quests.getById(questId);
				if (quest)
				{
					for (const auto &req : quest->requirements())
					{
						if (req.itemid())
						{
							object.m_requiredQuestItems[req.itemid()]++;
						}
						if (req.sourceid())
						{
							object.m_requiredQuestItems[req.sourceid()]++;
						}
					}
				}
			}
		}

		// Read play-time
		r >> io::read_range(object.m_playedTime);

		// Clear all auras
		object.m_auraData.clear();

		// Read aura count
		UInt16 auraCount = 0;
		r >> io::read<NetUInt16>(auraCount);
		
		// Resize array
		object.m_auraData.resize(auraCount);
		for (UInt16 i = 0; i < auraCount; ++i)
		{
			AuraData &auraData = object.m_auraData[i];
			if (!(r
				>> io::read<NetUInt64>(auraData.casterGuid)
				>> io::read<NetUInt64>(auraData.itemGuid)
				>> io::read<NetUInt32>(auraData.spell)
				>> io::read<NetUInt32>(auraData.stackCount)
				>> io::read<NetUInt32>(auraData.remainingCharges)
				>> io::read_range(auraData.basePoints)
				>> io::read_range(auraData.periodicTime)
				>> io::read<NetUInt32>(auraData.maxDuration)
				>> io::read<NetUInt32>(auraData.remainingTime)
				>> io::read<NetUInt32>(auraData.effectIndexMask)))
			{
				return r;
			}
		}

		// =============================================================
		// End of reading
		// =============================================================

		// Reset all auras
		for (UInt32 i = 0; i < 56; ++i)
		{
			object.setUInt32Value(unit_fields::AuraEffect + i, 0);
		}

		// Reset all buffs
		object.setUInt32Value(character_fields::ModHealingDonePos, 0);
		for (UInt8 i = 0; i < 7; ++i)
		{
			object.setUInt32Value(character_fields::ModDamageDoneNeg + i, 0);
			object.setUInt32Value(character_fields::ModDamageDonePos + i, 0);
			object.setFloatValue(character_fields::ModDamageDonePct + i, 1.00f);
		}

		// Reset mount
		//object.setUInt32Value(unit_fields::MountDisplayId, 0);

		// Remove "InCombat" flag
		object.removeFlag(unit_fields::UnitFlags, game::unit_flags::InCombat);

		// Remember health and powers
		UInt32 health = object.getUInt32Value(unit_fields::Health);
		UInt32 power[5];
		for (Int32 i = 0; i < 5; ++i)
		{
			power[i] = object.getUInt32Value(unit_fields::Power1 + i);
		}

		// Update all stats
		object.setLevel(object.getLevel());

		// Overwrite health and powers for later use
		object.setUInt32Value(unit_fields::Health, health);
		for (Int32 i = 0; i < 5; ++i)
		{
			object.setUInt32Value(unit_fields::Power1 + i, power[i]);
		}

		// Update movement info
		object.m_movementInfo.x = object.m_position.x;
		object.m_movementInfo.y = object.m_position.y;
		object.m_movementInfo.z = object.m_position.z;
		object.m_movementInfo.o = object.m_o;

		return r;
	}
}
