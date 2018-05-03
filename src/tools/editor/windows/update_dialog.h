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
#include "updater/updater_progress_handler.h"
#include "updater/prepare_progress_handler.h"
#include <QDialog>

// Forwards
namespace Ui
{
	class UpdateDialog;
}

namespace wowpp
{
	namespace editor
	{
		class EditorApplication;

		/// 
		class UpdateDialog : public QDialog, public wowpp::updating::IPrepareProgressHandler, public wowpp::updating::IUpdaterProgressHandler
		{
			Q_OBJECT

		public:

			/// 
			explicit UpdateDialog(EditorApplication &app);

		private:

			/// @copydoc 
			virtual void updateFile(const std::string &name, boost::uintmax_t size, boost::uintmax_t loaded) override;
			/// @copydoc 
			virtual void beginCheckLocalCopy(const std::string &name) override;

		signals:

			/// Sets the status message.
			void setStatusMessage(QString text);
			/// Sets the progress bar value.
			void setProgress(float percent);
			/// 
			void finishedUpdates(bool applyPatch);

		public slots:

			/// Sets the status message.
			void onSetStatusMessage(QString text);
			/// Sets the progress bar value.
			void onSetProgress(float percent);
			/// 
			void onFinishedUpdates(bool applyPatch);

		private:

			/// Processes updates
			void checkForUpdates();

		private:

			Ui::UpdateDialog *m_ui;
			EditorApplication &m_app;
			size_t m_lastLoaded;
			size_t m_loaded;
		};
	}
}
