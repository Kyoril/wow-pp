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

#include "object_editor.h"
#include "main_window.h"
#include "editor_application.h"
#include "string_editor.h"
#include "numeric_editor.h"
#include "min_max_editor.h"
#include "ui_object_editor.h"
#include "game/defines.h"
#include <QRegExp>

namespace wowpp
{
	namespace editor
	{
		ObjectEditor::ObjectEditor(EditorApplication &app)
			: QMainWindow()
			, m_application(app)
			, m_ui(new Ui::ObjectEditor())
		{
			m_ui->setupUi(this);

			// Setup view model
			m_viewModel = new PropertyViewModel(m_properties, nullptr);
			m_ui->unitPropertyWidget->setModel(m_viewModel);

			// Automatically deleted since it's a QObject
			m_unitFilter = new QSortFilterProxyModel;
			m_unitFilter->setSourceModel(app.getUnitListModel());
			m_ui->unitsListView->setModel(m_unitFilter);

			// Automatically deleted since it's a QObject
			m_spellFilter = new QSortFilterProxyModel;
			m_spellFilter->setSourceModel(app.getSpellListModel());
			m_ui->spellsListView->setModel(m_spellFilter);

			// Automatically deleted since it's a QObject
			m_itemFilter = new QSortFilterProxyModel;
			m_itemFilter->setSourceModel(app.getItemListModel());
			m_ui->itemsListView->setModel(m_itemFilter);
			
			connect(m_ui->unitsListView->selectionModel(),
				SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
				this, SLOT(onUnitSelectionChanged(QItemSelection, QItemSelection)));
			connect(m_ui->spellsListView->selectionModel(),
				SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
				this, SLOT(onSpellSelectionChanged(QItemSelection, QItemSelection)));
			connect(m_ui->itemsListView->selectionModel(),
				SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
				this, SLOT(onItemSelectionChanged(QItemSelection, QItemSelection)));

			connect(m_ui->actionSave, SIGNAL(triggered()), &m_application, SLOT(saveUnsavedChanges()));
		}

		void ObjectEditor::on_unitFilter_editingFinished()
		{
			QRegExp::PatternSyntax syntax = QRegExp::RegExp;
			Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;

			QRegExp regExp(m_ui->unitFilter->text(), caseSensitivity, syntax);
			m_unitFilter->setFilterRegExp(regExp);
		}

		void ObjectEditor::on_spellFilter_editingFinished()
		{
			QRegExp::PatternSyntax syntax = QRegExp::RegExp;
			Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;

			QRegExp regExp(m_ui->spellFilter->text(), caseSensitivity, syntax);
			m_spellFilter->setFilterRegExp(regExp);
		}

		void ObjectEditor::on_itemFilter_editingFinished()
		{
			QRegExp::PatternSyntax syntax = QRegExp::RegExp;
			Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;

			QRegExp regExp(m_ui->itemFilter->text(), caseSensitivity, syntax);
			m_itemFilter->setFilterRegExp(regExp);
		}

