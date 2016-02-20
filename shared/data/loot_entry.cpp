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

#include "loot_entry.h"
#include "item_entry.h"
#include "templates/basic_template_load_context.h"
#include "templates/basic_template_save_context.h"

namespace wowpp
{
	LootEntry::LootEntry()
		: Super()
	{
	}

	bool LootEntry::load(DataLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		if (const sff::read::tree::Array<DataFileIterator> *const groupArray = wrapper.table.getArray("groups"))
		{
			for (size_t j = 0; j < groupArray->getSize(); ++j)
			{
				lootGroups.resize(groupArray->getSize());
				if (const sff::read::tree::Array<DataFileIterator> *const itemArray = groupArray->getArray(j))
				{
					LootGroup &group = lootGroups[j];
					group.resize(itemArray->getSize());

					for (size_t n = 0; n < itemArray->getSize(); ++n)
					{
						const sff::read::tree::Table<DataFileIterator> *const itemTable = itemArray->getTable(n);
						if (!itemTable)
						{
							context.onError("Invalid loot group");
							return false;
						}

						LootDefinition &def = group[n];

						UInt32 itemId = 0;
						itemTable->tryGetInteger("i", itemId);
						context.loadLater.push_back([&context, &def, itemId]() -> bool
						{
							def.item = context.getItem(itemId);
							if (def.item == nullptr)
							{
								std::ostringstream strm;
								strm << "Item not found by id " << itemId;

								context.onError(strm.str());
								return false;
							}

							return true;
						});

						itemTable->tryGetInteger("min", def.minCount);
						itemTable->tryGetInteger("max", def.maxCount);
						itemTable->tryGetInteger("r", def.dropChance);

						UInt32 isActive = 1;
						itemTable->tryGetInteger("active", isActive);
						def.isActive = (isActive != 0);
					}
				}
			}
		}

		return true;
	}

	void LootEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		sff::write::Array<char> groupArray(context.table, "groups", sff::write::MultiLine);
		{
			for (const auto &group : lootGroups)
			{
				// Skip empty loot groups
				if (group.empty()) {
					continue;
				}

				sff::write::Array<char> itemsArray(groupArray, sff::write::MultiLine);
				{
					for (const auto &def : group)
					{
						sff::write::Table<char> itemTable(itemsArray, sff::write::Comma);
						{
							if (def.item != nullptr) {
								itemTable.addKey("i", def.item->id);
							}
							if (def.minCount != 1) {
								itemTable.addKey("min", def.minCount);
							}
							if (def.maxCount != 1) {
								itemTable.addKey("max", def.maxCount);
							}
							itemTable.addKey("r", def.dropChance);
						}
						itemTable.finish();
					}
				}
				itemsArray.finish();
			}
		}
		groupArray.finish();
	}
}
