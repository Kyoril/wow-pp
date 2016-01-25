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

#include "choose_spell_dialog.h"
#include "ui_choose_spell_dialog.h"
#include "templates/basic_template.h"
#include "editor_application.h"

namespace wowpp
{
	namespace editor
	{
		ChooseSpellDialog::ChooseSpellDialog(EditorApplication &app)
			: QDialog()
			, m_ui(new Ui::ChooseSpellDialog)
			, m_app(app)
			, m_selected(nullptr)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

			// Automatically deleted since it's a QObject
			m_spellFilter = new QSortFilterProxyModel(this);
			m_spellFilter->setSourceModel(app.getSpellListModel());
			m_ui->listView->setModel(m_spellFilter);

			connect(m_ui->listView->selectionModel(),
				SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
				this, SLOT(onSpellSelectionChanged(QItemSelection, QItemSelection)));
		}

		void ChooseSpellDialog::on_buttonBox_accepted()
		{
			// TODO
		}

		void ChooseSpellDialog::on_spellFilter_textChanged(QString)
		{
			QRegExp::PatternSyntax syntax = QRegExp::RegExp;
			Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;

			QRegExp regExp(m_ui->spellFilter->text(), caseSensitivity, syntax);
			m_spellFilter->setFilterRegExp(regExp);
		}

		void ChooseSpellDialog::onSpellSelectionChanged(const QItemSelection& selection, const QItemSelection& old)
		{
			// Get the selected unit
			m_selected = nullptr;
			if (selection.isEmpty())
				return;

			QItemSelection source = m_spellFilter->mapSelectionToSource(selection);
			if (source.isEmpty())
				return;

			int index = source.indexes().first().row();
			if (index < 0)
			{
				return;
			}

			const auto &spell = m_app.getProject().spells.getTemplates().entry(index);
			m_selected = &spell;
		}

	}
}
