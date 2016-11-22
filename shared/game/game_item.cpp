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
#include "game_item.h"
#include "proto_data/project.h"
#include "common/make_unique.h"

namespace wowpp
{
	GameItem::GameItem(proto::Project &project, const proto::ItemEntry &entry)
		: GameObject(project)
		, m_entry(entry)
	{
		// Resize values field
		m_values.resize(item_fields::ItemFieldCount, 0);
		m_valueBitset.resize((item_fields::ItemFieldCount + 31) / 32, 0);

		// 2.3.2 - 0x18
		m_objectType |= type_mask::Item;
	}

	GameItem::~GameItem()
	{
	}

	void GameItem::initialize()
	{
		GameObject::initialize();

		setUInt32Value(object_fields::Entry, m_entry.id());
		setFloatValue(object_fields::ScaleX, 1.0f);

		setUInt32Value(item_fields::MaxDurability, m_entry.durability());
		setUInt32Value(item_fields::Durability, m_entry.durability());
		setUInt32Value(item_fields::StackCount, 1);
		setUInt32Value(item_fields::Flags, m_entry.flags());

		// Spell charges
		for (size_t i = 0; i < 5; ++i)
		{
			if (i >= m_entry.spells_size()) {
				break;
			}

			setUInt32Value(item_fields::SpellCharges + i, m_entry.spells(i).charges());
		}

		// Generate loot
		generateLoot();
	}

	UInt16 GameItem::addStacks(UInt16 amount)
	{
		const UInt32 stackCount = getStackCount();

		const UInt32 availableStacks = m_entry.maxstack() - stackCount;
		if (amount <= availableStacks)
		{
			setUInt32Value(item_fields::StackCount, stackCount + amount);
			return amount;
		}

		// Max out the stack count and return the added stacks
		setUInt32Value(item_fields::StackCount, m_entry.maxstack());
		return static_cast<UInt16>(availableStacks);
	}

	void GameItem::notifyEquipped()
	{
		// Emit signal
		equipped();
	}

	bool GameItem::isCompatibleWithSpell(const proto::SpellEntry & spell)
	{
		if (spell.itemclass() != -1)
		{
			if (spell.itemclass() != static_cast<Int32>(m_entry.itemclass()))
			{
				return false;
			}

			if (spell.itemsubclassmask() != 0 && 
				(spell.itemsubclassmask() & (1 << m_entry.subclass())) == 0)
			{
				return false;
			}
		}

		// TODO: Check enchantments;

		return true;
	}

	void GameItem::generateLoot()
	{
		auto lootEntryId = m_entry.lootentry();
		const auto *lootEntry = lootEntryId ? getProject().itemLoot.getById(lootEntryId) : nullptr;
		if (lootEntry || m_entry.minlootgold() > 0)
		{
			// TODO: make a way so we don't need loot recipients for game objects as this is completely crap
			std::vector<GameCharacter *> lootRecipients;
			m_loot = make_unique<LootInstance>(
				getProject().items, getGuid(), lootEntry, m_entry.minlootgold(), m_entry.maxlootgold(), std::cref(lootRecipients));
			if (m_loot)
			{
				m_onLootCleared = m_loot->cleared.connect([this]()
				{
					// We use the despawned signal here, even though the item is not physically
					// spawned in the world. We do so, in order for loot handling to work just fine
					// with this kind of loot source.
					despawned(*this);
				});
			}
		}
	}

	io::Writer &operator<<(io::Writer &w, GameItem const &object)
	{
		return w
		       << reinterpret_cast<GameObject const &>(object);
	}

	io::Reader &operator>>(io::Reader &r, GameItem &object)
	{
		return r
		       >> reinterpret_cast<GameObject &>(object);
	}

}
