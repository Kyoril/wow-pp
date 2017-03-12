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
#include "main_window.h"
#include "editor_application.h"
#include "object_editor.h"
#include "load_map_dialog.h"
#include "ui_main_window.h"
#include "ogre_wrappers/ogre_blp_codec.h"
#include "ogre_wrappers/ogre_mpq_archive.h"
#include "world/world_editor.h"
#include "go_to_dialog.h"
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QSettings>
#include "game/map.h"

namespace wowpp
{
	namespace editor
	{
		MainWindow::MainWindow(EditorApplication &app)
			: QMainWindow()
			, m_application(app)
			, m_ui(new Ui::MainWindow())
		{
			m_ui->setupUi(this);

			m_onLog = wowpp::g_DefaultLog.signal().connect(
				[this](const wowpp::LogEntry & entry)
			{
				QString colorString;
				switch (entry.level->color)
				{
					case log_color::Black: colorString = "#000"; break;
					case log_color::White: colorString = "#fff"; break;
					case log_color::Blue: colorString = "#0096ff"; break;
					case log_color::Green: colorString = "#00ff1e"; break;
					case log_color::Grey: colorString = "#ccc"; break;
					case log_color::Purple: colorString = "#d800ff"; break;
					case log_color::Red: colorString = "#ff0024"; break;
					case log_color::Yellow: colorString = "#ffea00"; break;
					default:
						colorString = "#fff";
						break;
				}

				QString text = QString("<font color=\"%0\">%1</font><br>").arg(colorString).arg(entry.message.c_str());
				emit addLogEntry(text);
			});

			connect(this, SIGNAL(addLogEntry(const QString&)), this, SLOT(on_addLogEntry(const QString&)));

			// Connect slots
			connect(m_ui->actionSave, SIGNAL(triggered()), &m_application, SLOT(saveUnsavedChanges()));
			connect(m_ui->actionObjectEditor, SIGNAL(triggered()), &m_application, SLOT(showObjectEditor()));
			connect(m_ui->actionTriggerEditor, SIGNAL(triggered()), &m_application, SLOT(showTriggerEditor()));
			connect(m_ui->actionVariables, SIGNAL(triggered()), &m_application, SLOT(showVariableEditor()));

			// Create new label with this form as parent (qt will handle destruction)
			m_pageLabel = new QLabel(this);
			m_ui->statusBar->addWidget(m_pageLabel);

			// Layout will be deleted automatically on window destruction
			QVBoxLayout *layout = new QVBoxLayout(m_ui->renderWidget);
			layout->setMargin(0);

			// Window will be automatically deleted on destruction
			m_ogreWindow = new QtOgreWindow();

			// Container will be automatically deleted on destruction
			QWidget *container = QWidget::createWindowContainer(m_ogreWindow, nullptr);
			layout->addWidget(container, 1);

			readSettings();
		}

		void MainWindow::closeEvent(QCloseEvent *qEvent)
		{
			if (m_application.shutdown())
			{
				QSettings settings("WoW++", "WoW++ Editor");
				settings.setValue("geometry", saveGeometry());
				settings.setValue("windowState", saveState());

				qEvent->accept();
			}
			else
			{
				qEvent->ignore();
			}
		}

		void MainWindow::showEvent(QShowEvent * qEvent)
		{
			QMainWindow::showEvent(qEvent);

			// Automatically deleted since it's a QObject
			m_unitFilter = new QSortFilterProxyModel;
			m_unitFilter->setSourceModel(m_application.getUnitListModel());

			m_objectFilter = new QSortFilterProxyModel;
			m_objectFilter->setSourceModel(m_application.getObjectListModel());
		}

		void MainWindow::readSettings()
		{
			QSettings settings("WoW++", "Wow++ Editor");
			restoreGeometry(settings.value("geometry").toByteArray());
			restoreState(settings.value("windowState").toByteArray());
		}

		void MainWindow::on_actionExit_triggered()
		{
			close();
		}

		void MainWindow::on_Movement_triggered(QAction * action)
		{
			if (action == m_ui->actionSelect)
			{
				m_application.setTransformTool(transform_tool::Select);
			}
			else if (action == m_ui->actionTranslate)
			{
				m_application.setTransformTool(transform_tool::Translate);
			}
			else if (action == m_ui->actionRotate)
			{
				m_application.setTransformTool(transform_tool::Rotate);
			}
			else if (action == m_ui->actionScale)
			{
				m_application.setTransformTool(transform_tool::Scale);
			}

			// Refresh render window
			QApplication::postEvent(m_ui->renderWidget, new QEvent(QEvent::UpdateRequest));
		}

