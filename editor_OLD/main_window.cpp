//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
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

#include "main_window.h"
#include "ui_main_window.h"
#include "load_map.h"
#include "program.h"
#include "extract_dbc.h"

namespace wowpp
{
	namespace editor
	{
		MainWindow::MainWindow(Configuration &config, Project &project)
			: QMainWindow()
			, m_config(config)
			, m_project(project)
			, m_ui(new Ui::MainWindow)
		{
			m_ui->setupUi(this);
			setupToolBar();
		}

		void MainWindow::on_actionObjectEditor_triggered()
		{
            m_objectEditor.reset(new ObjectEditor(m_project));
            m_objectEditor->show();
		}

		void MainWindow::setupToolBar()
		{
			m_toolBar.reset(new QToolBar("Standard", this));
			m_toolBar->setMovable(false);
			m_toolBar->setFloatable(false);
			m_toolBar->setIconSize(QSize(16, 16));
			m_toolBar->addAction(m_ui->actionLoadMap);
			m_toolBar->addAction(m_ui->actionSave);
			m_toolBar->addSeparator();
			m_toolBar->addAction(m_ui->actionObjectEditor);
			addToolBar(m_toolBar.get());
		}

		void MainWindow::on_actionLoadMap_triggered()
		{
			LoadMap loadMapDialog(m_project);
			if (loadMapDialog.exec() == 1)
			{

			}
		}

		void MainWindow::on_actionSave_triggered()
		{
			Program *program = static_cast<Program*>(QCoreApplication::instance());
			if (program)
			{
				program->saveProject();
			}
		}

		void MainWindow::on_actionExit_triggered()
		{
			if (m_objectEditor)
			{
				m_objectEditor->close();
				m_objectEditor.reset();
			}

			close();
		}

		void MainWindow::on_actionExtractDBC_triggered()
		{
			ExtractDBC dbc(m_project);
			if (dbc.exec())
			{
				//TODO 
			}
		}

	}
}
