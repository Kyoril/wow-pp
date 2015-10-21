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

#include "vendor_entry.h"
#include "item_entry.h"
#include "templates/basic_template_load_context.h"
#include "templates/basic_template_save_context.h"

namespace wowpp
{
	VendorEntry::VendorEntry()
		: Super()
	{
	}

	bool VendorEntry::load(DataLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		if (const sff::read::tree::Array<DataFileIterator> *const itemArray = wrapper.table.getArray("items"))
		{
			for (size_t j = 0; j < itemArray->getSize(); ++j)
			{
				items.resize(itemArray->getSize());

				const sff::read::tree::Table<DataFileIterator> *const itemTable = itemArray->getTable(j);
				if (!itemTable)
				{
					context.onError("Invalid loot group.");
					return false;
				}

				VendorItemEntry &entry = items[j];
				
				UInt32 itemId = 0;
				itemTable->tryGetInteger("entry", itemId);
				entry.item = context.getItem(itemId);
				if (!entry.item)
				{
					context.onError("Could not find item entry referenced in vendor template.");
					return false;
				}

				itemTable->tryGetInteger("max_count", entry.maxCount);
				itemTable->tryGetInteger("interval", entry.interval);
				itemTable->tryGetInteger("extended_cost", entry.extendedCost);

				UInt32 isActive = 1;
				itemTable->tryGetInteger("active", isActive);
				entry.isActive = (isActive != 0);
			}
		}

		return true;
	}

	void VendorEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		sff::write::Array<char> itemArray(context.table, "items", sff::write::MultiLine);
		{
			for (const auto &entry : items)
			{
				sff::write::Table<char> itemTable(itemArray, sff::write::Comma);
				{
					if (entry.item != nullptr) itemTable.addKey("entry", entry.item->id);
					if (entry.maxCount != 0) itemTable.addKey("max_count", entry.maxCount);
					if (entry.interval != 0) itemTable.addKey("interval", entry.interval);
					if (entry.extendedCost != 0) itemTable.addKey("extended_cost", entry.extendedCost);
					if (!entry.isActive) itemTable.addKey("active", 0);
				}
				itemTable.finish();
			}
		}
		itemArray.finish();
	}
}
