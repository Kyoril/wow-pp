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
#include "memory_directory.h"

namespace wowpp
{
	namespace virtual_dir
	{
		MemoryFile::MemoryFile()
			: currentReaders(0)
			, currentWriters(0)
		{
		}

		ResolveResult resolve(MemoryDirectory &root, const Path &path)
		{
			const auto parts = splitRoot(path);
			if (parts.first == ".")
			{
				return resolve(root, parts.second);
			}

			if (parts.first == "")
			{
				assert(parts.second.empty());
				return &root;
			}

			const auto e = root.entries.find(parts.first);
			if (e != root.entries.end())
			{
				auto &entry = e->second;

				if (MemoryDirectory *const subDir = boost::get<MemoryDirectory>(&entry))
				{
					return resolve(*subDir, parts.second);
				}
				else if (parts.second.empty())
				{
					return &boost::get<MemoryFile &>(entry);
				}
			}

			return NotFound();
		}

		namespace
		{
			MemoryDirectory &createDirectory(
			    MemoryDirectory &root,
			    const Path &name
			)
			{
				assert(std::find(name.begin(), name.end(), PathSeparator) == name.end());
				if (name == "" ||
				        name == ".")
				{
					return root;
				}

				const auto existing = root.entries.find(name);
				if (existing == root.entries.end())
				{
					const auto created = root.entries.insert(
					                         std::make_pair(name, MemoryDirectory())).first;

					return boost::get<MemoryDirectory>(created->second);
				}
				else
				{
					if (auto *const directory =
					            boost::get<MemoryDirectory>(&existing->second))
					{
						return *directory;
					}
					else
					{
						throw std::runtime_error(
						    "Could not create directory " + name);
					}
				}
			}
		}

		MemoryDirectory &createDirectories(
		    MemoryDirectory &root,
		    const Path &path
		)
		{
			MemoryDirectory *current = &root;
			std::pair<Path, Path> rootSplit;
			rootSplit.second = path;
			do
			{
				rootSplit = splitRoot(rootSplit.second);
				current = &createDirectory(*current, rootSplit.first);
			}
			while (!rootSplit.second.empty());
			return *current;
		}
	}
}
