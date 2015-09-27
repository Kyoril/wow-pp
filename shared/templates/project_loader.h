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
#include "load_template_array.h"
#include "basic_template_load_context.h"
#include "simple_file_format/sff_load_file.h"
#include "log/default_log_levels.h"
#include "virtual_directory/reader.h"
#include <functional>
#include <istream>
#include <vector>

namespace wowpp
{
	typedef String::const_iterator StringIterator;

	template <class Context>
	struct ProjectLoader
	{
		struct ManagerFileContents
		{
			std::string text;
			sff::read::tree::Table<StringIterator> tree;
		};


		struct ManagerEntry
		{
			typedef std::function < bool (std::istream &file, const String &fileName, Context &context,
			                                ManagerFileContents &contents) > LoadFunction;


			String name;
			LoadFunction load;


			template <class T, class Ptr, class LoadTemplate>
			ManagerEntry(
			    const String &name,
			    TemplateManager<T, Ptr> &manager,
			    LoadTemplate loadTemplate
			)
				: name(name)
				, load([this, loadTemplate, name, &manager](
				           std::istream &file,
				           const String &fileName,
				           Context &context,
						   ManagerFileContents &contents) mutable -> bool
				{
					return this->loadManagerFromFile(file, fileName, context, manager, name, loadTemplate, contents);
				})
			{
			}

		private:

			template <class T, class Ptr, class LoadTemplate>
			static bool loadManagerFromFile(
			    std::istream &file,
			    const String &fileName,
			    Context &context,
			    TemplateManager<T, Ptr> &manager,
			    const String &arrayName,
				LoadTemplate loadTemplate,
			    ManagerFileContents &contents)
			{
				if (!loadSffFile(
				            contents.tree,
				            file,
				            contents.text,
				            fileName))
				{
					return false;
				}

				contents.tree.tryGetInteger("version", context.version);

				if (const sff::read::tree::Array<StringIterator> *const array =
				            contents.tree.getArray(arrayName))
				{
					if (!loadTemplateArray(manager, *array, context, loadTemplate))
					{
						ELOG("Could not load from the template array '" << arrayName << "'");
						return false;
					}

					return true;
				}
				else
				{
					return false;
				}
			}
		};


		typedef std::vector<ManagerEntry> Managers;


		static bool load(virtual_dir::IReader &directory, const Managers &managers, Context &context)
		{
			const virtual_dir::Path projectFilePath = "project.txt";
			const auto projectFile = directory.readFile(projectFilePath, false);
			if (!projectFile)
			{
				ELOG("Could not open project file '" << projectFilePath << "'");
				return false;
			}

			std::string fileContent;
			sff::read::tree::Table<StringIterator> fileTable;
			if (!loadSffFile(fileTable, *projectFile, fileContent, projectFilePath))
			{
				return false;
			}

			const auto projectVersion =
			    fileTable.getInteger<unsigned>("version", 2);

			if (projectVersion != 2)
			{
				ELOG("Unsupported project version: " << projectVersion);
				return false;
			}

			std::vector<std::unique_ptr<ManagerFileContents>> managerContents;
			bool success = true;

			for (const auto & manager : managers)
			{
				String relativeFileName;

				if (!fileTable.tryGetString(manager.name, relativeFileName))
				{
					success = false;

					ELOG("File name of '" << manager.name << "' is missing in the project");
					continue;
				}

				const auto managerFile = directory.readFile(relativeFileName, false);
				if (!managerFile)
				{
					success = false;

					ELOG("Could not open file '" << relativeFileName << "'");
					continue;
				}

				managerContents.emplace_back(new ManagerFileContents);

				if (!manager.load(*managerFile, relativeFileName, context, *managerContents.back()))
				{
					ELOG("Could not load '" << manager.name << "'");
					success = false;
				}
			}

			return success &&
			       context.executeLoadLater();
		}

	private:

		template <class FileName>
		static bool loadSffFile(
		    sff::read::tree::Table<StringIterator> &fileTable,
		    std::istream &source,
		    std::string &content,
		    const FileName &fileName)
		{
			try
			{
				sff::loadTableFromFile(fileTable, content, source);
				return true;
			}
			catch (const sff::read::ParseException<StringIterator> &exception)
			{
				const auto line = std::count<StringIterator>(
				                      content.begin(),
				                      exception.position.begin,
				                      '\n');

				const String relevantLine(
				    exception.position.begin,
				    std::find<StringIterator>(exception.position.begin, content.end(), '\n'));

				ELOG("Error in SFF file " << fileName << ":" << (1 + line));
				ELOG("Parser error: " << exception.what() << " at '" << relevantLine << "'");
				return false;
			}
		}
	};
}
