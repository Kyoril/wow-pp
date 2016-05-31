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
#include "virtual_directory/file_system_reader.h"
#include "simple_file_format/sff_read_tree.h"
#include "simple_file_format/sff_load_file.h"
#include "log/default_log_levels.h"
#include <google/protobuf/io/coded_stream.h>

namespace wowpp
{
	namespace proto
	{
		/// Context for several functionalities during the load process.
		struct DataLoadContext : boost::noncopyable
		{
			typedef std::function<void(const String &)> OnError;
			typedef std::function<bool()> LoadLaterFunction;
			typedef std::vector<LoadLaterFunction> LoadLater;

			OnError onError;
			OnError onWarning;
			LoadLater loadLater;
			UInt32 version;

			virtual ~DataLoadContext()
			{

			}
			bool executeLoadLater()
			{
				bool success = true;

				for (const LoadLaterFunction &function : loadLater)
				{
					if (!function())
					{
						success = false;
					}
				}

				return success;
			}
		};

		template <class Context>
		struct ProjectLoader
		{
			struct ManagerEntry
			{
				typedef std::function < bool(std::istream &file, const String &fileName, Context &context) > LoadFunction;

				String name;
				LoadFunction load;

				template<typename T>
				ManagerEntry(
				    const String &name,
				    T &manager
				)
					: name(name)
					, load([this, name, &manager](
					           std::istream & file,
					           const String & fileName,
					           Context & context) mutable -> bool
				{
					return this->loadManagerFromFile(file, fileName, context, manager, name);
				})
				{
				}

			private:

				template<typename T>
				static bool loadManagerFromFile(
				    std::istream &file,
				    const String &fileName,
				    Context &context,
				    T &manager,
				    const String &arrayName)
				{
					return manager.load(file);
				}
			};

			typedef std::vector<ManagerEntry> Managers;

			typedef String::const_iterator StringIterator;

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
				    fileTable.getInteger<unsigned>("version", 5);
				if (projectVersion != 5)
				{
					ELOG("Unsupported project version: " << projectVersion);
					return false;
				}

				bool success = true;

				for (const auto &manager : managers)
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
					if (!manager.load(*managerFile, relativeFileName, context))
					{
						ELOG("Could not load '" << manager.name << "'");
						success = false;
					}
				}

				return success &&
				       context.executeLoadLater();
			}

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
}
