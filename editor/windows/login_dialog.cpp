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
#include "login_dialog.h"
#include "ui_login_dialog.h"
#include "editor_application.h"
#include "team_connector.h"
#include "log/default_log_levels.h"
#include <QDesktopServices>
#include <QUrl>

namespace wowpp
{
	namespace editor
	{
		LoginDialog::LoginDialog(EditorApplication &app)
			: QDialog()
			, m_ui(new Ui::LoginDialog)
			, m_app(app)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

			auto *connector = m_app.getTeamConnector();
			if (connector)
			{
				connector->connected.connect([this](bool success)
				{
					if (success)
					{
						m_ui->statusLabel->setText("Authenticating...");
						accept();
					}
					else
					{
						m_ui->statusLabel->setText("Could not connect!");
						setLoginUiState(true);
					}
				});
				connector->disconnected.connect([this]()
				{
					m_ui->statusLabel->setText("Connection lost!");
					setLoginUiState(true);
				});
			}
		}

		void LoginDialog::on_loginBtn_clicked()
		{
			auto *connector = m_app.getTeamConnector();
			if (!connector)
			{
				return;
			}

			// Disable form input
			setLoginUiState(false);
			m_ui->statusLabel->setText("Connecting...");

			// Try to connect and send login request afterwards
			connector->tryConnect();
		}

		void LoginDialog::on_helpBtn_clicked()
		{
			QDesktopServices::openUrl(QUrl("http://www.wow-pp.eu/register"));
		}

		void LoginDialog::setLoginUiState(bool enabled)
		{
			m_ui->loginBtn->setEnabled(enabled);
			m_ui->lineEdit->setEnabled(enabled);
			m_ui->lineEdit_2->setEnabled(enabled);
			m_ui->checkBox->setEnabled(enabled);
		}
	}
}
