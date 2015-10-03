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

#include "game_creature.h"
#include "game_character.h"
#include "data/trigger_entry.h"
#include "data/item_entry.h"
#include "data/faction_template_entry.h"
#include "world_instance.h"
#include "tiled_unit_watcher.h"
#include "unit_finder.h"
#include "each_tile_in_sight.h"
#include "binary_io/vector_sink.h"
#include "log/default_log_levels.h"
#include "common/make_unique.h"
#include "creature_ai.h"

namespace wowpp
{
	GameCreature::GameCreature(
		TimerQueue &timers, 
		DataLoadContext::GetRace getRace, 
		DataLoadContext::GetClass getClass, 
		DataLoadContext::GetLevel getLevel, 
		DataLoadContext::GetSpell getSpell,
		const UnitEntry &entry)
		: GameUnit(timers, getRace, getClass, getLevel, getSpell)
		, m_originalEntry(entry)
		, m_entry(nullptr)
	{
	}

	void GameCreature::initialize()
	{
		// Initialize the unit
		GameUnit::initialize();

		// Setup entry
		setEntry(m_originalEntry);

		// TODO: We need to determine the creatures home point somehow

		// Setup AI
		m_ai = make_unique<CreatureAI>(
			*this, CreatureAI::Home(makeVector(0.0f, 0.0f, 0.0f)));

		// Start regeneration
		m_onSpawned = spawned.connect([this]()
		{
			CreatureAI::Home home(makeVector(0.0f, 0.0f, 0.0f));
			getLocation(home.position[0], home.position[1], home.position[2], home.orientation);

			m_ai->setHome(std::move(home));
			startRegeneration();
		});
	}

	void GameCreature::setEntry(const UnitEntry &entry)
	{
		const bool isInitialize = (m_entry == nullptr);

		// Choose a level
		UInt8 creatureLevel = entry.minLevel;
		if (entry.maxLevel != entry.minLevel)
		{
			std::uniform_int_distribution<int> levelDistribution(entry.minLevel, entry.maxLevel);
			creatureLevel = levelDistribution(randomGenerator);
		}

		// calculate interpolation factor
		const float t =
			(entry.maxLevel != entry.minLevel) ?
			(creatureLevel - entry.minLevel) / (entry.maxLevel - entry.minLevel) :
			0.0f;

		// Randomize gender
		std::uniform_int_distribution<int> genderDistribution(0, 1);
		int gender = genderDistribution(randomGenerator);

		// TODO
		assert(entry.allianceFaction);
		setFactionTemplate(*entry.allianceFaction);

		setLevel(creatureLevel);
		setClass(entry.unitClass);
		setUInt32Value(object_fields::Entry, entry.id);
		setFloatValue(object_fields::ScaleX, entry.scale);
		setUInt32Value(unit_fields::FactionTemplate, entry.hordeFaction->id);
		setGender(static_cast<game::Gender>(gender));
		setUInt32Value(unit_fields::DisplayId, (gender == game::gender::Male ? entry.maleModel : entry.femaleModel));
		setUInt32Value(unit_fields::NativeDisplayId, (gender == game::gender::Male ? entry.maleModel : entry.femaleModel));
		setUInt32Value(unit_fields::BaseHealth, interpolate(entry.minLevelHealth, entry.maxLevelHealth, t));
		setUInt32Value(unit_fields::MaxHealth, getUInt32Value(unit_fields::BaseHealth));
		setUInt32Value(unit_fields::Health, getUInt32Value(unit_fields::BaseHealth));
		setUInt32Value(unit_fields::MaxPower1, interpolate(entry.minLevelMana, entry.maxLevelMana, t));
		setUInt32Value(unit_fields::Power1, getUInt32Value(unit_fields::MaxPower1));
		setUInt32Value(unit_fields::UnitFlags, entry.unitFlags);
		setUInt32Value(unit_fields::DynamicFlags, entry.dynamicFlags);
		setUInt32Value(unit_fields::NpcFlags, entry.npcFlags);
		setByteValue(unit_fields::Bytes2, 1, 16);
		setByteValue(unit_fields::Bytes2, 0, 1);		// Sheath State: Melee weapon
		setUInt32Value(unit_fields::AttackPower, entry.attackPower);
		setUInt32Value(unit_fields::RangedAttackPower, entry.rangedAttackPower);
		setFloatValue(unit_fields::MinDamage, entry.minMeleeDamage);
		setFloatValue(unit_fields::MaxDamage, entry.maxMeleeDamage);
		setUInt32Value(unit_fields::BaseAttackTime, entry.meleeBaseAttackTime);

		// Update power type
		setByteValue(unit_fields::Bytes0, 3, power_type::Mana);
		if (entry.maxLevelMana > 0)
		{
			setByteValue(unit_fields::Bytes1, 1, 0xEE);
		}
		else
		{
			setByteValue(unit_fields::Bytes1, 1, 0x00);
		}

		setVirtualItem(0, entry.mainHand);
		setVirtualItem(1, entry.offHand);
		setVirtualItem(2, entry.ranged);

		// Unit Mods
		for (UInt32 i = unit_mods::StatStart; i < unit_mods::StatEnd; ++i)
		{
			setModifierValue(static_cast<UnitMods>(i), unit_mod_type::BaseValue, 0.0f);
			setModifierValue(static_cast<UnitMods>(i), unit_mod_type::TotalValue, 0.0f);
			setModifierValue(static_cast<UnitMods>(i), unit_mod_type::BasePct, 1.0f);
			setModifierValue(static_cast<UnitMods>(i), unit_mod_type::TotalPct, 1.0f);
		}
		setModifierValue(unit_mods::Armor, unit_mod_type::BaseValue, entry.armor);

		// Setup new entry
		m_entry = &entry;

		// Emit signal
		if (!isInitialize)
		{
			entryChanged();
		}

		updateAllStats();
	}

