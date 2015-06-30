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

#include "spell_entry.h"
#include "templates/basic_template_load_context.h"
#include "templates/basic_template_save_context.h"
#include "game/defines.h"

namespace wowpp
{
	SpellEntry::SpellEntry()
	{
	}

	bool SpellEntry::load(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		wrapper.table.tryGetString("name", name);

		const sff::read::tree::Array<DataFileIterator> *effectsArray = wrapper.table.getArray("effects");
		if (effectsArray)
		{
			effects.resize(effectsArray->getSize(), game::spell_effects::Invalid_);
			for (size_t j = 0, d = effectsArray->getSize(); j < d; ++j)
			{
				const sff::read::tree::Table<DataFileIterator> *const effectTable = effectsArray->getTable(j);
				if (!effectTable)
				{
					context.onError("Invalid spell effect table");
					return false;
				}

				// Get effect type
				UInt32 effectIndex = 0;
				if (!effectTable->tryGetInteger("type", effectIndex))
				{
					context.onError("Invalid spell effect type");
					return false;
				}

				// Validate effect
				if (effectIndex == game::spell_effects::Invalid_ ||
					effectIndex > game::spell_effects::Count_)
				{
					context.onError("Invalid spell effect type");
					return false;
				}

				effects[j] = static_cast<game::SpellEffect>(effectIndex);
			}
		}

		return true;
	}

	void SpellEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (!name.empty()) context.table.addKey("name", name);

		// Write spell effects
		if (!effects.empty())
		{
			sff::write::Array<char> effectsArray(context.table, "effects", sff::write::MultiLine);
			{
				for (auto &effect : effects)
				{
					sff::write::Table<char> effectTable(effectsArray, sff::write::Comma);
					{
						effectTable.addKey("type", static_cast<UInt32>(effect));
					}
					effectTable.finish();
				}
			}
			effectsArray.finish();
		}
	}
}
