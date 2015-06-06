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
#include "ui_object_editor.h"
#include "string_editor.h"
#include "numeric_editor.h"
#include "min_max_editor.h"
#include "program.h"
#include <QMessageBox>

namespace wowpp
{
	namespace editor
	{
		ObjectEditor::ObjectEditor(Project &project)
			: QMainWindow()
			, m_project(project)
			, ui(new Ui::ObjectEditor)
		{
			// Setup auto generated ui
			ui->setupUi(this);
			
			// Add action
			ui->unitsTreeWidget->addAction(ui->actionCreateUnit);

			// Setup view model
			m_viewModel.reset(new PropertyViewModel(m_properties, nullptr));
			ui->unitPropertyWidget->setModel(m_viewModel.get());

			// Fill out unit tree widget
			for (auto &unit : m_project.units.getTemplates())
			{
				QString nameString = QString("%1 - %2").arg(QString::number(unit->id), 5, (QLatin1Char)'0').arg(unit->name.c_str());

				QTreeWidgetItem* item = new QTreeWidgetItem(QStringList(nameString));
				item->setIcon(0, QIcon(":/Units.png"));

				ui->unitsTreeWidget->addTopLevelItem(item);
			}
		}

		void ObjectEditor::on_unitsTreeWidget_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
		{
			// Get the selected unit
			int index = ui->unitsTreeWidget->indexOfTopLevelItem(current);
			if (index < 0)
			{
				return;
			}


			// Update properties
			m_properties.clear();

			// Get unit entry
			UnitEntry *unit = m_project.units.getTemplates().at(index).get();
			
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
				}
			}
		}

		void ObjectEditor::on_actionCreateUnit_triggered()
		{
			size_t maxId = 1;

			// Create unit
			auto entry = std::unique_ptr<UnitEntry>(new UnitEntry());
			entry->id = maxId;

			QString nameString = QString("%1 - %2").arg(QString::number(entry->id), 5, (QLatin1Char)'0').arg(entry->name.c_str());

			QTreeWidgetItem* item = new QTreeWidgetItem(QStringList(nameString));
			item->setIcon(0, QIcon(":/Units.png"));

			ui->unitsTreeWidget->addTopLevelItem(item);

			// Add new unit entry
			m_project.units.add(
				std::move(entry));
		}

		void ObjectEditor::on_actionSave_triggered()
		{
			Program *program = static_cast<Program*>(QCoreApplication::instance());
			if (program)
			{
				program->saveProject();
			}
		}

		void ObjectEditor::on_lineEdit_textChanged(const QString &newText)
		{
			const bool hideAll = !newText.isEmpty();
			
			// Fill out unit tree widget
			for (size_t i = 0; i < m_project.units.getTemplates().size(); ++i)
			{
				ui->unitsTreeWidget->topLevelItem(i)->setHidden(hideAll);
			}

			if (!hideAll)
			{
				return;
			}

			QList<QTreeWidgetItem*> items = ui->unitsTreeWidget->findItems(newText, Qt::MatchFlag::MatchContains, 0);
			for (size_t i = 0; i < items.size(); ++i)
			{
				items[i]->setHidden(false);
			}
		}
	}
}
