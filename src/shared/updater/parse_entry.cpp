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
#include "parse_entry.h"
#include "update_list_properties.h"
#include "file_entry_handler.h"
#include "prepare_parameters.h"
#include "virtual_directory/path.h"
#include "common/sha1.h"

namespace wowpp
{
	namespace updating
	{
		namespace
		{
			PreparedUpdate makeIf(
			    const PrepareParameters &parameters,
			    const UpdateListProperties &listProperties,
			    const sff::read::tree::Table<std::string::const_iterator> &entryDescription,
			    const std::string &source,
				const std::string &parseDir,
			    const std::string &destination,
			    IFileEntryHandler &handler)
			{
				const auto condition = entryDescription.getString("condition");
				if (!parameters.conditionsSet.count(condition))
				{
					return PreparedUpdate();
				}

				const auto *const value = entryDescription.getTable("value");
				if (!value)
				{
					throw std::runtime_error("'if' value is missing");
				}

				return parseEntry(
				           parameters,
				           listProperties,
				           *value,
				           source,
							parseDir,
				           destination,
				           handler
				       );
			}
		}


		PreparedUpdate parseEntry(
		    const PrepareParameters &parameters,
		    const UpdateListProperties &listProperties,
		    const sff::read::tree::Table<std::string::const_iterator> &entryDescription,
		    const std::string &source,
			const std::string &parseDir,
		    const std::string &destination,
		    IFileEntryHandler &handler
		)
		{
			const auto type = entryDescription.getString("type");
			if (type == "if")
			{
				return makeIf(
				           parameters,
				           listProperties,
				           entryDescription,
				           source,
							parseDir,
				           destination,
				           handler);
			}

			const auto name = entryDescription.getString("name");
			std::string compressedName;
			const auto subSource =
			    virtual_dir::joinPaths(source,
			        entryDescription.tryGetString("compressedName", compressedName) ? compressedName : name);
			const auto subDestination = virtual_dir::joinPaths(destination, name);
			const auto subParse = virtual_dir::joinPaths(parseDir, name);

			if (const auto *const entries = entryDescription.getArray("entries"))
			{
				return handler.handleDirectory(
				           parameters,
				           listProperties,
				           *entries,
				           type,
				           subSource,
						   subParse,
				           subDestination
				       );
			}
			else
			{
				boost::uintmax_t newSize = 0;
				if (!entryDescription.tryGetInteger(
				            (listProperties.version >= 1 ? "originalSize" : "size"),
				            newSize))
				{
					throw std::runtime_error("Entry file size is missing");
				}

				SHA1Hash newSHA1;
				{
					std::string sha1Hex;
					if (!entryDescription.tryGetString("sha1", sha1Hex))
					{
						throw std::runtime_error("SHA-1 digest is missing");
					}
					std::istringstream sha1HexStream(sha1Hex);
					newSHA1 = sha1ParseHex(sha1HexStream);
					if (sha1HexStream.bad())
					{
						throw std::runtime_error("Invalid SHA-1 digest");
					}
				}

				boost::uintmax_t compressedSize = newSize;
				std::string compression;
				if (listProperties.version >= 1)
				{
					compression = entryDescription.getString("compression");
					if (!compression.empty() &&
					    !entryDescription.tryGetInteger("compressedSize", compressedSize))
					{
						throw std::runtime_error(
						    "Compressed file size is missing");
					}
				}

				return handler.handleFile(
				           parameters,
				           entryDescription,
				           subSource,
					       subParse,
				           subDestination,
				           newSize,
				           newSHA1,
				           compression,
				           compressedSize
				       );
			}
		}
	}
}
