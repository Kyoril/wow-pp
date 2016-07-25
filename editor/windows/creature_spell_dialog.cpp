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
#include "creature_spell_dialog.h"
#include "ui_creature_spell_dialog.h"
#include "choose_spell_dialog.h"
#include "editor_application.h"

namespace wowpp
{
	namespace editor
	{
		CreatureSpellDialog::CreatureSpellDialog(EditorApplication &app, proto::UnitSpellEntry *entry)
			: QDialog()
			, m_ui(new Ui::CreatureSpellDialog)
			, m_app(app)
			, m_selectedSpell(nullptr)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

			if (entry)
			{
				m_ui->priorityBox->setValue(entry->priority());
				m_ui->cooldownBox->setValue(entry->mincooldown());
				m_ui->cooldownBox_2->setValue(entry->maxcooldown());
				m_ui->spinBox->setValue(entry->mininitialcooldown());
				m_ui->spinBox_2->setValue(entry->maxinitialcooldown());
				m_ui->repeatedBox->setChecked(entry->repeated());
				m_ui->targetBox->setCurrentIndex(entry->target());
				if (entry->spellid())
				{
					const auto *spell = m_app.getProject().spells.getById(entry->spellid());
					if (spell)
					{
						m_selectedSpell = spell;
						m_ui->spellButton->setText(spell->name().c_str());
					}
				}
			}
		}

		void CreatureSpellDialog::on_buttonBox_accepted()
		{
			// TODO
		}
		UInt32 CreatureSpellDialog::getPriority() const
		{
			return static_cast<UInt32>(m_ui->priorityBox->value());
		}
		bool CreatureSpellDialog::getRepeated() const
		{
			return m_ui->repeatedBox->isChecked();
		}
		Int32 CreatureSpellDialog::getMinCooldown() const
		{
			return m_ui->cooldownBox->value();
		}
		Int32 CreatureSpellDialog::getMaxCooldown() const
		{
			return m_ui->cooldownBox_2->value();
		}
		Int32 CreatureSpellDialog::getMinInitialCooldown() const
		{
			return m_ui->spinBox->value();
		}
		Int32 CreatureSpellDialog::getMaxInitialCooldown() const
		{
			return m_ui->spinBox_2->value();
		}
		UInt32 CreatureSpellDialog::getTarget() const
		{
			return static_cast<UInt32>(m_ui->targetBox->currentIndex());
		}
		void CreatureSpellDialog::on_spellButton_clicked()
		{
			ChooseSpellDialog dialog(m_app);
			auto result = dialog.exec();
			if (result == QDialog::Accepted)
			{
				const auto *spell = dialog.getSelectedSpell();
				if (spell)
				{
					m_selectedSpell = spell;
					m_ui->spellButton->setText(spell->name().c_str());
				}
			}
		}
	}
}
