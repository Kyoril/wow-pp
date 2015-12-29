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
#include "loot_dialog.h"
#include "choose_trigger_dialog.h"
#include "game/defines.h"
#include "import_dialog.h"
#include <QRegExp>
#include <utility>
#include <memory>

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
			static QVariant ArmorMiscValue(const proto::UnitEntry &unit)
			{
				UInt32 armor = unit.armor();
				UInt32 level = (unit.minlevel() + unit.maxlevel()) / 2;
				if (level > 70) level = 70;

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

				return QString("%1% reduction at lvl %2").arg(tmp * 100.0f).arg(level);
			}
		
			static QVariant MeleeAttackMiscValue(const proto::UnitEntry &unit)
			{
				const float atkSpeed = (unit.meleeattacktime() / 1000.0f);
				UInt32 dmgBonus = unit.attackpower() / 14.0f * atkSpeed;
				return QString("%1 additional damage").arg(dmgBonus);
			}

			static QVariant UnitFlagsMiscValue(const proto::UnitEntry &unit)
			{
				static const QString flagNames[32] = 
				{
					"UNK_0",				"NOT_ATTACKABLE",	"DISABLE_MOVE",	"PVP_ATTACKABLE",
					"RENAME",				"PREPARATION",		"UNK_6",		"NOT_ATTACKABLE_PVP", 
					"OOC_NOT_ATTACKABLE",	"PASSIVE",			"LOOTING",		"PET_IN_COMBAT",
					"PVP",					"SILENCED",			"UNK_14",		"UNK_15",
					"NO_PL_SPELL_TARGET",	"PACIFIED",			"STUNNED",		"IN_COMBAT", 
					"TAXI_FLIGHT",			"DISARMED",			"CONFUSED",		"FLEEING",
					"PLAYER_CONTROLLED",	"NOT_SELECTABLE",	"SKINNABLE",	"MOUNT",
					"UNK_28",				"UNK_29",			"SHEATHE",		"UNK_31"
				};

				QString buffer;
				for (UInt32 mask = 1, bitIndex = 0; mask != 0; mask <<= 1, bitIndex++)
				{
					if ((unit.unitflags() & mask) != 0)
					{
						if (!buffer.isEmpty()) buffer.append(" | ");
						buffer.append(QString("%1 ").arg(flagNames[bitIndex]));
					}
				}

				return buffer;
			}
		}

		void ObjectEditor::addLootItem(const proto::LootDefinition &def, QTreeWidgetItem *parent)
		{
			const auto *itemEntry = m_application.getProject().items.getById(def.item());
			if (!itemEntry)
				return;

			QTreeWidgetItem *item = new QTreeWidgetItem(parent); 
			item->setText(0, QString("%1 %2").arg(QString::number(def.item()), 5, QLatin1Char('0')).arg(itemEntry->name().c_str()));
			item->setText(1, QString("%1%").arg(def.dropchance()));
			if (def.maxcount() > def.mincount())
			{
				item->setText(2, QString("%1x - %2x").arg(def.mincount()).arg(def.maxcount()));
			}
			else
			{
				item->setText(2, QString("%1x").arg(def.mincount()));
			}

			QColor textColor = QColor(Qt::white);
			switch (itemEntry->quality())
			{
			case 0:
				textColor = QColor(Qt::gray);
				break;
			case 1:
				textColor = QColor(Qt::white);
				break;
			case 2:
				textColor = QColor(Qt::green);
				break;
			case 3:
				textColor = QColor(0, 114, 198);
				break;
			case 4:
				textColor = QColor(Qt::magenta);
				break;
			case 5:
				textColor = QColor(Qt::yellow);
				break;
			default:
				textColor = QColor(Qt::red);
				break;
			}
			item->setTextColor(0, textColor);
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
			auto *unit = m_application.getProject().units.getTemplates().mutable_entry(index);
			m_selected = unit;
			if (!unit)
				return;

			m_ui->lootView->clear();
			if (!unit->unitlootentry())
			{
				m_ui->lootLine->setText("- NO LOOT -");
				m_ui->lootToolButton->setDisabled(true);
				m_ui->lootSimulatorButton->setDisabled(true);
			}
			else
			{
				QIcon groupIcon;
				groupIcon.addFile(QStringLiteral(":/Items.png"), QSize(), QIcon::Normal, QIcon::Off);

				size_t groupIndex = 0;
				const auto *lootEntry = m_application.getProject().unitLoot.getById(unit->unitlootentry());
				if (lootEntry)
				{
					for (const auto &group : lootEntry->groups())
					{
						// Add group
						QTreeWidgetItem *groupItem = new QTreeWidgetItem();
						groupItem->setIcon(0, groupIcon);
						m_ui->lootView->addTopLevelItem(groupItem);

						float totalDropChance = 0.0f;
						for (const auto &def : group.definitions())
						{
							totalDropChance += def.dropchance();
							addLootItem(def, groupItem);
						}

						groupItem->setText(0, QString("Group %1").arg(groupIndex++));
						groupItem->setText(1, QString("%1% Total").arg(totalDropChance));
						groupItem->setText(2, QString("%1 Items").arg(group.definitions_size()));
					}
				}

				m_ui->lootLine->setText(QString("Loot Entry %1").arg(unit->unitlootentry()));
				m_ui->lootToolButton->setDisabled(false);
				m_ui->lootSimulatorButton->setDisabled(false);
			}
			
			// Add unit properties
#define WOWPP_NUM_PROPERTY(name, type, ref, prop, readonly) { \
			auto getBinder = [unit]() -> type { return unit->##prop(); }; \
			auto setBinder = [unit](type value) { unit->set_##prop(value); }; \
			m_properties.push_back(PropertyPtr(new NumericProperty(name, ref(getBinder, setBinder), readonly))); }
#define WOWPP_STR_PROPERTY(name, prop, readonly) { \
			auto getBinder = [unit]() -> String { return unit->##prop(); }; \
			auto setBinder = [unit](const String &value) { unit->set_##prop(value.c_str()); }; \
			m_properties.push_back(PropertyPtr(new StringProperty(name, getBinder, setBinder, readonly))); }
#define WOWPP_MIN_MAX_PROPERTY(name, type, ref, prop, readonly) { \
			auto getMinBinder = [unit]() -> type { return unit->min##prop(); }; \
			auto setMinBinder = [unit](type value) { unit->set_min##prop(value); }; \
			auto getMaxBinder = [unit]() -> type { return unit->max##prop(); }; \
			auto setMaxBinder = [unit](type value) { unit->set_max##prop(value); }; \
			m_properties.push_back(PropertyPtr(new MinMaxProperty(name, ref(getMinBinder, setMinBinder), ref(getMaxBinder, setMaxBinder), readonly))); }

			WOWPP_NUM_PROPERTY("Entry", UInt32, UInt32Ref, id, true);
			WOWPP_STR_PROPERTY("Name", name, false);
			WOWPP_STR_PROPERTY("Subname", subname, false);
			WOWPP_MIN_MAX_PROPERTY("Level", UInt32, UInt32Ref, level, false);
			WOWPP_MIN_MAX_PROPERTY("Health", UInt32, UInt32Ref, levelhealth, false);
			WOWPP_MIN_MAX_PROPERTY("Mana", UInt32, UInt32Ref, levelmana, false);
			WOWPP_NUM_PROPERTY("Damage School", UInt32, UInt32Ref, damageschool, false);
			WOWPP_MIN_MAX_PROPERTY("Melee Damage", float, FloatRef, meleedmg, false);
			WOWPP_NUM_PROPERTY("Melee Attack Power", UInt32, UInt32Ref, attackpower, false);
			WOWPP_NUM_PROPERTY("Melee Attack Time", UInt32, UInt32Ref, meleeattacktime, false);
			WOWPP_MIN_MAX_PROPERTY("Ranged Damage", float, FloatRef, rangeddmg, false);
			WOWPP_NUM_PROPERTY("Ranged Attack Power", UInt32, UInt32Ref, rangedattackpower, false);
			WOWPP_NUM_PROPERTY("Ranged Attack Time", UInt32, UInt32Ref, rangedattacktime, false);
			WOWPP_NUM_PROPERTY("Scale", float, FloatRef, scale, false);
			WOWPP_NUM_PROPERTY("Male Model ID", UInt32, UInt32Ref, malemodel, false);
			WOWPP_NUM_PROPERTY("Female Model ID", UInt32, UInt32Ref, femalemodel, false);
			WOWPP_NUM_PROPERTY("Alliance Faction ID", UInt32, UInt32Ref, alliancefaction, false);
			WOWPP_NUM_PROPERTY("Horde Faction ID", UInt32, UInt32Ref, hordefaction, false);
			WOWPP_NUM_PROPERTY("Family", UInt32, UInt32Ref, family, false);
			WOWPP_NUM_PROPERTY("NPC Flags", UInt32, UInt32Ref, npcflags, false);
			WOWPP_NUM_PROPERTY("Unit Flags", UInt32, UInt32Ref, unitflags, false);
			WOWPP_NUM_PROPERTY("Dynamic Flags", UInt32, UInt32Ref, dynamicflags, false);
			WOWPP_NUM_PROPERTY("Extra Flags", UInt32, UInt32Ref, extraflags, false);
			WOWPP_NUM_PROPERTY("Creature Type Flags", UInt32, UInt32Ref, creaturetypeflags, false);
			WOWPP_NUM_PROPERTY("Walk Speed Factor", float, FloatRef, walkspeed, false);
			WOWPP_NUM_PROPERTY("Run Speed Factor", float, FloatRef, runspeed, false);
			WOWPP_NUM_PROPERTY("Unit Class", UInt32, UInt32Ref, unitclass, false);
			WOWPP_NUM_PROPERTY("Rank", UInt32, UInt32Ref, rank, false);
			WOWPP_NUM_PROPERTY("Armor", UInt32, UInt32Ref, armor, false);
			/*WOWPP_NUM_PROPERTY("Holy Resistance", UInt32, UInt32Ref, armor, false);
			WOWPP_NUM_PROPERTY("Fire Resistance", UInt32, UInt32Ref, armor, false);
			WOWPP_NUM_PROPERTY("Nature Resistance", UInt32, UInt32Ref, armor, false);
			WOWPP_NUM_PROPERTY("Frost Resistance", UInt32, UInt32Ref, armor, false);
			WOWPP_NUM_PROPERTY("Shadow Resistance", UInt32, UInt32Ref, armor, false);
			WOWPP_NUM_PROPERTY("Arcane Resistance", UInt32, UInt32Ref, armor, false);*/
			WOWPP_MIN_MAX_PROPERTY("Loot Gold", UInt32, UInt32Ref, lootgold, false);
			WOWPP_MIN_MAX_PROPERTY("Experience", UInt32, UInt32Ref, levelxp, false);

			// Update the view 
			m_viewModel->layoutChanged();

			m_ui->unitTriggerWidget->clear();
			for (const auto &triggerId : unit->triggers())
			{
				const auto *triggerEntry = m_application.getProject().triggers.getById(triggerId);
				if (triggerEntry)
				{
					m_ui->unitTriggerWidget->addItem(
						QString(triggerEntry->name().c_str()));
				}
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
			auto *spell = m_application.getProject().spells.getTemplates().mutable_entry(index);
			if (!spell)
				return;

			m_ui->spellIdField->setText(QString::number(spell->id()));
			m_ui->spellNameField->setText(spell->name().c_str());

			// Determine the cast time of this spell
			Int64 castTime = spell->casttime();

			m_ui->castTimeField->setText(QString::number(castTime));
			m_ui->cooldownField->setText(QString::number(spell->cooldown()));
			m_ui->resourceField->setCurrentIndex(spell->powertype());
			m_ui->costField->setText(QString::number(spell->cost()));

			// Attributes
			for (size_t i = 1; i <= 32; ++i)
			{
				QCheckBox *box = m_ui->attributeTabs->findChild<QCheckBox*>(QString("attr_%1_box").arg(i));
				if (box)
				{
					const bool hasAttribute = (spell->attributes(0) & (1 << (i - 1))) != 0;
					box->setChecked(hasAttribute);
				}
			}

			// Attributes Ex 1 - 6
			for (size_t j = 1; j < spell->attributes_size(); ++j)
			{
				for (size_t i = 1; i <= 32; ++i)
				{
					QCheckBox *box = m_ui->attributeTabs->findChild<QCheckBox*>(QString("attr_ex_%1_box_%2").arg(i).arg(j));
					if (box)
					{
						const bool hasAttribute = (spell->attributes(j) & (1 << (i - 1))) != 0;
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
					if (i < spell->effects_size())
					{
						effectName = "TODO"; //game::constant_literal::spellEffectNames.getName(spell->effects(i).type()).c_str();
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
			auto *item = m_application.getProject().items.getTemplates().mutable_entry(index);
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
					auto it = std::find_if(m_selected->triggers().begin(), m_selected->triggers().end(), [&newTrigger](const UInt32 &trigger) -> bool
					{
						return (trigger == newTrigger->id());
					});

					if (it == m_selected->triggers().end())
					{
						m_selected->mutable_triggers()->Add(newTrigger->id());
						m_ui->unitTriggerWidget->addItem(
							QString(newTrigger->name().c_str()));

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
				if (row < 0 || row >= m_selected->triggers_size())
					return;

				if (m_selected->triggers_size() > 1 &&
					row != m_selected->triggers_size() - 1)
				{
					m_selected->mutable_triggers()->SwapElements(row, m_selected->triggers_size() - 1);
				}
				m_selected->mutable_triggers()->RemoveLast();

				auto *taken = m_ui->unitTriggerWidget->takeItem(row);
				delete taken;

				m_application.markAsChanged();
			}
		}

		void ObjectEditor::on_lootSimulatorButton_clicked()
		{
			if (!m_selected)
				return;

			if (!m_selected->unitlootentry())
				return;

			const auto *loot = m_application.getProject().unitLoot.getById(m_selected->unitlootentry());
			if (!loot)
				return;

			LootDialog dialog(m_application.getProject(), *loot);
			auto result = dialog.exec();
		}

		void ObjectEditor::on_actionImport_triggered()
		{
			// Counter variables used by the import method
			UInt32 lastEntry = 0;
			UInt32 lastGroup = 0;
			UInt32 groupIndex = 0;

			// Prepare the import task
			ImportTask task;
			task.countQuery = "SELECT COUNT(*) FROM `wowpp_creature_loot_template` WHERE `lootcondition` = 0 AND `active` != 0;";
			task.selectQuery = "SELECT `entry`, `item`, `ChanceOrQuestChance`, `groupid`, `mincountOrRef`, `maxcount` FROM `wowpp_creature_loot_template` WHERE `lootcondition` = 0  AND `active` != 0 ORDER BY `entry`, `groupid`;";
			task.beforeImport = [this]() {
				// Remove old unit loot
				for (int i = 0; i < m_application.getProject().units.getTemplates().entry_size(); ++i)
				{
					auto *unit = m_application.getProject().units.getTemplates().mutable_entry(i);
					unit->set_unitlootentry(0);
				}
				m_application.getProject().unitLoot.clear();
			};
			task.onImport = [this, &lastEntry, &lastGroup, &groupIndex](wowpp::MySQL::Row &row) -> bool {
				UInt32 entry = 0, itemId = 0, groupId = 0, minCount = 0, maxCount = 0;
				float dropChance = 0.0f;
				row.getField(0, entry);
				row.getField(1, itemId);
				row.getField(2, dropChance);
				row.getField(3, groupId);
				row.getField(4, minCount);
				row.getField(5, maxCount);

				// Find referenced item
				const auto *itemEntry = m_application.getProject().items.getById(itemId);
				if (!itemEntry)
				{
					ELOG("Could not find referenced item " << itemId << " (referenced in creature loot entry " << entry << " - group " << groupId << ")");
					return false;
				}

				// Create a new loot entry
				bool created = false;
				if (entry > lastEntry)
				{
					auto *added = m_application.getProject().unitLoot.add(entry);

					lastEntry = entry;
					lastGroup = groupId;
					groupIndex = 0;
					created = true;
				}

				auto *lootEntry = m_application.getProject().unitLoot.getById(entry);
				if (!lootEntry)
				{
					// Error
					ELOG("Loot entry " << entry << " found, but no creature to assign found");
					return false;
				}

				if (created)
				{
					auto *unitEntry = m_application.getProject().units.getById(entry);
					if (!unitEntry)
					{
						WLOG("No unit with entry " << entry << " found - creature loot template will not be assigned!");
					}
					else
					{
						unitEntry->set_unitlootentry(lootEntry->id());
					}
				}

				// If there are no loot groups yet, create a new one
				if (lootEntry->groups().empty() || groupId > lastGroup)
				{
					auto *addedGroup = lootEntry->add_groups();
					if (groupId > lastGroup)
					{
						lastGroup = groupId;
						groupIndex++;
					}
				}

				if (lootEntry->groups().empty())
				{
					ELOG("Error retrieving loot group");
					return false;
				}

				auto *group = lootEntry->mutable_groups(groupIndex);
				auto *def = group->add_definitions();
				def->set_item(itemEntry->id());
				def->set_mincount(minCount);
				def->set_maxcount(maxCount);
				def->set_dropchance(dropChance);
				def->set_isactive(true);

				return true;
			};
			
			// Do import job
			ImportDialog dialog(m_application, std::move(task));
			auto result = dialog.exec();
		}
		void ObjectEditor::on_actionImport_Vendors_triggered()
		{
			UInt32 lastEntry = 0;

			ImportTask task;
			task.countQuery = "SELECT COUNT(*) FROM `wowpp_npc_vendor`;";
			task.selectQuery = "SELECT `entry`, `item`, `maxcount`, `incrtime`, `ExtendedCost` FROM `wowpp_npc_vendor` ORDER BY `entry`, `item`;";
			task.beforeImport = [this]() {
				for (int i = 0; i < m_application.getProject().units.getTemplates().entry_size(); ++i)
				{
					auto *unit = m_application.getProject().units.getTemplates().mutable_entry(i);
					unit->set_vendorentry(0);
				}
				m_application.getProject().vendors.clear();
			};
			task.onImport = [this, &lastEntry](wowpp::MySQL::Row &row) -> bool {
				UInt32 entry = 0, itemId = 0, maxCount = 0, incrTime = 0, extendedCost = 0;
				row.getField(0, entry);
				row.getField(1, itemId);
				row.getField(2, maxCount);
				row.getField(3, incrTime);
				row.getField(4, extendedCost);

				// Find referenced item
				const auto *itemEntry = m_application.getProject().items.getById(itemId);
				if (!itemEntry)
				{
					ELOG("Could not find referenced item " << itemId << " (referenced in vendor entry " << entry << ")");
					return false;
				}

				// Create a new loot entry
				bool created = false;
				if (entry > lastEntry)
				{
					auto *added = m_application.getProject().vendors.add(entry);

					lastEntry = entry;
					created = true;
				}

				auto *vendorEntry = m_application.getProject().vendors.getById(entry);
				if (!vendorEntry)
				{
					// Error
					ELOG("Vendor entry " << entry << " not found!");
					return false;
				}

				// Add items
				auto *addedItem = vendorEntry->add_items();
				addedItem->set_item(itemId);
				addedItem->set_interval(incrTime);
				addedItem->set_isactive(true);
				addedItem->set_maxcount(maxCount);
				addedItem->set_extendedcost(extendedCost);

				if (created)
				{
					auto *unitEntry = m_application.getProject().units.getById(entry);
					if (!unitEntry)
					{
						WLOG("No unit with entry " << entry << " found - vendor will not be assigned!");
					}
					else
					{
						unitEntry->set_vendorentry(vendorEntry->id());
					}
				}

				return true;
			};

			// Do import job
			ImportDialog dialog(m_application, std::move(task));
			auto result = dialog.exec();
		}
		void ObjectEditor::on_actionImport_Trainers_triggered()
		{

		}
	}
}
