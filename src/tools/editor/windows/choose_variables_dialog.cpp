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
#include "choose_variables_dialog.h"
#include "ui_choose_variables_dialog.h"
#include "editor_application.h"

namespace wowpp
{
	namespace editor
	{
		ChooseVariablesDialog::ChooseVariablesDialog(EditorApplication &app)
			: QDialog()
			, m_ui(new Ui::ChooseVariablesDialog)
			, m_app(app)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

			// Automatically deleted since it's a QObject
			m_variableFilter = new QSortFilterProxyModel(this);
			m_variableFilter->setSourceModel(app.getVariableListModel());
			m_ui->variableView->setModel(m_variableFilter);

			connect(m_ui->variableView->selectionModel(),
				SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
				this, SLOT(onVariableSelectionChanged(QItemSelection, QItemSelection)));
		}

		void ChooseVariablesDialog::on_buttonBox_accepted()
		{
			// TODO
		}

		void ChooseVariablesDialog::on_variableFilter_textChanged(QString)
		{
			QRegExp::PatternSyntax syntax = QRegExp::RegExp;
			Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;

			QRegExp regExp(m_ui->variableFilter->text(), caseSensitivity, syntax);
			m_variableFilter->setFilterRegExp(regExp);
		}

		void ChooseVariablesDialog::onVariableSelectionChanged(const QItemSelection& selection, const QItemSelection& old)
		{
			// Get the selected unit
			m_selected.clear();
			if (selection.isEmpty())
				return;

			QItemSelection source = m_variableFilter->mapSelectionToSource(selection);
			if (source.isEmpty())
			{
				return;
			}

			for (const auto &i : source.indexes())
			{
				int index = i.row();
				if (index < 0)
				{
					return;
				}

				const auto &variable = m_app.getProject().variables.getTemplates().entry(index);
				m_selected.push_back(&variable);
			}
		}

	}
}
