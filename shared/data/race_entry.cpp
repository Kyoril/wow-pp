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

#include "race_entry.h"
#include "templates/basic_template_load_context.h"
#include "templates/basic_template_save_context.h"
#include "item_entry.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	RaceEntry::RaceEntry()
	{
	}

	bool RaceEntry::load(DataLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		wrapper.table.tryGetString("name", name);
		wrapper.table.tryGetInteger("faction", factionID);
		wrapper.table.tryGetInteger("maleModel", maleModel);
		wrapper.table.tryGetInteger("femaleModel", femaleModel);
		wrapper.table.tryGetInteger("language", baseLanguage);
		wrapper.table.tryGetInteger("startingTaxiMask", startingTaxiMask);
		wrapper.table.tryGetInteger("cinematic", cinematic);
		wrapper.table.tryGetInteger("map", startMap);
		wrapper.table.tryGetInteger("zone", startZone);
		wrapper.table.tryGetInteger("rotation", startRotation);

		const sff::read::tree::Array<DataFileIterator> *const positionArray = wrapper.table.getArray("position");
		if (!positionArray ||
			!loadPosition(startPosition, *positionArray))
		{
			context.onError("Invalid position in a race entry");
			return false;
		}

		const sff::read::tree::Array<DataFileIterator> *initialSpellArray = wrapper.table.getArray("initial_spells");
		if (initialSpellArray)
		{
			for (size_t j = 0, d = initialSpellArray->getSize(); j < d; ++j)
			{
				const sff::read::tree::Table<DataFileIterator> *const classTable = initialSpellArray->getTable(j);
				if (!classTable)
				{
					context.onError("Invalid initial spells table");
					return false;
				}

				// Get class id
				UInt32 classId;
				if (!classTable->tryGetInteger("class", classId))
				{
					context.onError("Invalid class entry");
					return false;
				}

				// Entry already exists?
				if (initialSpells.find(classId) != initialSpells.end())
				{
					context.onError("Duplicate entry: class already has initial spells for that race");
					return false;
				}

				// Setup spellIds
				InitialSpellIds &spellIds = initialSpells[classId];
				const sff::read::tree::Array<DataFileIterator> *spellArray = classTable->getArray("spells");
				if (spellArray)
				{
					spellIds.resize(spellArray->getSize());
					for (size_t x = 0, y = spellArray->getSize(); x < y; ++x)
					{
						spellIds[x] = spellArray->getInteger(x, 0xffff);
					}
				}
			}
		}

		const sff::read::tree::Array<DataFileIterator> *initialItemsArray = wrapper.table.getArray("initial_items");
		if (initialItemsArray)
		{
			for (size_t j = 0, d = initialItemsArray->getSize(); j < d; ++j)
			{
				const sff::read::tree::Table<DataFileIterator> *const entryTable = initialItemsArray->getTable(j);
				if (!entryTable)
				{
					context.onError("Invalid initial items table");
					return false;
				}

				// Get class id
				UInt32 classId;
				if (!entryTable->tryGetInteger("class", classId))
				{
					context.onError("Invalid class entry");
					return false;
				}

				UInt32 genderId;
				if (!entryTable->tryGetInteger("gender", genderId))
				{
					context.onError("Invalid gender id");
					return false;
				}

				auto it = initialItems.find(classId);
				if (it != initialItems.end())
				{
					auto it2 = it->second.find(genderId);
					if (it2 != it->second.end())
					{
						context.onError("Duplicate entry: There already is an initial item list for that race/class/gender combination.");
						return false;
					}
				}

				const sff::read::tree::Array<DataFileIterator> *itemArray = entryTable->getArray("items");
				if (!itemArray)
				{
					context.onError("Missing items array!");
					return false;
				}

				// Load items later
				for (size_t x = 0; x < itemArray->getSize(); ++x)
				{
					UInt32 itemId = itemArray->getInteger(x, 0);
					if (itemId > 0)
					{
						context.loadLater.push_back(
							[classId, genderId, itemId, this, &context]() -> bool
						{
							const ItemEntry * item = context.getItem(itemId);
							if (item)
							{
								this->initialItems[classId][genderId].push_back(item);
								return true;
							}
							else
							{
								ELOG("Unknown item " << itemId << ": Can't be set as initial item");
								return false;
							}
						});
					}
				}
			}
		}

		const sff::read::tree::Array<DataFileIterator> *initialActionsArray = wrapper.table.getArray("initial_actions");
		if (initialActionsArray)
		{
			for (size_t j = 0, d = initialActionsArray->getSize(); j < d; ++j)
			{
				const sff::read::tree::Table<DataFileIterator> *const classTable = initialActionsArray->getTable(j);
				if (!classTable)
				{
					context.onError("Invalid initial actions table");
					return false;
				}

				// Get class id
				UInt32 classId;
				if (!classTable->tryGetInteger("class", classId))
				{
					context.onError("Invalid class entry");
					return false;
				}

				// Entry already exists?
				if (initialActionButtons.find(classId) != initialActionButtons.end())
				{
					context.onError("Duplicate entry: class already has initial actions for that race");
					return false;
				}

				const sff::read::tree::Array<DataFileIterator> *buttonsArray = classTable->getArray("buttons");
				if (!buttonsArray)
				{
					context.onError("Missing buttons array!");
					return false;
				}

				// Setup buttons
				ActionButtons &buttons = initialActionButtons[classId];

				// Iterate through button tables
				for (size_t n = 0, d2 = buttonsArray->getSize(); n < d2; ++n)
				{
					const sff::read::tree::Table<DataFileIterator> *const buttonTable = buttonsArray->getTable(n);
					if (!buttonTable)
					{
						context.onError("Invalid buttons table");
						return false;
					}

					UInt32 buttonId = 0;
					if (!buttonTable->tryGetInteger("button", buttonId))
					{
						context.onError("Missing button index");
						return false;
					}

					ActionButton &button = buttons[buttonId];
					buttonTable->tryGetInteger("action", button.action);
					buttonTable->tryGetInteger("type", button.type);
				}
			}
		}

		return true;
	}

	void RaceEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (!name.empty()) context.table.addKey("name", name);
		context.table.addKey("faction", factionID);
		context.table.addKey("maleModel", maleModel);
		context.table.addKey("femaleModel", femaleModel);
		context.table.addKey("language", baseLanguage);
		context.table.addKey("startingTaxiMask", startingTaxiMask);
		context.table.addKey("cinematic", cinematic);
		context.table.addKey("map", startMap);
		context.table.addKey("zone", startZone);
		{
			sff::write::Array<char> positionArray(context.table, "position", sff::write::Comma);
			positionArray.addElement(startPosition[0]);
			positionArray.addElement(startPosition[1]);
			positionArray.addElement(startPosition[2]);
			positionArray.finish();
		}
		context.table.addKey("rotation", startRotation);

		// Write stats
		sff::write::Array<char> initialSpellArray(context.table, "initial_spells", sff::write::MultiLine);
		{
			for (const auto &entry : initialSpells)
			{
				UInt32 classId = entry.first;

				// New stat table
				sff::write::Table<char> classTable(initialSpellArray, sff::write::Comma);
				{
					classTable.addKey("class", classId);

					sff::write::Array<char> spellArray(classTable, "spells", sff::write::Comma);
					{
						for (const auto &spell : entry.second)
						{
							spellArray.addElement(spell);
						}
					}
					spellArray.finish();
				}
				classTable.finish();
			}
		}
		initialSpellArray.finish();

		// Write action buttons
		sff::write::Array<char> initialActionsArray(context.table, "initial_actions", sff::write::MultiLine);
		{
			for (const auto &entry : initialActionButtons)
			{
				UInt32 classId = entry.first;

				// New stat table
				sff::write::Table<char> classTable(initialSpellArray, sff::write::Comma);
				{
					classTable.addKey("class", classId);

					sff::write::Array<char> buttonsArray(classTable, "buttons", sff::write::MultiLine);
					{
						for (const auto &button : entry.second)
						{
							sff::write::Table<char> buttonTable(buttonsArray, sff::write::Comma);
							{
								buttonTable.addKey("button", static_cast<UInt32>(button.first));
								buttonTable.addKey("action", button.second.action);
								buttonTable.addKey("type", static_cast<UInt32>(button.second.type));
							}
							buttonTable.finish();
						}
					}
					buttonsArray.finish();
				}
				classTable.finish();
			}
		}
		initialActionsArray.finish();
	}
}
