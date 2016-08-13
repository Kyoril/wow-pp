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
#include "memory_reader.h"
#include "memory_directory.h"
#include "memory_file_device.h"

namespace wowpp
{
	namespace virtual_dir
	{
		MemoryReader::MemoryReader(MemoryDirectory &directory)
			: m_directory(directory)
		{
		}

		namespace
		{
			struct FileTypeVisitor : boost::static_visitor<file_type::Enum>
			{
				FileTypeVisitor()
				{
				}

				file_type::Enum operator ()(NotFound) const
				{
					throw std::runtime_error("File not found");
				}

				file_type::Enum operator ()(MemoryDirectory *) const
				{
					return file_type::Directory;
				}

				file_type::Enum operator ()(MemoryFile *) const
				{
					return file_type::File;
				}
			};
		}

		file_type::Enum MemoryReader::getType(
		    const Path &fileName
		)
		{
			const auto resolved = resolve(m_directory, fileName);
			const FileTypeVisitor visitor;
			return boost::apply_visitor(visitor, resolved);
		}

		namespace
		{
			struct ReadFileVisitor : boost::static_visitor<void>
			{
				//GCC 4.7 does not like non-copyable return values for
				//operator () in a static_visitor
				std::unique_ptr<std::istream> result;

				explicit ReadFileVisitor(const std::string &fileName)
					: m_fileName(fileName)
				{
				}

				void operator ()(NotFound)
				{
					throw std::runtime_error(m_fileName + ": File not found");
				}

				void operator ()(MemoryDirectory *)
				{
					throw std::runtime_error(m_fileName +
					                         ": Cannot open directory as a file to read");
				}

				void operator ()(MemoryFile *file)
				{
					result.reset(
					    new boost::iostreams::stream<MemoryFileDevice>(
					        *file,
					        false
					    ));
				}

			private:

				const std::string &m_fileName;
			};
		}

		std::unique_ptr<std::istream> MemoryReader::readFile(
		    const Path &fileName,
		    bool /*openAsText*/
		)
		{
			const auto resolved = resolve(m_directory, fileName);
			ReadFileVisitor visitor(fileName);
			boost::apply_visitor(visitor, resolved);
			assert(visitor.result);
			return std::move(visitor.result);
		}

		namespace
		{
			typedef std::set<Path> PathSet;

			struct QueryEntriesVisitor : boost::static_visitor<PathSet>
			{
				explicit QueryEntriesVisitor(const std::string &rootName)
					: m_rootName(rootName)
				{
				}

				PathSet operator ()(NotFound) const
				{
					throw std::runtime_error(
					    m_rootName + " cannot be queried for entries because "
					    "it's unknown");
				}

				PathSet operator ()(MemoryDirectory *dir) const
				{
					std::set<Path> entries;
					for (const auto &entry : dir->entries)
					{
						entries.insert(entry.first);
					}
					return entries;
				}

				PathSet operator ()(MemoryFile *) const
				{
					throw std::runtime_error(
					    m_rootName + " cannot be queried for entries because "
					    "it's not a directory");
				}

			private:

				const std::string &m_rootName;
			};
		}

		std::set<Path> MemoryReader::queryEntries(
		    const Path &fileName
		)
		{
			const auto dirEntry = resolve(m_directory, fileName);
			const QueryEntriesVisitor visitor(fileName);
			return boost::apply_visitor(visitor, dirEntry);
		}
	}
}
