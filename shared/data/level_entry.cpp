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

#include "level_entry.h"
#include "templates/basic_template_load_context.h"
#include "templates/basic_template_save_context.h"

namespace wowpp
{
	LevelEntry::LevelEntry()
	{
	}

	bool LevelEntry::load(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		// Load xp to next level
		nextLevelXP = wrapper.table.getInteger("next_level_xp", 0);

		// Load class/race stats
		if (const sff::read::tree::Array<DataFileIterator> *const statArray = wrapper.table.getArray("stats"))
		{
			for (size_t j = 0, d = statArray->getSize(); j < d; ++j)
			{
				const sff::read::tree::Table<DataFileIterator> *const statTable = statArray->getTable(j);
				if (!statTable)
				{
					context.onError("Invalid level");
					return false;
				}

				// Get race / class
				UInt32 raceId, classId;
				if (!statTable->tryGetInteger("race", raceId) ||
					!statTable->tryGetInteger("class", classId))
				{
					context.onError("Missing race/class definition for level stats");
					return false;
				}

				// Create new stat array
				StatArray arr;
				arr.fill(0);

				for (size_t i = 0; i < arr.size(); ++i)
				{
					// Build stat name
					std::stringstream strm;
					strm << "stat_" << i;

					// Load stat value from table (or set to 0 if invalid)
					arr[i] = statTable->getInteger(strm.str(), 0);
				}

				// Set value
				stats[raceId][classId] = arr;
			}
		}

		return true;
	}

	void LevelEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		// Write xp to next level
		context.table.addKey("next_level_xp", nextLevelXP);

		// Write stats
		sff::write::Array<char> statArray(context.table, "stats", sff::write::MultiLine);
		{
			for (const auto &entry : stats)
			{
				UInt32 raceId = entry.first;

				for (const auto &entry2 : entry.second)
				{
					UInt32 classId = entry2.first;

					// New stat table
					sff::write::Table<char> statTable(statArray, sff::write::Comma);
					{
						statTable.addKey("race", raceId);
						statTable.addKey("class", classId);

						const auto &arr = entry2.second;
						for (size_t i = 0; i < arr.size(); ++i)
						{
							std::stringstream strm;
							strm << "stat_" << i;

							statTable.addKey(strm.str(), arr[i]);
						}
					}
					statTable.finish();
				}
			}
		}
		statArray.finish();
	}
}