	void GameCreature::onKilled(GameUnit *killer)
	{
		GameUnit::onKilled(killer);
		
		// Decide whether to despawn based on unit type
		const bool isElite = (m_entry->rank > 0 && m_entry->rank < 4);
		const bool isRare = (m_entry->rank == 4);

		// Calculate despawn delay for rare mobs and elite mobs
		GameTime despawnDelay = constants::OneSecond * 30;
		if (isElite || isRare) despawnDelay = constants::OneMinute * 3;

		// Despawn in 30 seconds
		triggerDespawnTimer(despawnDelay);
	}

	float GameCreature::calcXpModifier(UInt32 attackerLevel) const
	{
		// No experience points
		const UInt32 level = getLevel();
		if (level < getGrayLevel(attackerLevel))
			return 0.0f;

		const UInt32 ZD = getZeroDiffXPValue(attackerLevel);
		if (level < attackerLevel)
		{
			return (1.0f - (attackerLevel - level) / ZD);
		}
		else
		{
			return (1.0f + 0.05f * (level - attackerLevel));
		}
	}

	const String & GameCreature::getName() const
	{
		if (!m_entry)
		{
			return m_originalEntry.name;
		}
		
		return m_entry->name;
	}

	void GameCreature::setVirtualItem(UInt32 slot, const ItemEntry *item)
	{
		if (!item)
		{
			setUInt32Value(unit_fields::VirtualItemSlotDisplay + slot, 0);
			setUInt32Value(unit_fields::VirtualItemInfo + (slot * 2) + 0, 0);
			setUInt32Value(unit_fields::VirtualItemInfo + (slot * 2) + 1, 0);
			return;
		}

		setUInt32Value(unit_fields::VirtualItemSlotDisplay + slot, item->displayId);
		setByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 0, 0, item->itemClass);
		setByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 0, 1, item->subClass);
		setByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 0, 2, 0xFF);
		setByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 0, 3, item->material);
		setByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 1, 0, item->inventoryType);
		setByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 1, 1, item->sheath);
	}

