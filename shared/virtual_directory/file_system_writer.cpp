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

#include "file_system_writer.h"
#include <fstream>

namespace wowpp
{
	namespace virtual_dir
	{
		FileSystemWriter::FileSystemWriter(boost::filesystem::path directory)
			: m_directory(std::move(directory))
		{
		}

		std::unique_ptr<std::ostream> FileSystemWriter::writeFile(
		    const Path &fileName,
		    bool openAsText,
		    bool createDirectories
		)
		{
			const auto fullPath = (m_directory / fileName);
			if (createDirectories)
			{
				boost::filesystem::create_directories(
				    fullPath.parent_path()
				);
			}

			std::unique_ptr<std::ostream> file(new std::ofstream(
			                                       fullPath.string(),
			                                       (openAsText ? std::ios::out : std::ios::binary)));

			if (*file)
			{
				return file;
			}

			return std::unique_ptr<std::ostream>();
		}
	}
}
