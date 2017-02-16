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
#include "prepare_update.h"
#include "prepare_parameters.h"
#include "update_source.h"
#include "update_list_properties.h"
#include "file_system_entry_handler.h"
#include "parse_entry.h"
#include "simple_file_format/sff_load_file.h"

namespace wowpp
{
	namespace updating
	{
		PreparedUpdate prepareUpdate(
			const std::string &parseDir,
		    const std::string &outputDir,
		    const PrepareParameters &parameters
		)
		{
			const auto listFile = parameters.source->readFile("list.txt");

			std::string sourceContent;
			sff::read::tree::Table<std::string::const_iterator> sourceTable;
			sff::loadTableFromFile(sourceTable, sourceContent, *listFile.content);

			UpdateListProperties listProperties;
			listProperties.version = sourceTable.getInteger<unsigned>("version", 0);
			if (listProperties.version <= 1)
			{
				const auto *const root = sourceTable.getTable("root");
				if (!root)
				{
					throw std::runtime_error("Root directory entry is missing");
				}

				FileSystemEntryHandler fileSystem;
				return parseEntry(
				           parameters,
				           listProperties,
				           *root,
				           "",
						   parseDir,
				           outputDir,
				           fileSystem
				       );
			}
			else
			{
				throw std::runtime_error(
				    "Unsupported update list version: " +
				    boost::lexical_cast<std::string>(listProperties.version));
			}
		}
	}
}