// 	void GameCreature::updateVictim()
// 	{
// 		if (!isAlive())
// 			return;
// 
// 		GameUnit *newVictim = nullptr;
// 		float maxThreat = -1.0;
// 
// 		for (auto it : m_threat)
// 		{
// 			if (it.second > maxThreat)
// 			{
// 				// Try to find unit
// 				newVictim = dynamic_cast<GameUnit*>(m_worldInstance->findObjectByGUID(it.first));
// 				if (newVictim)
// 				{
// 					maxThreat = it.first;
// 				}
// 			}
// 		}
// 
// 		if (newVictim &&
// 			newVictim != getVictim())
// 		{
// 			if (!getVictim())
// 			{
// 				// Aggro event
// 				auto it = m_entry->triggersByEvent.find(trigger_event::OnAggro);
// 				if (it != m_entry->triggersByEvent.end())
// 				{
// 					for (const auto *trigger : it->second)
// 					{
// 						trigger->execute(*trigger, this);
// 					}
// 				}
// 
// 				// Send aggro message to all clients nearby (this will play the creatures aggro sound)
// 				TileIndex2D tile;
// 				if (getTileIndex(tile))
// 				{
// 					std::vector<char> buffer;
// 					io::VectorSink sink(buffer);
// 					game::Protocol::OutgoingPacket packet(sink);
// 					game::server_write::aiReaction(packet, getGuid(), 2);
// 
// 					forEachSubscriberInSight(
// 						m_worldInstance->getGrid(), 
// 						tile,
// 						[&packet, &buffer](ITileSubscriber &subscriber)
// 					{
// 						subscriber.sendPacket(packet, buffer);
// 					});
// 				}
// 			}
// 
// 			/* THIS WORKS! (kind of) */
// 			TileIndex2D tile;
// 			if (getTileIndex(tile))
// 			{
// 				float o;
// 				Vector<float, 3> oldPosition;
// 				getLocation(oldPosition[0], oldPosition[1], oldPosition[2], o);
// 
// 				Vector<float, 3> newPosition;
// 				newVictim->getLocation(newPosition[0], newPosition[1], newPosition[2], o);
// 
// 				const float dist = getDistanceTo(*newVictim);
// 				const float speed = 7.5f;
// 
// 				std::vector<char> buffer;
// 				io::VectorSink sink(buffer);
// 				game::Protocol::OutgoingPacket packet(sink);
// 				game::server_write::monsterMove(packet, getGuid(), oldPosition, newPosition, (dist / speed) * 1000);
// 
// 				forEachSubscriberInSight(
// 					m_worldInstance->getGrid(),
// 					tile,
// 					[&packet, &buffer](ITileSubscriber &subscriber)
// 				{
// 					subscriber.sendPacket(packet, buffer);
// 				});
// 			}
// 
// 			// New target to attack
// 			startAttack(*newVictim);
// 		}
// 		else if (!newVictim)
// 		{
// 			// No target any more
// 			stopAttack();
// 		}
// 	}

	bool GameCreature::canBlock() const
	{
		const UInt32 slot = 1;
		const auto invType = getByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 1, 0);

		// Creatures can only block if they wield a shield
		return (invType == inventory_type::Shield);
	}

	bool GameCreature::canParry() const
	{
		const UInt32 slot = 0;
		const auto invType = getByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 1, 0);

		return (invType != 0);
	}

	bool GameCreature::canDodge() const
	{
		return true;
	}

	void GameCreature::updateDamage()
	{
		const float att_speed = static_cast<float>(getUInt32Value(unit_fields::BaseAttackTime)) / 1000.0f;
		const float base_value = static_cast<float>(getUInt32Value(unit_fields::AttackPower)) / 14.0f * att_speed;

		const UnitEntry *entry = (m_entry ? m_entry : &m_originalEntry);
		setFloatValue(unit_fields::MinDamage, base_value + entry->minMeleeDamage);
		setFloatValue(unit_fields::MaxDamage, base_value + entry->maxMeleeDamage);
	}

	void GameCreature::regenerateHealth()
	{
		const UInt32 maxHealth = getUInt32Value(unit_fields::MaxHealth);
		const UInt32 addHealth = maxHealth / 3;
		heal(addHealth, nullptr, false);
	}

	void GameCreature::addLootRecipient(UInt64 guid)
	{
		m_lootRecipients.add(guid);
	}

	bool GameCreature::isLootRecipient(GameCharacter &character) const
	{
		return m_lootRecipients.contains(character.getGuid());
	}

	void GameCreature::removeLootRecipients()
	{
		m_lootRecipients.clear();
	}

	UInt32 getZeroDiffXPValue(UInt32 killerLevel)
	{
		if (killerLevel < 8)
			return 5;
		else if (killerLevel < 10)
			return 6;
		else if (killerLevel < 12)
			return 7;
		else if (killerLevel < 16)
			return 8;
		else if (killerLevel < 20)
			return 9;
		else if (killerLevel < 30)
			return 11;
		else if (killerLevel < 40)
			return 12;
		else if (killerLevel < 45)
			return 13;
		else if (killerLevel < 50)
			return 14;
		else if (killerLevel < 55)
			return 15;
		else if (killerLevel < 60)
			return 16;

		return 17;
	}

	UInt32 getGrayLevel(UInt32 killerLevel)
	{
		if (killerLevel < 6)
			return 0;
		else if (killerLevel < 50)
			return killerLevel - ::floor(killerLevel / 10) - 5;
		else if (killerLevel == 50)
			return killerLevel - 10;
		else if (killerLevel < 60)
			return killerLevel - ::floor(killerLevel / 5) - 1;
		
		return killerLevel - 9;
	}

}
