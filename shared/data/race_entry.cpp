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

namespace wowpp
{
	RaceEntry::RaceEntry()
	{
	}

	bool RaceEntry::load(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper)
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
	}
}
