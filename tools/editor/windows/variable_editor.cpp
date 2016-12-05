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
#include "variable_editor.h"
#include "editor_application.h"
#include "variable_type_delegate.h"
#include "ui_variable_editor.h"
#include "add_variable_dialog.h"
#include <QRegExp>

namespace wowpp
{
	namespace editor
	{
		VariableEditor::VariableEditor(EditorApplication &app)
			: QMainWindow()
			, m_application(app)
			, m_ui(new Ui::VariableEditor())
		{
			m_ui->setupUi(this);

			m_viewModel = new VariableViewModel(m_application.getProject(), this);
			m_ui->variableWidget->setModel(m_viewModel);

			m_ui->variableWidget->setColumnWidth(0, 25);
			m_ui->variableWidget->setColumnWidth(1, 250);
			m_ui->variableWidget->setColumnWidth(2, 75);

			m_ui->variableWidget->setItemDelegateForColumn(2, new VariableTypeDelegate(this));
		}

		void VariableEditor::on_addVariableBtn_clicked()
		{
			AddVariableDialog dlg(m_application);
			if (dlg.exec() == QDialog::Accepted)
			{
				// Refresh view as the variable has been added already
				m_viewModel->layoutChanged();

				// Mark as changed
				m_application.markAsChanged(dlg.getAddedId(), pp::editor_team::data_entry_type::Variables, pp::editor_team::data_entry_change_type::Added);
			}
		}
	}
}
