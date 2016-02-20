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

#include "memory_writer.h"
#include "memory_directory.h"
#include "memory_file_device.h"

namespace wowpp
{
	namespace virtual_dir
	{
		MemoryWriter::MemoryWriter(MemoryDirectory &directory)
			: m_directory(directory)
		{
		}

		namespace
		{
			MemoryFile &touchFile(
			    MemoryDirectory &dir,
			    const Path &leaf
			)
			{
				const auto existing = dir.entries.find(leaf);
				if (existing == dir.entries.end())
				{
					const auto created = dir.entries.insert(std::make_pair(
					        leaf,
					        MemoryFile())).first;

					return boost::get<MemoryFile>(created->second);
				}
				else
				{
					if (auto *const file =
					            boost::get<MemoryFile>(&existing->second))
					{
						return *file;
					}
					else
					{
						throw std::runtime_error(
						    "Could not create file " + leaf);
					}
				}
			}
		}

		std::unique_ptr<std::ostream> MemoryWriter::writeFile(
		    const Path &fileName,
		    bool /*openAsText*/,
		    bool /*createDirectories*/ //TODO do not ignore this flag
		)
		{
			const auto leafSplitFileName = splitLeaf(fileName);
			auto &directory = createDirectories(
			                      m_directory,
			                      leafSplitFileName.first
			                  );

			auto &file = touchFile(directory, leafSplitFileName.second);

			std::unique_ptr<std::ostream> result(
			    new boost::iostreams::stream<MemoryFileDevice>(
			        file,
			        true
			    ));
			return result;
		}
	}
}
