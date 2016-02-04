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

#include "spell_effect_dialog.h"
#include "ui_spell_effect_dialog.h"
#include "proto_data/project.h"
#include "editor_application.h"
#include "game/defines.h"

namespace wowpp
{
	namespace editor
	{
		SpellEffectDialog::SpellEffectDialog(EditorApplication &app, const proto::SpellEffect &effect)
			: QDialog()
			, m_ui(new Ui::SpellEffectDialog)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

			setWindowTitle(QString("%1 - Effect #%2").arg(windowTitle()).arg(effect.index()));
			m_ui->lineEdit->setText(game::constant_literal::spellEffectNames.getName(
				static_cast<game::SpellEffect>(effect.type())).c_str());

			m_ui->lineEdit_9->setText(game::constant_literal::targetNames.getName(
				static_cast<game::Targets>(effect.targeta())).c_str());
			m_ui->lineEdit_8->setText(game::constant_literal::targetNames.getName(
				static_cast<game::Targets>(effect.targetb())).c_str());

			m_ui->lineEdit_14->setText(game::constant_literal::auraTypeNames.getName(
				static_cast<game::AuraType>(effect.aura())).c_str());
			m_ui->lineEdit_15->setText(QString("%1").arg(effect.amplitude()));

			m_ui->lineEdit_2->setText(QString("%1").arg(effect.basepoints()));
			m_ui->lineEdit_3->setText(QString("%1").arg(effect.diesides()));
			m_ui->lineEdit_4->setText(QString("%1").arg(effect.basedice()));
			m_ui->lineEdit_5->setText(QString("%1").arg(effect.diceperlevel()));
			m_ui->lineEdit_6->setText(QString("%1").arg(effect.pointsperlevel()));
			m_ui->lineEdit_12->setText(QString("%1").arg(effect.pointspercombopoint()));

			m_ui->lineEdit_7->setText(QString("%1").arg(effect.mechanic()));

			m_ui->lineEdit_13->setText(QString("%1").arg(effect.radius()));

			m_ui->lineEdit_10->setText(QString("%1").arg(effect.miscvaluea()));
			m_ui->lineEdit_11->setText(QString("%1").arg(effect.miscvalueb()));
			m_ui->lineEdit_16->setText(QString("%1").arg(effect.multiplevalue()));
			m_ui->lineEdit_17->setText(QString("%1").arg(effect.chaintarget()));
			m_ui->lineEdit_18->setText(QString("%1").arg(effect.itemtype()));

			const auto *triggerSpell = app.getProject().spells.getById(effect.triggerspell());
			if (triggerSpell)
			{
				m_ui->triggerSpellButton->setText(QString("%1").arg(effect.triggerspell()));
			}
			else
			{
				if (effect.triggerspell() != 0)
				{
					m_ui->triggerSpellButton->setText(QString("INVALID SPELL ID %1").arg(effect.triggerspell()));
				}
				else
				{
					m_ui->triggerSpellButton->setText("NONE");
				}
			}

		}
	}
}
