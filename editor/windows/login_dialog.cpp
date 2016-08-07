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
#include "wowpp_protocol/wowpp_editor_team.h"
#include "log/default_log_levels.h"
#include "proto_data/project.h"
#include "common/sha1.h"
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
				m_onConnected = connector->connected.connect([this](bool success)
				{
					if (success)
					{
						m_ui->statusLabel->setText("Authenticating...");
						
						// Convert username to uppercase letters
						String username = m_ui->lineEdit->text().toStdString();
						std::transform(username.begin(), username.end(), username.begin(), ::toupper);

						String password = m_ui->lineEdit_2->text().toStdString();
						std::transform(password.begin(), password.end(), password.begin(), ::toupper);

						std::stringstream strm(username + ":" + password);

						// Send login request
						m_app.getTeamConnector()->editorLoginRequest(
							username,
							sha1(strm));

						accept();
					}
					else
					{
						m_ui->statusLabel->setText("Could not connect!");
						setLoginUiState(true);
					}
				});
				m_onDisconnect = connector->disconnected.connect([this]()
				{
					m_ui->statusLabel->setText("Connection lost!");
					setLoginUiState(true);
				});
				m_onLoginResult = connector->loginResult.connect([this](UInt32 result, UInt32 serverVersion)
				{
					if (serverVersion != pp::editor_team::ProtocolVersion)
					{
						m_ui->statusLabel->setText("Server protocol version mismatch");
					}
					else
					{
						switch (result)
						{
							case pp::editor_team::login_result::Success:
								m_ui->statusLabel->setText("Checking for updates...");
								return;
							case pp::editor_team::login_result::WrongUserName:
							case pp::editor_team::login_result::WrongPassword:
								m_ui->statusLabel->setText("Wrong username or password!");
								break;
							case pp::editor_team::login_result::AlreadyLoggedIn:
								m_ui->statusLabel->setText("Account already logged in!");
								break;
							case pp::editor_team::login_result::TimedOut:
								m_ui->statusLabel->setText("Login request timed out...");
								break;
							case pp::editor_team::login_result::ServerError:
								m_ui->statusLabel->setText("Internal server error...");
								break;
							default:
								m_ui->statusLabel->setText(QString("Unknown error code %1").arg(result));
								break;
						}
					}

					setLoginUiState(true);
				});
			}
		}

		/*
		// Load local project for comparison and send hash values to the server
		proto::Project project;
		if (!project.load(m_app.getConfiguration().dataPath))
		{
			// Could not load project, so no hashs to find
		}

		// Setup hash map
		std::map<String, String> projectHashs;
		projectHashs["spells"] = project.spells.hashString;
		projectHashs["units"] = project.units.hashString;
		projectHashs["objects"] = project.objects.hashString;
		projectHashs["maps"] = project.maps.hashString;
		projectHashs["emotes"] = project.emotes.hashString;
		projectHashs["unit_loot"] = project.unitLoot.hashString;
		projectHashs["object_loot"] = project.objectLoot.hashString;
		projectHashs["item_loot"] = project.itemLoot.hashString;
		projectHashs["skinning_loot"] = project.skinningLoot.hashString;
		projectHashs["skills"] = project.skills.hashString;
		projectHashs["trainers"] = project.trainers.hashString;
		projectHashs["vendors"] = project.vendors.hashString;
		projectHashs["talents"] = project.talents.hashString;
		projectHashs["items"] = project.items.hashString;
		projectHashs["item_sets"] = project.itemSets.hashString;
		projectHashs["classes"] = project.classes.hashString;
		projectHashs["races"] = project.races.hashString;
		projectHashs["levels"] = project.levels.hashString;
		projectHashs["triggers"] = project.triggers.hashString;
		projectHashs["zones"] = project.zones.hashString;
		projectHashs["quests"] = project.quests.hashString;
		projectHashs["factions"] = project.factions.hashString;
		projectHashs["faction_templates"] = project.factionTemplates.hashString;
		projectHashs["area_triggers"] = project.areaTriggers.hashString;
		projectHashs["spell_categories"] = project.spellCategories.hashString;
		*/

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