		void MainWindow::on_actionDelete_triggered()
		{
			// Check if any objects are selected
			auto &selection = m_application.getSelection();
			if (selection.empty())
				return;

			for (auto &selected : selection.getSelectedObjects())
			{
				selected->deselect();
				selected->remove();
			}

			selection.clear();
		}

		void MainWindow::on_comboBox_currentIndexChanged(int index)
		{
			switch (index)
			{
				case 1:
					m_ui->unitPaletteView->setModel(m_unitFilter);
					break;
				case 2:
					m_ui->unitPaletteView->setModel(m_objectFilter);
					break;
				default:
					m_ui->unitPaletteView->setModel(nullptr);
					break;
			}
		}

		void MainWindow::on_actionUnit_Palette_triggered()
		{
			m_ui->unitPalette->show();
		}

		void MainWindow::on_unitPaletteFilter_editingFinished()
		{
			QRegExp::PatternSyntax syntax = QRegExp::RegExp;
			Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;

			QRegExp regExp(m_ui->unitPaletteFilter->text(), caseSensitivity, syntax);
			m_objectFilter->setFilterRegExp(regExp);
			m_unitFilter->setFilterRegExp(regExp);
		}

		void MainWindow::on_actionOutput_Log_triggered()
		{
			m_ui->outputLogWidget->show();
		}

		void MainWindow::on_actionDisplayNavMesh_triggered()
		{
			m_application.showNavMesh();
		}

		void MainWindow::on_actionGoTo_triggered()
		{
			GoToDialog dialog(m_application);
			if (dialog.exec() == 0)
			{
				auto *camera = m_ogreWindow->getCamera();
				if (!camera)
					return;

				float x, y, z;
				dialog.getLocation(x, y, z);

				camera->setPosition(x, y, z);
			}
		}

		void MainWindow::on_addLogEntry(const QString &string)
		{
			QTextCursor cursor = m_ui->outputLogBox->textCursor();
			cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
			m_ui->outputLogBox->setTextCursor(cursor);

			m_ui->outputLogBox->insertHtml(string);
		}

		void MainWindow::on_actionLoadMap_triggered()
		{
			LoadMapDialog dialog(m_application);
			int result = dialog.exec();
			if (result == QDialog::Accepted)
			{
				auto *entry = dialog.getSelectedMap();
				if (!entry)
					return;

				auto *sceneMgr = m_ogreWindow->getSceneManager();
				auto *camera = m_ogreWindow->getCamera();
				if (!sceneMgr || !camera)
					return;

				// Reset scene
				m_onPageChanged.disconnect();
				m_ogreWindow->setScene(std::unique_ptr<IScene>());

				auto list = Ogre::ResourceGroupManager::getSingleton().getResourceGroups();
				for (auto &name : list)
				{
					Ogre::ResourceGroupManager::getSingleton().unloadUnreferencedResourcesInGroup(name, false);
				}

				camera->setNearClipDistance(0.5f);
				camera->setFarClipDistance(1500.0f);
				camera->setOrientation(Ogre::Quaternion(Ogre::Degree(90.0f), Ogre::Vector3::UNIT_X));
				camera->setFixedYawAxis(true, Ogre::Vector3::UNIT_Z);

				camera->setPosition(0.0f, 0.0f, 110.9062f);
				camera->pitch(Ogre::Degree(-45.0f));

				std::unique_ptr<WorldEditor> scene(
					new WorldEditor(m_application, *sceneMgr, *camera, *entry, m_application.getProject()));

				m_onPageChanged = scene->pageChanged.connect([this](paging::PagePosition position) {
					this->m_pageLabel->setText(QString("ADT Tile: %0 x %1").arg(position[0]).arg(position[1]));
				});

				m_ogreWindow->setScene(std::move(scene));

				// TODO: Remove this as it should be data driven. Also, there should be support for favorite locations
				// in the editor.
				if (entry->id() == 1)
				{
					// GM Island
					camera->setPosition(16226.2f, 16257.0f, 13.2022f);

					// Night Elf Starting Zone
					//camera->setPosition(10304.0f, 870.033f, 1336.65f);
				}
				else if (entry->id() == 0)
				{
					camera->setPosition(2020.13f, 1512.17f, 109.491f);
					//camera->setPosition(1762.19995f, -1244.80005f, 62.2191010f);
				}
				else if (entry->id() == 530)
				{
					// Blood elf starting zone
					//camera->setPosition(8719.53f, -6657.67f, 72.7551f);

					// Draenei starting zone
					//camera->setPosition(-4043.94f, -13782.8f, 75.2519f);

					// The Dark Portal - Hellfire Peninsula
					camera->setPosition(-327.082f, 1018.85f, 54.2546f);
				}
			}
		}
	}
}
