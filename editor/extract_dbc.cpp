//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
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

#include "extract_dbc.h"
#include "ui_extract_dbc.h"
#include "program.h"
#include "data/project.h"
#include <boost/filesystem.hpp>

namespace wowpp
{
	namespace editor
	{
		ExtractDBC::ExtractDBC(Project &project)
			: QDialog()
			, m_ui(new Ui::ExtractDBC)
			, m_project(project)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

			Program *program = static_cast<Program*>(QCoreApplication::instance());
			if (!program)
				return;

			// Find all dbc files
			Ogre::StringVectorPtr files = Ogre::ResourceGroupManager::getSingleton().listResourceNames("Locale");
			for (auto &file : *files)
			{
				if (Ogre::StringUtil::endsWith(file, ".dbc"))
				{
					// Extract it
					Ogre::DataStreamPtr ptr = Ogre::ResourceGroupManager::getSingleton().openResource(file, "Locale", false);
					ptr->size();

					Ogre::String baseName, extension, path;
					Ogre::StringUtil::splitFullFilename(file, baseName, extension, path);

					// Create the directory if needed
					const boost::filesystem::path dataPath(program->getConfiguration().dataPath);
					if (!boost::filesystem::is_directory(dataPath / "dbc"))
					{
						if (!boost::filesystem::create_directory(dataPath / "dbc"))
						{
							return;
						}
					}

					std::ofstream out((dataPath / "dbc" / (baseName + "." + extension)).string(), std::ios::binary | std::ios::out);
					if (out)
					{
						std::array<char, 1024> buffer;
						while (!ptr->eof())
						{
							size_t read = ptr->read(&buffer[0], buffer.size());
							if (read > 0)
							{
								out.write(&buffer[0], read);
							}
						}
					}
					out.close();
				}
			}
		}

		void ExtractDBC::on_cancelButton_clicked()
		{
			//TODO cancel import process

			close();
		}
	}
}
