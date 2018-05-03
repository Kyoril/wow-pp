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
#include "update_application.h"
#include "prepared_update.h"
#include "common/create_process.h"
#include "common/clock.h"

namespace wowpp
{
	namespace updating
	{
		namespace
		{
			std::string makeRandomString()
			{
				return boost::lexical_cast<std::string>(getCurrentTime());
			}
		}

		ApplicationUpdate updateApplication(
		    const boost::filesystem::path &applicationPath,
		    const PreparedUpdate &preparedUpdate
		)
		{
			ApplicationUpdate update;

			for (const wowpp::updating::PreparedUpdateStep & step : preparedUpdate.steps)
			{
				try
				{
					if (!boost::filesystem::equivalent(applicationPath, step.destinationPath))
					{
						continue;
					}
				}
				catch (const boost::filesystem::filesystem_error &ex)
				{
					std::cerr << ex.what() << '\n';
					continue;
				}

				update.perform = [applicationPath, step](
				                     const UpdateParameters & updateParameters,
				                     const char * const * argv,
				                     size_t argc) -> void
				{
					const boost::filesystem::path executableCopy =
					applicationPath.string() +
					"." +
					makeRandomString();

					boost::filesystem::rename(
					    applicationPath,
					    executableCopy
					);

					try
					{
						while (step.step(updateParameters)) {
							;
						}

						wowpp::makeExecutable(applicationPath.string());
					}
					catch (...)
					{
						//revert the copy in case of error
						try
						{
							boost::filesystem::rename(
							    executableCopy,
							    applicationPath
							);
						}
						catch (...)
						{
						}

						throw;
					}

					std::vector<std::string> arguments(argv, argv + argc);
					arguments.push_back("--remove-previous \"" + executableCopy.string() + "\"");
					wowpp::createProcess(applicationPath.string(), arguments);
				};

				break;
			}

			return update;
		}
	}
}