		void ObjectEditor::onUnitSelectionChanged(const QItemSelection& selection, const QItemSelection& old)
		{
			// Get the selected unit
			if (selection.isEmpty())
				return;

			QItemSelection source = m_unitFilter->mapSelectionToSource(selection);
			if (source.isEmpty())
				return;

			int index = source.indexes().first().row();
			if (index < 0)
			{
				return;
			}

			// Update properties
			m_properties.clear();

			// Get unit entry
			UnitEntry *unit = m_application.getProject().units.getTemplates().at(index).get();
			if (!unit)
				return;

			// Add unit properties
			m_properties.push_back(PropertyPtr(new NumericProperty("Entry", UInt32Ref(unit->id), true)));
			m_properties.push_back(PropertyPtr(new StringProperty("Name", unit->name)));
			m_properties.push_back(PropertyPtr(new StringProperty("Subname", unit->subname)));
			m_properties.push_back(PropertyPtr(new MinMaxProperty("Level", UInt32Ref(unit->minLevel), UInt32Ref((unit->maxLevel)))));
			m_properties.push_back(PropertyPtr(new MinMaxProperty("Health", UInt32Ref(unit->minLevelHealth), UInt32Ref(unit->maxLevelHealth))));
			m_properties.push_back(PropertyPtr(new MinMaxProperty("Mana", UInt32Ref(unit->minLevelMana), UInt32Ref(unit->maxLevelMana))));
			m_properties.push_back(PropertyPtr(new MinMaxProperty("Melee Damage", FloatRef(unit->minMeleeDamage), FloatRef(unit->maxMeleeDamage))));
			m_properties.push_back(PropertyPtr(new MinMaxProperty("Ranged Damage", FloatRef(unit->minRangedDamage), FloatRef(unit->maxRangedDamage))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Scale", FloatRef(unit->scale))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Male Model ID", UInt32Ref(unit->maleModel))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Female Model ID", UInt32Ref(unit->femaleModel))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Alliance Faction ID", UInt32Ref(unit->allianceFactionID))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Horde Faction ID", UInt32Ref(unit->hordeFactionID))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Family", UInt32Ref(unit->family))));
			m_properties.push_back(PropertyPtr(new NumericProperty("NPC Flags", UInt32Ref(unit->npcFlags))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Unit Flags", UInt32Ref(unit->unitFlags))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Dynamic Flags", UInt32Ref(unit->dynamicFlags))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Extra Flags", UInt32Ref(unit->extraFlags))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Creature Type Flags", UInt32Ref(unit->creatureTypeFlags))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Walk Speed", FloatRef(unit->walkSpeed))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Run Speed", FloatRef(unit->runSpeed))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Unit Class", UInt32Ref(unit->unitClass))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Rank", UInt32Ref(unit->rank))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Armor", UInt32Ref(unit->armor))));
			for (size_t i = 0; i < unit->resistances.size(); ++i)
			{
				std::stringstream strm;
				strm << "Resistance " << i;
				m_properties.push_back(PropertyPtr(new NumericProperty(strm.str(), UInt32Ref(unit->resistances[i]))));
			}
			m_properties.push_back(PropertyPtr(new NumericProperty("Melee Attack Time", UInt32Ref(unit->meleeBaseAttackTime))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Ranged Attack Time", UInt32Ref(unit->rangedBaseAttackTime))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Damage School", UInt32Ref(unit->damageSchool))));
			m_properties.push_back(PropertyPtr(new MinMaxProperty("Loot Gold", UInt32Ref(unit->minLootGold), UInt32Ref(unit->maxLootGold))));
			/*
			bool regeneratesHealth;
			*/

			// Update the view 
			m_viewModel->layoutChanged();
		}
		
		void ObjectEditor::on_unitPropertyWidget_doubleClicked(QModelIndex index)
		{
			// Get the clicked property
			int row = index.row();
			if (row < 0)
			{
				// Invalid?
				return;
			}

			// Now we want to get the property of this
			std::unique_ptr<QDialog> dialog;

			// Determine prop type
			Property &prop = *m_properties.at(row);
			if (!prop.isReadOnly())
			{
				auto &propType = typeid(prop);

				// Create editor
				if (propType == typeid(StringProperty))
				{
					StringProperty &stringProp = reinterpret_cast<StringProperty &>(prop);
					dialog.reset(new StringEditor(stringProp));
				}
				else if (propType == typeid(NumericProperty))
				{
					NumericProperty &uintProp = reinterpret_cast<NumericProperty &>(prop);
					dialog.reset(new NumericEditor(uintProp));
				}
				else if (propType == typeid(MinMaxProperty))
				{
					MinMaxProperty &minMaxUIntProp = reinterpret_cast<MinMaxProperty &>(prop);
					dialog.reset(new MinMaxEditor(minMaxUIntProp));
				}

				// Display dialog
				if (dialog &&
					dialog->exec() == QDialog::Accepted)
				{
					// Value changed
					m_viewModel->layoutChanged();
					m_application.markAsChanged();
				}
			}
		}

		void ObjectEditor::onSpellSelectionChanged(const QItemSelection& selection, const QItemSelection& old)
		{
			// Get the selected unit
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
	
			// Get spell entry
			SpellEntry *spell = m_application.getProject().spells.getTemplates().at(index).get();
			if (!spell)
				return;

			m_ui->spellIdField->setText(QString::number(spell->id));
			m_ui->spellNameField->setText(spell->name.c_str());

			// Determine the cast time of this spell
			Int64 castTime = 0;
			if (spell->castTimeIndex != 0)
			{
				// Find spell cast time by it's index
				const auto *castTimeEntry = m_application.getProject().castTimes.getById(spell->castTimeIndex);
				if (castTimeEntry)
				{
					castTime = castTimeEntry->castTime;
				}
			}

			m_ui->castTimeField->setText(QString::number(castTime));
			m_ui->cooldownField->setText(QString::number(spell->cooldown));
			m_ui->resourceField->setCurrentIndex(spell->powerType);
			m_ui->costField->setText(QString::number(spell->cost));

			// Attributes
			for (size_t i = 1; i <= 32; ++i)
			{
				QCheckBox *box = m_ui->attributeTabs->findChild<QCheckBox*>(QString("attr_%1_box").arg(i));
				if (box)
				{
					const bool hasAttribute = (spell->attributes & (1 << (i - 1))) != 0;
					box->setChecked(hasAttribute);
				}
			}

			// Attributes Ex 1 - 6
			for (size_t j = 1; j <= spell->attributesEx.size(); ++j)
			{
				for (size_t i = 1; i <= 32; ++i)
				{
					QCheckBox *box = m_ui->attributeTabs->findChild<QCheckBox*>(QString("attr_ex_%1_box_%2").arg(i).arg(j));
					if (box)
					{
						const bool hasAttribute = (spell->attributesEx[j - 1] & (1 << (i - 1))) != 0;
						box->setChecked(hasAttribute);
					}
				}
			}

			// Check all spell buttons
			for (size_t i = 0; i < 3; ++i)
			{
				QPushButton *button = nullptr;
				switch (i)
				{
				case 0:
					button = m_ui->effectButton1;
					break;
				case 1:
					button = m_ui->effectButton2;
					break;
				case 2:
					button = m_ui->effectButton3;
					break;
				}

				if (button)
				{
					// Get effect name
					QString effectName = QString("NONE");
					if (i < spell->effects.size())
					{
						effectName = game::constant_literal::spellEffectNames.getName(spell->effects[i].type).c_str();
					}

					button->setText(effectName);
				}
			}
		}

		void ObjectEditor::onItemSelectionChanged(const QItemSelection& selection, const QItemSelection& old)
		{
			// Get the selected unit
			if (selection.isEmpty())
				return;

			QItemSelection source = m_itemFilter->mapSelectionToSource(selection);
			if (source.isEmpty())
				return;

			int index = source.indexes().first().row();
			if (index < 0)
			{
				return;
			}

			// Get item entry
			ItemEntry *item = m_application.getProject().items.getTemplates().at(index).get();
			if (!item)
				return;
		}

}
}
