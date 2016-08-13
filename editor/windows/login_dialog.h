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
#include <QDialog>

// Forwards
namespace Ui
{
	class LoginDialog;
}

namespace wowpp
{
	namespace editor
	{
		class EditorApplication;

		/// 
		class LoginDialog : public QDialog
		{
			Q_OBJECT

		public:

			/// 
			explicit LoginDialog(EditorApplication &app);

		public slots:

			void on_loginBtn_clicked();
			void on_helpBtn_clicked();

		private:

			void setLoginUiState(bool enabled);
			void checkForUpdates();

		private:

			Ui::LoginDialog *m_ui;
			EditorApplication &m_app;
			boost::signals2::scoped_connection m_onConnected, m_onDisconnect, m_onLoginResult;
		};
	}
}
