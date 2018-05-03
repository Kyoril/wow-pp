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
#include "file_system_update_source.h"

namespace wowpp
{
	namespace updating
	{
		FileSystemUpdateSource::FileSystemUpdateSource(boost::filesystem::path root)
			: m_root(std::move(root))
		{
		}

		UpdateSourceFile FileSystemUpdateSource::readFile(
		    const std::string &path
		)
		{
			//TODO: filter paths with ".."
			const auto fullPath = m_root / path;
			std::unique_ptr<std::istream> file(new std::ifstream(
			                                       fullPath.string(), std::ios::binary));

			if (!static_cast<std::ifstream &>(*file))
			{
				throw std::runtime_error("File not found: " + fullPath.string());
			}

			//intentionally left empty
			boost::any internalData;
			boost::optional<boost::uintmax_t> size;

			return UpdateSourceFile(internalData, std::move(file), size);
		}
	}
}
