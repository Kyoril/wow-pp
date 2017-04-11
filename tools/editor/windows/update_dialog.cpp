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
#include "common/create_process.h"
#include <thread>

namespace wowpp
{
	namespace editor
	{
		namespace
		{
			std::thread updatingThread;
			std::uintmax_t updateSize = 0;

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
			, m_lastLoaded(0)
			, m_loaded(0)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

			connect(this, SIGNAL(setStatusMessage(QString)), this, SLOT(onSetStatusMessage(QString)));
			connect(this, SIGNAL(setProgress(float)), this, SLOT(onSetProgress(float)));
			connect(this, SIGNAL(finishedUpdates(bool)), this, SLOT(onFinishedUpdates(bool)));

			// Start the update thread
			ILOG("Connecting to the update server...");
			updatingThread = std::thread(std::bind(&UpdateDialog::checkForUpdates, this));
		}

		void UpdateDialog::updateFile(const std::string &name, boost::uintmax_t size, boost::uintmax_t loaded)
		{
			if (m_lastLoaded == 0)
			{
				std::stringstream statusStream;
				statusStream << "Updating file " << name << "...";
				emit setStatusMessage(statusStream.str().c_str());
			}

			// Increment download counter
			if (loaded >= m_lastLoaded)
			{
				m_loaded += loaded - m_lastLoaded;
				const float percent = static_cast<float>(m_loaded) / static_cast<float>(updateSize);
				emit setProgress(percent);
			}
			m_lastLoaded = loaded;

			if (loaded >= size)
			{
				m_lastLoaded = 0;
			}

			/*
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

		void UpdateDialog::checkForUpdates()
		{
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

			try
			{
				// Open the source from the target url by downloading it from the server
				auto source = wowpp::updating::openSourceFromUrl(
					wowpp::updating::UpdateURL("http://patch.wow-pp.eu")
				);

				// Prepare the downloaded data
				emit setStatusMessage("Preparing patch data...");
				wowpp::updating::PrepareParameters prepareParameters(
					std::move(source),
					conditionsSet,
					false,
					*this
				);

				// Create update directory
				boost::filesystem::path outputPath = "update";
				if (!boost::filesystem::exists(outputPath))
					createDirectory(outputPath);

				emit setStatusMessage("Updating files...");
				const auto preparedUpdate = wowpp::updating::prepareUpdate(
					"",
					outputPath.string(),
					prepareParameters
				);

				// Get download size
				updateSize = preparedUpdate.estimates.downloadSize;

				wowpp::updating::UpdateParameters updateParameters(
					std::move(prepareParameters.source),
					false,
					*this
				);

				{
					boost::asio::io_service dispatcher;
					for (const auto & step : preparedUpdate.steps)
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

				DLOG("Updated " << m_loaded << " / " << updateSize << " bytes");
			}
			catch (const std::exception &e)
			{
				emit setStatusMessage(e.what());
				return;
			}

			emit finishedUpdates(m_loaded != 0);
		}

		void UpdateDialog::onSetStatusMessage(QString text)
		{
			m_ui->statusLabel->setText(text);
		}

		void UpdateDialog::onSetProgress(float percent)
		{
			m_ui->progressBar->setValue(static_cast<int>(percent * 100.0f));
		}

		void UpdateDialog::onFinishedUpdates(bool applyPatch)
		{
			// Wait for the thread to finish
			updatingThread.join();

			if (applyPatch)
			{
				int processId = ::getpid();
#ifdef _WIN32
				const std::string process = "update_helper.exe";
#else
				const std::string process = "update_helper";
#endif

				std::vector<std::string> arguments;
				arguments.push_back("--pid");
				arguments.push_back(std::to_string(processId));
				arguments.push_back("--dir");
				arguments.push_back("update");
				createProcess(process, arguments);

				close();
			}
			else
			{
				accept();
			}
		}
	}
}
