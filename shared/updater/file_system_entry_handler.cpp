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
#include "file_system_entry_handler.h"
#include "prepare_parameters.h"
#include "prepare_progress_handler.h"
#include "update_source.h"
#include "copy_with_progress.h"
#include "parse_directory_entries.h"
#include "common/macros.h"

namespace wowpp
{
	namespace updating
	{
		PreparedUpdate FileSystemEntryHandler::handleDirectory(
		    const PrepareParameters &parameters,
		    const UpdateListProperties &listProperties,
		    const sff::read::tree::Array<std::string::const_iterator> &entries,
		    const std::string &type,
		    const std::string &source,
			const std::string &parseDir,
		    const std::string &destination)
		{
			if (type == "fs")
			{
				boost::filesystem::create_directories(destination);
				return parseDirectoryEntries(
				           parameters,
				           listProperties,
				           source,
						   parseDir,
				           destination,
				           entries,
				           *this
				       );
			}
			else
			{
				throw std::runtime_error(
				    "Unknown file system entry type: " + type);
			}
		}

		PreparedUpdate FileSystemEntryHandler::handleFile(
		    const PrepareParameters &parameters,
		    const sff::read::tree::Table<std::string::const_iterator> &/*entryDescription*/,
		    const std::string &source,
			const std::string &parseDir,
		    const std::string &destination,
		    boost::uintmax_t originalSize,
		    const SHA1Hash &sha1,
		    const std::string &compression,
		    boost::uintmax_t compressedSize
		)
		{
			for (;;)
			{
				{
					parameters.progressHandler.beginCheckLocalCopy(source);

					//TODO: prevent race condition
					std::ifstream previousFile(
					    parseDir,
					    std::ios::binary | std::ios::ate);

					if (previousFile)
					{
						const boost::uintmax_t previousSize = previousFile.tellg();
						if (previousSize == originalSize)
						{
							previousFile.seekg(0, std::ios::beg);
							const auto previousDigest = wowpp::sha1(previousFile);
							if (previousDigest == sha1)
							{
								break;
							}
						}
					}
				}

				PreparedUpdate update;
				update.estimates.downloadSize = compressedSize;
				update.estimates.updateSize = originalSize;
				update.steps.push_back(PreparedUpdateStep(
				                           destination,
				                           [source, destination, compression, compressedSize]
				                           (const UpdateParameters & parameters) -> bool
				{
					const auto sourceFile = parameters.source->readFile(source);
					checkExpectedFileSize(source, compressedSize, sourceFile);

					bool doZLibUncompress = false;

					if (compression == "zlib")
					{
						doZLibUncompress = true;
					}
					else if (!compression.empty())
					{
						throw std::runtime_error(
						    "Unsupported compression type " + compression);
					}

					std::ofstream sinkFile(destination, std::ios::binary);
					if (!sinkFile)
					{
						throw std::runtime_error("Could not open output file " + destination);
					}
					copyWithProgress(
					    parameters,
					    *sourceFile.content,
					    sinkFile,
					    source,
					    compressedSize,
					    doZLibUncompress
					);

					//TODO: Copy step-wise
					return false;
				}));

				return update;
			}

			return PreparedUpdate();
		}

		PreparedUpdate FileSystemEntryHandler::finish(const PrepareParameters &/*parameters*/)
		{
			return PreparedUpdate();
		}
	}
}
