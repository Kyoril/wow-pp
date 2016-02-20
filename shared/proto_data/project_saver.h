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

#include "common/typedefs.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	namespace proto
	{
		struct ProjectSaver
		{
			struct Manager
			{
				typedef std::function<bool(const String &)> SaveManager;

				String fileName;
				String name;
				SaveManager save;

				Manager()
				{
				}

				template<class T>
				Manager(const String &name,
				        const String &fileName,
				        const T &manager)
					: fileName(fileName)
					, name(name)
					, save([this, name, &manager](
					           const String & fileName) mutable -> bool
				{
					return this->saveManagerToFile(fileName, name, manager);
				})
				{
				}

			private:

				template<class T>
				static bool saveManagerToFile(
				    const String &filename,
				    const String &name,
				    const T &manager)
				{
					std::ofstream file(filename.c_str(), std::ios::out | std::ios::binary);
					if (!file)
					{
						ELOG("Could not save file '" << filename << "'");
						return false;
					}

					return manager.save(file);
				}
			};

			typedef std::vector<Manager> Managers;

			static bool save(const boost::filesystem::path &directory, const Managers &managers);
		};
	}
}
