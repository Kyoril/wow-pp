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
#include "update_dialog.h"
#include "ui_update_dialog.h"
#include "editor_application.h"
#include "log/default_log_levels.h"
#include "updater/open_source_from_url.h"
#include "updater/update_url.h"
#include "updater/prepare_parameters.h"
#include "updater/update_parameters.h"
#include "updater/prepare_update.h"
#include "updater/update_source.h"
#include "updater/update_application.h"
#include <thread>

namespace wowpp
{
	namespace editor
	{
		namespace
		{
			boost::thread updatingThread;
			boost::uintmax_t updateSize = 0;
			boost::uintmax_t updated = 0;
			boost::uintmax_t lastUpdateStatus = 0;

			static void createDirectory(const boost::filesystem::path &directory)
			{
				try
				{
					boost::filesystem::create_directories(directory);
				}
				catch (const boost::filesystem::filesystem_error &e)
				{
					ELOG(e.what());
				}
			}
		}
		
		UpdateDialog::UpdateDialog(EditorApplication &app)
			: QDialog()
			, m_ui(new Ui::UpdateDialog)
			, m_app(app)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

			// Start the update thread

			ILOG("Connecting to the update server...");

			// Prepare preprocessor flags
			std::set<std::string> conditionsSet;

			// Operating system
#ifdef _WIN32
			conditionsSet.insert("WINDOWS");
#endif

			// Which architecture is being used?
			if (sizeof(void *) == 8)
			{
				conditionsSet.insert("X64");
			}
			else
			{
				conditionsSet.insert("X86");
			}

			// Open the source from the target url by downloading it from the server
			auto source = wowpp::updating::openSourceFromUrl(
				wowpp::updating::UpdateURL("http://127.0.0.1/wowpp_editor")
			);

			// Prepare the downloaded data
			ILOG("Preparing data...");
			wowpp::updating::PrepareParameters prepareParameters(
				std::move(source),
				conditionsSet,
				false,
				*this
			);

			ILOG("Updating files...");

			const std::string outputDir = "./";
			const auto preparedUpdate = wowpp::updating::prepareUpdate(
				outputDir,
				prepareParameters
			);

			// Get download size
			updateSize = preparedUpdate.estimates.downloadSize;
			DLOG("Download size: " << preparedUpdate.estimates.downloadSize);
			DLOG("Update size: " << updateSize);

			wowpp::updating::UpdateParameters updateParameters(
				std::move(prepareParameters.source),
				false,
				*this
			);

			{
				boost::asio::io_service dispatcher;
				for(const auto & step : preparedUpdate.steps)
				{
					dispatcher.post(
						[&]()
					{
						std::string errorMessage;
						try
						{
							while (step.step(updateParameters)) {
								;
							}

							return;
						}
						catch (const std::exception &ex)
						{
							errorMessage = ex.what();
						}
						catch (...)
						{
							errorMessage = "Caught an exception not derived from std::exception";
						}

						dispatcher.stop();
						dispatcher.post([errorMessage]()
						{
							throw std::runtime_error(errorMessage);
						});
					});
				}

				dispatcher.run();
			}

			DLOG("Updated " << updated << " / " << updateSize << " bytes");
		}

		void UpdateDialog::updateFile(const std::string &name, boost::uintmax_t size, boost::uintmax_t loaded)
		{
			ILOG("File " << name << ": " << loaded << " / " << size);

			/*boost::mutex::scoped_lock guiLock(m_guiMutex);

			// Increment download counter
			if (loaded >= lastUpdateStatus)
			{
				updated += loaded - lastUpdateStatus;
			}
			lastUpdateStatus = loaded;

			// Status string
			std::stringstream statusStream;
			statusStream << "Updating file " << name << "...";
			SetDlgItemTextA(dialogHandle, IDC_STATUS_LABEL, statusStream.str().c_str());

			// Current file
			std::stringstream currentStream;
			currentStream << loaded << " / " << size << " bytes";
			SetDlgItemTextA(dialogHandle, IDC_CURRENT, currentStream.str().c_str());

			// Progress bar
			int percent = static_cast<int>(static_cast<float>(updated) / static_cast<float>(updateSize) * 100.0f);
			SendMessageA(GetDlgItem(dialogHandle, IDC_PROGRESS_BAR), PBM_SETPOS, percent, 0);

			// Overall progress
			std::stringstream totalStream;
			totalStream << updated << " / " << updateSize << " bytes (" << percent << "%)";
			SetDlgItemTextA(dialogHandle, IDC_TOTAL, totalStream.str().c_str());

			// Log file process
			if (loaded >= size)
			{
				// Reset counter
				lastUpdateStatus = 0;
				ILOG("Successfully loaded file " << name << " (Size: " << size << " bytes)");
			}*/
		}

		void UpdateDialog::beginCheckLocalCopy(const std::string &name)
		{

		}
	}
}
