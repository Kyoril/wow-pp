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

#include "project_saver.h"

namespace wowpp
{
	static bool saveAndAddManagerToTable(sff::write::Table<char> &fileTable, const boost::filesystem::path &directory, const ProjectSaver::Manager &manager)
	{
		const String managerRelativeFileName = (manager.fileName + ".txt");
		fileTable.addKey(manager.name, managerRelativeFileName);

		const String managerAbsoluteFileName = (directory / managerRelativeFileName).string();

		return manager.save(managerAbsoluteFileName);
	}

	static bool saveProjectToTable(sff::write::Table<char> &fileTable, const boost::filesystem::path &directory, const ProjectSaver::Managers &managers)
	{
		fileTable.addKey("version", 4);

		bool success = true;

		for (const ProjectSaver::Manager &manager : managers)
		{
			success =
			    saveAndAddManagerToTable(fileTable, directory, manager) &&
			    success;
		}

		return success;
	}

	bool ProjectSaver::save(const boost::filesystem::path &directory, const Managers &managers)
	{
		const String projectFileName = (directory / "project.txt").string();

		return
		    sff::save_file(projectFileName,	boost::bind(saveProjectToTable, _1, directory, boost::cref(managers)));
	}
}
