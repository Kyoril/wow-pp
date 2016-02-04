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

#include "faction_entry.h"
#include "templates/basic_template_load_context.h"
#include "templates/basic_template_save_context.h"

namespace wowpp
{
	FactionEntry::FactionEntry()
		: Super()
		, repListId(-1)
	{
	}

	bool FactionEntry::load(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		wrapper.table.tryGetInteger("rep_list_id", repListId);
		wrapper.table.tryGetString("name", name);
		
		// Read base reputation values
		if (const sff::read::tree::Array<DataFileIterator> *const baseRepArray = wrapper.table.getArray("base_rep"))
		{
			baseReputation.resize(baseRepArray->getSize());
			for (size_t j = 0; j < baseRepArray->getSize(); ++j)
			{
				const sff::read::tree::Table<DataFileIterator> *const baseRepTable = baseRepArray->getTable(j);
				if (!baseRepTable)
				{
					context.onError("Invalid base reputation entry");
					return false;
				}

				BaseReputation &rep = baseReputation[j];
				baseRepTable->tryGetInteger("race", rep.raceMask);
				baseRepTable->tryGetInteger("class", rep.classMask);
				baseRepTable->tryGetInteger("value", rep.value);
				baseRepTable->tryGetInteger("flags", rep.flags);
			}
		}

		return true;
	}

	void FactionEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (repListId != -1) context.table.addKey("rep_list_id", repListId);
		if (!name.empty()) context.table.addKey("name", name);

		// Write base reputation values
		sff::write::Array<char> baseRepArray(context.table, "base_rep", sff::write::Comma);
		{
			for (const auto &rep : baseReputation)
			{
				sff::write::Table<char> baseRepTable(baseRepArray, sff::write::Comma);
				{
					baseRepTable.addKey("race", rep.raceMask);
					baseRepTable.addKey("class", rep.classMask);
					baseRepTable.addKey("value", rep.value);
					baseRepTable.addKey("flags", rep.flags);
				}
				baseRepTable.finish();
			}
		}
		baseRepArray.finish();
	}
}
