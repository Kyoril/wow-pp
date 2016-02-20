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

#include "trainer_entry.h"
#include "spell_entry.h"
#include "skill_entry.h"
#include "templates/basic_template_load_context.h"
#include "templates/basic_template_save_context.h"
#include <memory>

namespace wowpp
{
	TrainerEntry::TrainerEntry()
		: Super()
		, classId(0)
		, trainerType(trainer_types::ClassTrainer)
	{
	}

	bool TrainerEntry::load(DataLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		wrapper.table.tryGetInteger("class", classId);
		wrapper.table.tryGetInteger("type", trainerType);
		wrapper.table.tryGetString("title", title);

		if (const sff::read::tree::Array<DataFileIterator> *const spellsArray = wrapper.table.getArray("spells"))
		{
			for (size_t j = 0; j < spellsArray->getSize(); ++j)
			{
				spells.resize(spellsArray->getSize());

				const sff::read::tree::Table<DataFileIterator> *const spellTable = spellsArray->getTable(j);
				if (!spellTable)
				{
					context.onError("Invalid spell trainer entry.");
					return false;
				}

				TrainerSpellEntry &entry = spells[j];

				UInt32 spellId = 0;
				spellTable->tryGetInteger("entry", spellId);
				spellTable->tryGetInteger("cost", entry.spellCost);

				UInt32 skillId = 0;
				spellTable->tryGetInteger("req_skill", skillId);
				spellTable->tryGetInteger("req_skill_value", entry.reqSkillValue);
				spellTable->tryGetInteger("req_level", entry.reqLevel);

				entry.spell = context.getSpell(spellId);
				if (!entry.spell)
				{
					context.onError("Could not find trainer spell entry.");
					return false;
				}

				if (skillId != 0)
				{
					entry.reqSkill = context.getSkill(skillId);
					if (!entry.reqSkill)
					{
						context.onError("Could not find trainer skill entry.");
						return false;
					}
				}
			}
		}

		return true;
	}

	void TrainerEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (classId != 0) {
			context.table.addKey("class", classId);
		}
		if (trainerType != 0) {
			context.table.addKey("type", trainerType);
		}
		if (!title.empty()) {
			context.table.addKey("title", title);
		}

		sff::write::Array<char> spellArray(context.table, "spells", sff::write::MultiLine);
		{
			for (const auto &entry : spells)
			{
				sff::write::Table<char> spellTable(spellArray, sff::write::Comma);
				{
					if (entry.spell != nullptr) {
						spellTable.addKey("entry", entry.spell->id);
					}
					if (entry.spellCost != 0) {
						spellTable.addKey("cost", entry.spellCost);
					}
					if (entry.reqSkill != nullptr) {
						spellTable.addKey("req_skill", entry.reqSkill->id);
					}
					if (entry.reqSkillValue != 0) {
						spellTable.addKey("req_skill_value", entry.reqSkillValue);
					}
					if (entry.reqLevel) {
						spellTable.addKey("req_level", entry.reqLevel);
					}
				}
				spellTable.finish();
			}
		}
		spellArray.finish();
	}
}
