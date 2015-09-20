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

#pragma once

#include "path.h"
#include "reader.h"
#include <boost/variant.hpp>
#include <unordered_map>
#include <vector>

namespace wowpp
{
	namespace virtual_dir
	{
		struct MemoryFile
		{
			std::vector<char> content;
			size_t currentReaders;
			size_t currentWriters;


			MemoryFile();
		};


		struct MemoryDirectory
		{
			// boost::recursive_wrapper<> fixes compiler errors on Mac OS X using Clang 4+ with libstdc++11
			typedef boost::variant<boost::recursive_wrapper<MemoryDirectory>, boost::recursive_wrapper<MemoryFile> > Entry;


			std::unordered_map<Path, Entry> entries;
		};


		struct NotFound
		{
		};

		typedef boost::variant<NotFound, MemoryDirectory *, MemoryFile *> ResolveResult;

		ResolveResult resolve(MemoryDirectory &root, const Path &path);

		MemoryDirectory &createDirectories(
		    MemoryDirectory &root,
		    const Path &path
		);
	}
}
