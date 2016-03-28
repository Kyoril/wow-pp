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

			// Connect slots
			connect(m_ui->actionSave, SIGNAL(triggered()), &m_application, SLOT(saveUnsavedChanges()));
			connect(m_ui->actionObjectEditor, SIGNAL(triggered()), &m_application, SLOT(showObjectEditor()));
			connect(m_ui->actionTriggerEditor, SIGNAL(triggered()), &m_application, SLOT(showTriggerEditor()));

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
			ILOG("Triggered " << action->objectName().toStdString());
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
				m_ogreWindow->setScene(std::move(scene));

				if (entry->id() == 0)
				{
					camera->setPosition(3043.45f, 681.295f, 66.8132f);
				}
			}
		}
	}
}
