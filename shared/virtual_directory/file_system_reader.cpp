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

#include "file_system_reader.h"
#include <fstream>

namespace wowpp
{
	namespace virtual_dir
	{
		FileSystemReader::FileSystemReader(boost::filesystem::path directory)
			: m_directory(std::move(directory))
		{
		}

		file_type::Enum FileSystemReader::getType(
		    const Path &fileName
		)
		{
			auto fullPath = m_directory / fileName;
			const auto type = boost::filesystem::status(
			                      m_directory / fileName
			                  ).type();

			switch (type)
			{
			case boost::filesystem::regular_file:
				return file_type::File;
			case boost::filesystem::directory_file:
				return file_type::Directory;
			default:
				throw std::runtime_error("Unsupported physical file type: " + fullPath.string());
			}
		}

		std::unique_ptr<std::istream> FileSystemReader::readFile(
		    const Path &fileName,
		    bool openAsText)
		{
			const auto fullPath = m_directory / fileName;

			std::unique_ptr<std::istream> file(new std::ifstream(
			                                       fullPath.string().c_str(),
			                                       openAsText ? std::ios::in : std::ios::binary));

			if (static_cast<std::ifstream &>(*file))
			{
				return std::move(file);
			}

			return std::unique_ptr<std::istream>();
		}

		std::set<Path> FileSystemReader::queryEntries(
		    const Path &fileName)
		{
			std::set<Path> entries;
			const auto fullPath = m_directory / fileName;

			for (boost::filesystem::directory_iterator i(fullPath);
			        i != boost::filesystem::directory_iterator(); ++i)
			{
				entries.insert(
				    i->path().filename().string()
				);
			}

			return entries;
		}
	}
}
