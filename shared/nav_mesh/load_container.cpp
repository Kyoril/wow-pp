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

#include "load_container.h"
#include "binary_io/stream_source.h"
#include "binary_io/reader.h"
#include "common/typedefs.h"
#include <algorithm>

namespace wowpp
{
	namespace navigation
	{
		bool loadNavigationMeshFile(
		    std::istream &file,
		    NavigationMesh &mesh)
		{
			io::StreamSource source(file);
			io::Reader reader(source);

			UInt32 version = 0;
			reader
				>> io::read<NetUInt32>(version)
				;

			if (version != 0)
			{
				return false;
			}

			static_assert(sizeof(mesh[0]) == 1, "Navigation mesh may not be empty");
			return reader
				>> io::read_container<NetUInt32>(mesh)
				;
		}
	}
}
