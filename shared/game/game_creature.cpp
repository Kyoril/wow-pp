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

#include "game_creature.h"
#include "data/trigger_entry.h"
#include "data/item_entry.h"
#include "world_instance.h"
#include "tiled_unit_watcher.h"
#include "unit_finder.h"
#include "log/default_log_levels.h"

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
		// TODO: Put the code below into an AI class
		// This can only be executed when the unit is spawned (so that it is added to a world instance)
		spawned.connect([this]()
		{
			float x, y, z, o;
			this->getLocation(x, y, z, o);

			Circle circle(x, y, 20.0f);
			m_aggroWatcher = m_worldInstance->getUnitFinder().watchUnits(circle);
			m_aggroWatcher->visibilityChanged.connect([this](GameUnit& unit, bool isVisible) -> bool
			{
				if (&unit == this)
				{
					return false;
				}

				if (!isAlive())
				{
					return false;
				}

				if (unit.getTypeId() == object_type::Character)
				{
					if (isVisible)
					{
						// TODO: Determine whether the unit is hostile and we should attack that unit

						// Start attacking that unit
						addThreat(unit, 0.0001f);
						return true;
					}
					else
					{
						// Stop attacking that target.
						resetThreat(unit);
						return false;
					}
				}

				return false;
			});

			m_aggroWatcher->start();
		});
	}

	void GameCreature::initialize()
	{
		// Initialize the unit
		GameUnit::initialize();

		setEntry(m_originalEntry);
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

		setLevel(creatureLevel);
		setClass(entry.unitClass);
		setUInt32Value(object_fields::Entry, entry.id);
		setFloatValue(object_fields::ScaleX, entry.scale);
		setUInt32Value(unit_fields::FactionTemplate, entry.hordeFactionID);
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
		setFloatValue(unit_fields::MinDamage, entry.minMeleeDamage);
		setFloatValue(unit_fields::MaxDamage, entry.maxMeleeDamage);
		setUInt32Value(unit_fields::BaseAttackTime, entry.meleeBaseAttackTime);

		setVirtualItem(0, entry.mainHand);
		setVirtualItem(1, entry.offHand);
		setVirtualItem(2, entry.ranged);

		if (entry.offHand) setUInt32Value(unit_fields::VirtualItemSlotDisplay + 1, entry.mainHand->displayId);
		if (entry.ranged) setUInt32Value(unit_fields::VirtualItemSlotDisplay + 2, entry.mainHand->displayId);

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

		// Reset aggro list
		m_threat.clear();

		auto it = m_entry->triggersByEvent.find(trigger_event::OnKilled);
		if (it != m_entry->triggersByEvent.end())
		{
			for (const auto *trigger : it->second)
			{
				trigger->execute(*trigger, this);
			}
		}

		if (killer)
		{
			// Reward the killer with experience points
			const float t =
				(m_entry->maxLevel != m_entry->minLevel) ?
				(getLevel() - m_entry->minLevel) / (m_entry->maxLevel - m_entry->minLevel) :
				0.0f;

			// Base XP for equal level
			UInt32 xp = interpolate(m_entry->xpMin, m_entry->xpMax, t);

			// Level adjustment factor
			const float levelXPMod = calcXpModifier(killer->getLevel());
			xp *= levelXPMod;
			if (xp > 0)
			{
				killer->rewardExperience(this, xp);
			}
		}

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

	void GameCreature::addThreat(GameUnit &threatening, float threat)
	{
		if (!isAlive())
		{
			// We are dead, so we can't have threat
			return;
		}

		auto *world = getWorldInstance();
		if (!world)
		{
			// Not in a world
			return;
		}

		UInt64 guid = threatening.getGuid();

		float &curThreat = m_threat[guid];
		curThreat += threat;

		updateVictim();
	}

	void GameCreature::resetThreat()
	{
		m_threat.clear();

		// We have no more targets
		stopAttack();
	}

	void GameCreature::resetThreat(GameUnit &threatening)
	{
		auto it = m_threat.find(threatening.getGuid());
		if (it != m_threat.end())
		{
			DLOG("Resetting threat");

			// Remove from the threat list
			m_threat.erase(it);

			// Update victim
			updateVictim();
		}
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
		setByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 0, 2, 0);
		setByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 0, 3, item->material);
		setByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 1, 0, item->inventoryType);
		setByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 1, 1, item->sheath);
	}

	void GameCreature::updateVictim()
	{
		if (!isAlive())
			return;

		GameUnit *newVictim = nullptr;
		float maxThreat = -1.0;

		for (auto it : m_threat)
		{
			if (it.second > maxThreat)
			{
				// Try to find unit
				newVictim = dynamic_cast<GameUnit*>(m_worldInstance->findObjectByGUID(it.first));
				if (newVictim)
				{
					maxThreat = it.first;
				}
			}
		}

		if (newVictim &&
			newVictim != getVictim())
		{
			if (!getVictim())
			{
				// Aggro event
				auto it = m_entry->triggersByEvent.find(trigger_event::OnAggro);
				if (it != m_entry->triggersByEvent.end())
				{
					for (const auto *trigger : it->second)
					{
						trigger->execute(*trigger, this);
					}
				}
			}

			// New target to attack
			startAttack(*newVictim);
		}
		else if (!newVictim)
		{
			// No target any more
			stopAttack();
		}
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
