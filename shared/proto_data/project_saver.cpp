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

// Content from PCH since we can't use them here
#include <functional>
#include <fstream>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/signals2.hpp>
#include <boost/date_time.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/uuid/sha1.hpp>
#include "common/sha1.h"

#include "project_saver.h"
#include "simple_file_format/sff_write.h"
#include "simple_file_format/sff_save_file.h"

namespace wowpp
{
	namespace proto
	{
		static bool saveAndAddManagerToTable(sff::write::Table<char> &fileTable, const boost::filesystem::path &directory, ProjectSaver::Manager &manager)
		{
			const String managerRelativeFileName = (manager.fileName + ".wppdat");
			const String managerAbsoluteFileName = (directory / managerRelativeFileName).string();

			if (!manager.save(managerAbsoluteFileName))
			{
				return false;
			}

			sff::write::Table<char> table(fileTable, manager.name, sff::write::Comma);
			table.addKey("file", managerRelativeFileName);
			std::ifstream srcFile(managerAbsoluteFileName, std::ios::in | std::ios::binary);
			if (srcFile)
			{
				std::ostringstream formatter;
				sha1PrintHex(formatter, sha1(srcFile));

				String hashString = formatter.str();
				table.addKey("sha1", hashString);

				manager.hash(hashString);
			}
			table.finish();

			return true;
		}

		static bool saveProjectToTable(sff::write::Table<char> &fileTable, const boost::filesystem::path &directory, ProjectSaver::Managers &managers)
		{
			fileTable.addKey("version", 6);

			bool success = true;

			for (ProjectSaver::Manager &manager : managers)
			{
				success =
				    saveAndAddManagerToTable(fileTable, directory, manager) &&
				    success;
			}

			return success;
		}

		bool ProjectSaver::save(const boost::filesystem::path &directory, Managers &managers)
		{
			const String projectFileName = (directory / "project.txt").string();

			return
			    sff::save_file(projectFileName, std::bind(saveProjectToTable, std::placeholders::_1, directory, std::ref(managers)));
		}
	}
}
