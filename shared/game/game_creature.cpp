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
#include "proto_data/project.h"
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
		proto::Project &project,
		TimerQueue &timers, 
		const proto::UnitEntry &entry)
		: GameUnit(project, timers)
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

	void GameCreature::setEntry(const proto::UnitEntry &entry)
	{
		const bool isInitialize = (m_entry == nullptr);

		// Choose a level
		UInt8 creatureLevel = entry.minlevel();
		if (entry.maxlevel() != entry.minlevel())
		{
			std::uniform_int_distribution<int> levelDistribution(entry.minlevel(), entry.maxlevel());
			creatureLevel = levelDistribution(randomGenerator);
		}

		// calculate interpolation factor
		const float t =
			(entry.maxlevel() != entry.minlevel()) ?
			(creatureLevel - entry.minlevel()) / (entry.maxlevel() - entry.minlevel()) :
			0.0f;

		// Randomize gender
		std::uniform_int_distribution<int> genderDistribution(0, 1);
		int gender = genderDistribution(randomGenerator);

		// TODO
		const auto *faction = getProject().factionTemplates.getById(entry.alliancefaction());
		assert(faction);
		setFactionTemplate(*faction);

		setLevel(creatureLevel);
		setClass(entry.unitclass());
		setUInt32Value(object_fields::Entry, entry.id());
		setFloatValue(object_fields::ScaleX, entry.scale());
		setUInt32Value(unit_fields::FactionTemplate, faction->id());
		setGender(static_cast<game::Gender>(gender));
		setUInt32Value(unit_fields::DisplayId, (gender == game::gender::Male ? entry.malemodel() : entry.femalemodel()));
		setUInt32Value(unit_fields::NativeDisplayId, (gender == game::gender::Male ? entry.malemodel() : entry.femalemodel()));
		setUInt32Value(unit_fields::BaseHealth, interpolate(entry.minlevelhealth(), entry.maxlevelhealth(), t));
		setUInt32Value(unit_fields::MaxHealth, getUInt32Value(unit_fields::BaseHealth));
		setUInt32Value(unit_fields::Health, getUInt32Value(unit_fields::BaseHealth));
		setUInt32Value(unit_fields::MaxPower1, interpolate(entry.minlevelmana(), entry.maxlevelmana(), t));
		setUInt32Value(unit_fields::Power1, getUInt32Value(unit_fields::MaxPower1));
		setUInt32Value(unit_fields::UnitFlags, entry.unitflags());
		setUInt32Value(unit_fields::DynamicFlags, entry.dynamicflags());
		setUInt32Value(unit_fields::NpcFlags, entry.npcflags());
		setByteValue(unit_fields::Bytes2, 1, 16);
		setByteValue(unit_fields::Bytes2, 0, 1);		// Sheath State: Melee weapon
		setUInt32Value(unit_fields::AttackPower, entry.attackpower());
		setUInt32Value(unit_fields::RangedAttackPower, entry.rangedattackpower());
		setFloatValue(unit_fields::MinDamage, entry.minmeleedmg());
		setFloatValue(unit_fields::MaxDamage, entry.maxmeleedmg());
		setUInt32Value(unit_fields::BaseAttackTime, entry.meleeattacktime());

		// Update power type
		setByteValue(unit_fields::Bytes0, 3, game::power_type::Mana);
		setUInt32Value(unit_fields::BaseMana, getUInt32Value(unit_fields::MaxPower1));
		setUInt32Value(unit_fields::Bytes0, 0x00020200);
// 		if (entry.maxLevelMana > 0)
// 		{
// 			setByteValue(unit_fields::Bytes1, 1, 0xEE);
// 		}
// 		else
// 		{
// 			setByteValue(unit_fields::Bytes1, 1, 0x00);
// 		}

		setVirtualItem(0, getProject().items.getById(entry.mainhandweapon()));
		setVirtualItem(1, getProject().items.getById(entry.offhandweapon()));
		setVirtualItem(2, getProject().items.getById(entry.rangedweapon()));

		// Unit Mods
		for (UInt32 i = unit_mods::StatStart; i < unit_mods::StatEnd; ++i)
		{
			setModifierValue(static_cast<UnitMods>(i), unit_mod_type::BaseValue, 0.0f);
			setModifierValue(static_cast<UnitMods>(i), unit_mod_type::TotalValue, 0.0f);
			setModifierValue(static_cast<UnitMods>(i), unit_mod_type::BasePct, 1.0f);
			setModifierValue(static_cast<UnitMods>(i), unit_mod_type::TotalPct, 1.0f);
		}
		setModifierValue(unit_mods::Armor, unit_mod_type::BaseValue, entry.armor());

		// Setup new entry
		m_entry = &entry;

		// Emit signal
		if (!isInitialize)
		{
			entryChanged();
		}

		updateAllStats();
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
			return m_originalEntry.name();
		}
		
		return m_entry->name();
	}

	void GameCreature::setVirtualItem(UInt32 slot, const proto::ItemEntry *item)
	{
		if (!item)
		{
			setUInt32Value(unit_fields::VirtualItemSlotDisplay + slot, 0);
			setUInt32Value(unit_fields::VirtualItemInfo + (slot * 2) + 0, 0);
			setUInt32Value(unit_fields::VirtualItemInfo + (slot * 2) + 1, 0);
			return;
		}

		setUInt32Value(unit_fields::VirtualItemSlotDisplay + slot, item->displayid());
		setByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 0, 0, item->itemclass());
		setByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 0, 1, item->subclass());
		setByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 0, 2, 0xFF);
		setByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 0, 3, item->material());
		setByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 1, 0, item->inventorytype());
		setByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 1, 1, item->sheath());
	}

	bool GameCreature::canBlock() const
	{
		const UInt32 slot = 1;
		const auto invType = getByteValue(unit_fields::VirtualItemInfo + (slot * 2) + 1, 0);

		// Creatures can only block if they wield a shield
		return (invType == game::inventory_type::Shield);
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

	bool GameCreature::canDualWield() const
	{
		return true;
	}

	void GameCreature::updateDamage()
	{
		const float att_speed = static_cast<float>(getUInt32Value(unit_fields::BaseAttackTime)) / 1000.0f;
		const float base_value = static_cast<float>(getUInt32Value(unit_fields::AttackPower)) / 14.0f * att_speed;

		const auto *entry = (m_entry ? m_entry : &m_originalEntry);
		setFloatValue(unit_fields::MinDamage, base_value + entry->minmeleedmg());
		setFloatValue(unit_fields::MaxDamage, base_value + entry->maxmeleedmg());
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

	void GameCreature::setUnitLoot(std::unique_ptr<LootInstance> unitLoot)
	{
		m_unitLoot = std::move(unitLoot);
	}

	void GameCreature::raiseTrigger(trigger_event::Type e)
	{
		for (const auto &triggerId : getEntry().triggers())
		{
			const auto *triggerEntry = getProject().triggers.getById(triggerId);
			if (triggerEntry)
			{
				for (const auto &triggerEvent : triggerEntry->events())
				{
					if (triggerEvent == e)
					{
						unitTrigger(std::cref(*triggerEntry), std::ref(*this));
						return;
					}
				}
			}
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
