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
#include "template_manager.h"
#include "basic_template_save_context.h"
#include "simple_file_format/sff_write.h"
#include "simple_file_format/sff_read_tree.h"
#include "simple_file_format/sff_save_file.h"
#include "log/default_log_levels.h"
#include <boost/filesystem.hpp>
#include <functional>
#include <vector>
#include <map>

namespace wowpp
{
	struct ProjectSaver
	{
		struct Manager
		{
			typedef std::function<bool (const String &)> SaveManager;


			template <class T>
			struct SaveObjectMethod
			{
				typedef void (T::*Type)(BasicTemplateSaveContext &) const;
			};


			String fileName;
			String name;
			SaveManager save;


			Manager();

			template <class T, class Ptr>
			Manager(const String &name,
			        const String &fileName,
			        const TemplateManager<T, Ptr> &manager,
			        typename SaveObjectMethod<T>::Type saveObject)
				: fileName(fileName)
				, name(name)
				, save(std::bind(&Manager::saveManagerToFile<T, Ptr>, std::placeholders::_1, name, std::cref(manager), saveObject))
			{
			}

		private:

			template <class T, class Ptr>
			static bool saveManagerToTable(
			    const TemplateManager<T, Ptr> &manager,
			    typename SaveObjectMethod<T>::Type saveObject,
			    const String &arrayName,
			    sff::write::Table<char> &table)
			{
				sff::write::Array<char> objectArray(table, arrayName, sff::write::MultiLine);

				typedef typename TemplateManager<T, Ptr>::Identifier Identifier;
				std::map<Identifier, const T *> templatesSortedById;
				for (const auto &dataObject : manager.getTemplates())
				{
					templatesSortedById.insert(std::make_pair(dataObject->id, dataObject.get()));
				}
				for (const auto &entry : templatesSortedById)
				{
					const auto &dataObject = *entry.second;
					sff::write::Table<char> objectTable(objectArray, sff::write::Comma);
					BasicTemplateSaveContext context(objectTable);
					(dataObject.*saveObject)(context);
					objectTable.finish();
				}

				objectArray.finish();
				return true;
			}

			template <class T, class Ptr>
			static bool saveManagerToFile(
			    const String &fileName,
			    const String &name,
			    const TemplateManager<T, Ptr> &manager,
			    typename SaveObjectMethod<T>::Type saveObject)
			{
				if (!sff::save_file(
				            fileName,
				            std::bind(saveManagerToTable<T, Ptr>, std::cref(manager), saveObject, name, std::placeholders::_1)))
				{
					ELOG("Could not save file '" << fileName << "'");
					return false;
				}

				return true;
			}
		};

		typedef std::vector<Manager> Managers;

		static bool save(const boost::filesystem::path &directory, const Managers &managers);
	};
}
