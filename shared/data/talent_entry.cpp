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

#include "talent_entry.h"
#include "templates/basic_template_load_context.h"
#include "templates/basic_template_save_context.h"
#include "data_load_context.h"
#include "spell_entry.h"
#include "common/make_unique.h"

namespace wowpp
{
	TalentEntry::TalentEntry()
		: tab(0)
		, row(0)
		, column(0)
		, dependsOn(nullptr)
		, dependsOnRank(0)
		, dependsOnSpell(nullptr)
	{
	}

	bool TalentEntry::load(DataLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		wrapper.table.tryGetInteger("tab", tab);
		wrapper.table.tryGetInteger("row", row);
		wrapper.table.tryGetInteger("column", column);

		UInt32 entry = 0;
		wrapper.table.tryGetInteger("depends_on_talent", entry);
		if (entry != 0)
		{
			context.loadLater.push_back([entry, this, &context]() -> bool
			{
				dependsOn = context.getTalent(entry);
				return true;
			});
		}

		wrapper.table.tryGetInteger("depends_on_rank", dependsOnRank);

		entry = 0;
		wrapper.table.tryGetInteger("depends_on_spell", entry);
		if (entry != 0)
		{
			dependsOnSpell = context.getSpell(entry);
			if (dependsOnSpell == nullptr)
			{
				context.onWarning("Could not find dependent spell referenced in talent entry!");
			}
		}

		const sff::read::tree::Array<DataFileIterator> *rankArray = wrapper.table.getArray("ranks");
		if (rankArray)
		{
			for (size_t j = 0, d = rankArray->getSize(); j < d; ++j)
			{
				UInt32 spellId = rankArray->getInteger(j, 0);
				if (spellId == 0)
				{
					context.onWarning("Invalid spell id in talent entry - spell rank will be ignored");
					continue;
				}

				const auto *spell = context.getSpell(spellId);
				if (!spell)
				{
					std::ostringstream strm;
					strm << "Could not find spell " << spellId << " referenced in talent entry " << id << "!";
					context.onError(strm.str());
					continue;
				}

				auto *editableSpell = context.getEditableSpell(spellId);
				if (!editableSpell)
				{
					continue;
				}

				editableSpell->talentCost = ranks.size() + 1;
				ranks.push_back(spell);
			}
		}

		return true;
	}

	void TalentEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (tab != 0) {
			context.table.addKey("tab", tab);
		}
		if (row != 0) {
			context.table.addKey("row", row);
		}
		if (column != 0) {
			context.table.addKey("column", column);
		}
		if (dependsOn != nullptr) {
			context.table.addKey("depends_on_talent", dependsOn->id);
		}
		if (dependsOnRank != 0) {
			context.table.addKey("depends_on_rank", dependsOnRank);
		}
		if (dependsOnSpell != nullptr) {
			context.table.addKey("depends_on_spell", dependsOnSpell->id);
		}

		// Write spell effects
		if (!ranks.empty())
		{
			auto ranksArray = make_unique<sff::write::Array<char>>(context.table, "ranks", sff::write::Comma);
			{
				for (auto &rank : ranks)
				{
					ranksArray->addElement(rank->id);
				}
			}
			ranksArray->finish();
		}
	}
}
