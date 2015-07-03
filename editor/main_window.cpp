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
#include "editor_application.h"
#include "object_editor.h"
#include "ui_main_window.h"
#include <QCloseEvent>

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
		}

		void MainWindow::closeEvent(QCloseEvent *qEvent)
		{
			if (m_application.shutdown())
			{
				qEvent->accept();
			}
			else
			{
				qEvent->ignore();
			}
		}

		void MainWindow::on_actionExit_triggered()
		{
			close();
		}

	}
}
