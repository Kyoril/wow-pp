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

#include "object_editor.h"
#include "main_window.h"
#include "editor_application.h"
#include "string_editor.h"
#include "numeric_editor.h"
#include "min_max_editor.h"
#include "ui_object_editor.h"
#include "choose_trigger_dialog.h"
#include "game/defines.h"
#include <QRegExp>
#include <utility>

namespace wowpp
{
	namespace editor
	{
		ObjectEditor::ObjectEditor(EditorApplication &app)
			: QMainWindow()
			, m_application(app)
			, m_ui(new Ui::ObjectEditor())
			, m_selected(nullptr)
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

		namespace
		{
			static float ArmorReductionHelper(UInt32 armor, UInt32 level)
			{
				float tmp = 0.0f;
				if (level < 60)
				{
					tmp = armor / (armor + 400.0f + 85.0f * level);
				}
				else if (level < 70)
				{
					tmp = armor / (armor - 22167.5f + 467.5f * level);
				}
				else
				{
					tmp = armor / (armor + 10557.5f);
				}

				// Hard caps
				if (tmp < 0.0f) tmp = 0.0f;
				if (tmp > 0.75f) tmp = 0.75f;
				return tmp * 100.0f;
			}
		}

		void ObjectEditor::onUnitSelectionChanged(const QItemSelection& selection, const QItemSelection& old)
		{
			// Get the selected unit
			if (selection.isEmpty())
			{
				m_selected = nullptr;
				return;
			}

			QItemSelection source = m_unitFilter->mapSelectionToSource(selection);
			if (source.isEmpty())
			{
				m_selected = nullptr;
				return;
			}

			int index = source.indexes().first().row();
			if (index < 0)
			{
				m_selected = nullptr;
				return;
			}

			// Update properties
			m_properties.clear();

			// Get unit entry
			UnitEntry *unit = m_application.getProject().units.getTemplates().at(index).get();
			m_selected = unit;
			if (!unit)
				return;

			// Add unit properties
			m_properties.push_back(PropertyPtr(new NumericProperty("Entry", UInt32Ref(unit->id), true)));
			m_properties.push_back(PropertyPtr(new StringProperty("Name", unit->name)));
			m_properties.push_back(PropertyPtr(new StringProperty("Subname", unit->subname)));
			m_properties.push_back(PropertyPtr(new MinMaxProperty("Level", UInt32Ref(unit->minLevel), UInt32Ref((unit->maxLevel)))));
			m_properties.push_back(PropertyPtr(new MinMaxProperty("Health", UInt32Ref(unit->minLevelHealth), UInt32Ref(unit->maxLevelHealth))));
			m_properties.push_back(PropertyPtr(new MinMaxProperty("Mana", UInt32Ref(unit->minLevelMana), UInt32Ref(unit->maxLevelMana))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Damage School", UInt32Ref(unit->damageSchool))));
			m_properties.push_back(PropertyPtr(new MinMaxProperty("Melee Damage", FloatRef(unit->minMeleeDamage), FloatRef(unit->maxMeleeDamage))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Melee Attack Power", UInt32Ref(unit->attackPower))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Melee Attack Time", UInt32Ref(unit->meleeBaseAttackTime))));
			m_properties.push_back(PropertyPtr(new MinMaxProperty("Ranged Damage", FloatRef(unit->minRangedDamage), FloatRef(unit->maxRangedDamage))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Ranged Attack Power", UInt32Ref(unit->rangedAttackPower))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Ranged Attack Time", UInt32Ref(unit->rangedBaseAttackTime))));
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
			m_properties.push_back(PropertyPtr(new NumericProperty("Armor", UInt32Ref(unit->armor), false, [unit]() -> QVariant { return QString("%1%").arg(ArmorReductionHelper(unit->armor, unit->minLevel)); })));
			m_properties.push_back(PropertyPtr(new NumericProperty("Holy Resistance", UInt32Ref(unit->resistances[0]))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Fire Resistance", UInt32Ref(unit->resistances[1]))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Nature Resistance", UInt32Ref(unit->resistances[2]))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Frost Resistance", UInt32Ref(unit->resistances[3]))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Shadow Resistance", UInt32Ref(unit->resistances[4]))));
			m_properties.push_back(PropertyPtr(new NumericProperty("Arcane Resistance", UInt32Ref(unit->resistances[5]))));
			m_properties.push_back(PropertyPtr(new MinMaxProperty("Loot Gold", UInt32Ref(unit->minLootGold), UInt32Ref(unit->maxLootGold))));
			m_properties.push_back(PropertyPtr(new MinMaxProperty("Experience", UInt32Ref(unit->xpMin), UInt32Ref(unit->xpMax))));

			// Update the view 
			m_viewModel->layoutChanged();

			m_ui->unitTriggerWidget->clear();
			for (const auto *trigger : unit->triggers)
			{
				m_ui->unitTriggerWidget->addItem(
					QString(trigger->name.c_str()));
			}
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

		void ObjectEditor::on_unitAddTriggerBtn_clicked()
		{
			if (!m_selected)
				return;

			ChooseTriggerDialog dialog(m_application);
			auto result = dialog.exec();
			if (result == QDialog::Accepted)
			{
				const auto *newTrigger = dialog.getSelectedTrigger();
				if (newTrigger)
				{
					auto it = std::find_if(m_selected->triggers.begin(), m_selected->triggers.end(), [&newTrigger](const TriggerEntry *trigger) -> bool
					{
						return (trigger->id == newTrigger->id);
					});

					if (it == m_selected->triggers.end())
					{
						m_selected->triggers.push_back(newTrigger);
						m_ui->unitTriggerWidget->addItem(
							QString(newTrigger->name.c_str()));

						m_application.markAsChanged();
					}
				}
			}
		}

		void ObjectEditor::on_unitRemoveTriggerBtn_clicked()
		{
			if (!m_selected)
				return;

			// Find selected trigger
			auto index = m_ui->unitTriggerWidget->currentIndex();
			if (index.isValid())
			{
				int row = index.row();
				if (row < 0 || row >= m_selected->triggers.size())
					return;

				std::swap(m_selected->triggers.back(), m_selected->triggers[row]);
				m_selected->triggers.pop_back();

				auto *taken = m_ui->unitTriggerWidget->takeItem(row);
				delete taken;

				m_application.markAsChanged();
			}
		}

	}
}
