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
#include "parse_directory_entries.h"
#include "parse_entry.h"
#include "common/macros.h"

namespace wowpp
{
	namespace updating
	{
		PreparedUpdate parseDirectoryEntries(
		    const PrepareParameters &parameters,
		    const UpdateListProperties &listProperties,
		    const std::string &source,
			const std::string &parseDir,
		    const std::string &destination,
		    const sff::read::tree::Array<std::string::const_iterator> &entries,
		    IFileEntryHandler &handler
		)
		{
			std::vector<PreparedUpdate> updates;

			for (size_t i = 0, c = entries.getSize(); i < c; ++i)
			{
				const auto *const entry = entries.getTable(i);
				if (!entry)
				{
					throw std::runtime_error("Non-table element in entries array");
				}

				updates.push_back(parseEntry(
				                      parameters,
				                      listProperties,
				                      *entry,
				                      source,
									  parseDir,
				                      destination,
				                      handler
				                  ));
			}

			return accumulate(std::move(updates));
		}
	}
}
