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
#include "object_editor.h"
#include "main_window.h"
#include "editor_application.h"
#include "string_editor.h"
#include "numeric_editor.h"
#include "min_max_editor.h"
#include "ui_object_editor.h"
#include "loot_dialog.h"
#include "choose_trigger_dialog.h"
#include "creature_spell_dialog.h"
#include "choose_variables_dialog.h"
#include "spell_effect_dialog.h"
#include "game/defines.h"
#include "import_dialog.h"
#include <QRegExp>

namespace wowpp
{
	namespace editor
	{
		ObjectEditor::ObjectEditor(EditorApplication &app)
			: QMainWindow()
			, m_application(app)
			, m_ui(new Ui::ObjectEditor())
			, m_selectedUnit(nullptr)
			, m_selectedSpell(nullptr)
			, m_selectedSkill(nullptr)
			, m_selectedQuest(nullptr)
			, m_selectedObject(nullptr)
			, m_selectedItem(nullptr)
			, m_selectionChanging(false)
		{
			m_ui->setupUi(this);

			// Setup view model
			m_viewModel = new PropertyViewModel(m_properties, nullptr);
			m_objectViewModel = new PropertyViewModel(m_objectProperties, nullptr);
			m_itemViewModel = new PropertyViewModel(m_itemProperties, nullptr);
			m_ui->unitPropertyWidget->setModel(m_viewModel);
			m_ui->objectPropertyWidget->setModel(m_objectViewModel);
			m_ui->lootView->header()->setVisible(true);
			m_ui->objectLootView->header()->setVisible(true);
			m_ui->itemLootView->header()->setVisible(true);

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

			// Automatically deleted since it's a QObject
			m_questFilter = new QSortFilterProxyModel;
			m_questFilter->setSourceModel(app.getQuestListModel());
			m_ui->questsListView->setModel(m_questFilter);

			// Automatically deleted since it's a QObject
			m_objectFilter = new QSortFilterProxyModel;
			m_objectFilter->setSourceModel(app.getObjectListModel());
			m_ui->objectsListView->setModel(m_objectFilter);

			m_skillFilter = new QSortFilterProxyModel;
			m_skillFilter->setSourceModel(app.getSkillListModel());
			m_ui->skillsListView->setModel(m_skillFilter);

			// Map selection box
			m_ui->spellTeleportMapBox->setModel(app.getMapListModel());

			// Connect checkbox signals
			for (UInt32 i = 1; i <= game::race::Max; ++i)
			{
				QCheckBox *box = m_ui->spellRaceBox->findChild<QCheckBox*>(QString("race_%1").arg(i));
				if (box)
				{
					connect(box,
						SIGNAL(stateChanged(int)),
						this, SLOT(onRaceClassChanged(int)));
				}
			}
			for (UInt32 i = 1; i <= game::char_class::Max; ++i)
			{
				QCheckBox *box = m_ui->spellClassBox->findChild<QCheckBox*>(QString("class_%1").arg(i));
				if (box)
				{
					connect(box,
						SIGNAL(stateChanged(int)),
						this, SLOT(onRaceClassChanged(int)));
				}
			}
			for (UInt32 i = 1; i <= 30; ++i)
			{
				QCheckBox *box = m_ui->mechanicImmunityBox->findChild<QCheckBox*>(QString("mech_flag_%1").arg(i));
				if (box)
				{
					connect(box,
						SIGNAL(stateChanged(int)),
						this, SLOT(onMechanicImmunityChanged(int)));
				}
			}
			for (UInt32 i = 1; i <= game::spell_school::End; ++i)
			{
				QCheckBox *box = m_ui->schoolImmunityBox->findChild<QCheckBox*>(QString("school_flag_%1").arg(i));
				if (box)
				{
					connect(box,
						SIGNAL(stateChanged(int)),
						this, SLOT(onSchoolImmunityChanged(int)));
				}
			}

			connect(m_ui->unitsListView->selectionModel(),
				SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
				this, SLOT(onUnitSelectionChanged(QItemSelection, QItemSelection)));
			connect(m_ui->spellsListView->selectionModel(),
				SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
				this, SLOT(onSpellSelectionChanged(QItemSelection, QItemSelection)));
			connect(m_ui->skillsListView->selectionModel(),
				SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
				this, SLOT(onSkillSelectionChanged(QItemSelection, QItemSelection)));
			connect(m_ui->itemsListView->selectionModel(),
				SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
				this, SLOT(onItemSelectionChanged(QItemSelection, QItemSelection)));
			connect(m_ui->questsListView->selectionModel(),
				SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
				this, SLOT(onQuestSelectionChanged(QItemSelection, QItemSelection)));
			connect(m_ui->objectsListView->selectionModel(),
				SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
				this, SLOT(onObjectSelectionChanged(QItemSelection, QItemSelection)));

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

		void ObjectEditor::on_skillFilter_editingFinished()
		{
			QRegExp::PatternSyntax syntax = QRegExp::RegExp;
			Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;

			QRegExp regExp(m_ui->skillFilter->text(), caseSensitivity, syntax);
			m_skillFilter->setFilterRegExp(regExp);
		}

		void ObjectEditor::on_itemFilter_editingFinished()
		{
			QRegExp::PatternSyntax syntax = QRegExp::RegExp;
			Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;

			QRegExp regExp(m_ui->itemFilter->text(), caseSensitivity, syntax);
			m_itemFilter->setFilterRegExp(regExp);
		}
		void ObjectEditor::on_questFilter_editingFinished()
		{
			QRegExp::PatternSyntax syntax = QRegExp::RegExp;
			Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;

			QRegExp regExp(m_ui->questFilter->text(), caseSensitivity, syntax);
			m_questFilter->setFilterRegExp(regExp);
		}
		void ObjectEditor::on_objectFilter_editingFinished()
		{
			QRegExp::PatternSyntax syntax = QRegExp::RegExp;
			Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;

			QRegExp regExp(m_ui->objectFilter->text(), caseSensitivity, syntax);
			m_objectFilter->setFilterRegExp(regExp);
		}
#if 0
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
#endif

		namespace
		{
			static void applyItemColorToWidget(QTreeWidgetItem *item, UInt32 column, UInt32 itemQuality)
			{
				QColor textColor = QColor(Qt::white);
				switch (itemQuality)
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
				item->setTextColor(column, textColor);
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

			applyItemColorToWidget(item, 0, itemEntry->quality());
			if (def.conditiontype())
			{
				item->setText(3, QString("COND %1: VAL1=%2 VAL2=%3").arg(def.conditiontype()).arg(def.conditionvala()).arg(def.conditionvalb()));
			}
		}


		namespace
		{
			static void applySpellToTreeItem(QTreeWidgetItem &item, const proto::UnitSpellEntry &entry, const proto::SpellEntry &spell)
			{
				static QString spellCastTargetNames[] = {
					"SELF",
					"CURRENT_TARGET"
				};

				item.setText(0, QString("%1 %2").arg(spell.id()).arg(spell.name().c_str()));
				item.setText(1, QString("%1").arg(entry.priority()));
				item.setText(2, QString("%1").arg(entry.repeated() ? "Yes" : "No"));
				item.setText(3, spellCastTargetNames[entry.target()]);
				if (entry.mininitialcooldown() != entry.maxinitialcooldown())
				{
					item.setText(4, QString("%1 - %2").arg(entry.mininitialcooldown()).arg(entry.maxinitialcooldown()));
				}
				else
				{
					item.setText(4, QString("%1").arg(entry.mininitialcooldown()));
				}
				if (entry.mincooldown() != entry.maxcooldown())
				{
					item.setText(5, QString("%1 - %2").arg(entry.mincooldown()).arg(entry.maxcooldown()));
				}
				else
				{
					item.setText(5, QString("%1").arg(entry.mincooldown()));
				}
				if (entry.minrange() != entry.maxrange())
				{
					item.setText(6, QString("%1 - %2").arg(entry.minrange()).arg(entry.maxrange()));
				}
				else
				{
					item.setText(6, QString("%1").arg(entry.minrange()));
				}
				item.setText(7, QString("%1%").arg(entry.probability()));
			}
		}

		void ObjectEditor::addSpellEntry(const wowpp::proto::UnitSpellEntry &creatureSpell)
		{
			const auto *spellEntry = m_application.getProject().spells.getById(creatureSpell.spellid());
			if (!spellEntry)
			{
				return;
			}

			QTreeWidgetItem *item = new QTreeWidgetItem(m_ui->treeWidget);
			applySpellToTreeItem(*item, creatureSpell, *spellEntry);
		}

		void ObjectEditor::showEffectDialog(const proto::SpellEffect & effect)
		{
			SpellEffectDialog dialog(m_application, effect);
			dialog.exec();
		}

		void ObjectEditor::onUnitSelectionChanged(const QItemSelection& selection, const QItemSelection& old)
		{
			// Get the selected unit
			if (selection.isEmpty())
			{
				m_selectedUnit = nullptr;
				return;
			}

			QItemSelection source = m_unitFilter->mapSelectionToSource(selection);
			if (source.isEmpty())
			{
				m_selectedUnit = nullptr;
				return;
			}

			int index = source.indexes().first().row();
			if (index < 0)
			{
				m_selectedUnit = nullptr;
				return;
			}

			// Update properties
			m_properties.clear();

			// Get unit entry
			auto *unit = m_application.getProject().units.getTemplates().mutable_entry(index);
			m_selectedUnit = unit;
			if (!unit)
				return;

			m_selectionChanging = true;

			m_ui->unitQuestWidget->clear();
			for (const auto &questid : unit->quests())
			{
				const auto *quest = m_application.getProject().quests.getById(questid);
				if (quest)
				{
					m_ui->unitQuestWidget->addItem(
						QString("%1 %2 [%3]").arg(questid, 5, 10, QLatin1Char('0')).arg(quest->name().c_str()).arg(static_cast<Int32>(quest->questlevel())));
				}
			}

			m_ui->trainerWidget->clear();
			if (unit->trainerentry())
			{
				const auto *trainer = m_application.getProject().trainers.getById(unit->trainerentry());
				if (trainer)
				{
					for (auto &entry : trainer->spells())
					{
						const auto *spell = m_application.getProject().spells.getById(entry.spell());
						if (!spell)
						{
							continue;
						}

						const auto *skill = (entry.reqskill() ? m_application.getProject().skills.getById(entry.reqskill()) : nullptr);

						auto *item = new QTreeWidgetItem(m_ui->trainerWidget);
						item->setText(0, QString("%1 %2").arg(spell->id(), 5, 10, QLatin1Char('0')).arg(spell->name().c_str()));
						item->setText(1, QString("%1").arg(entry.spellcost()));
						item->setText(2, QString("%1").arg(skill ? skill->name().c_str() : "-"));
						item->setText(3, QString("%1").arg(entry.reqskillval()));
						item->setText(4, QString("%1").arg(entry.reqlevel()));
					}
				}
			}

			m_ui->unitEndQuestWidget->clear();
			for (const auto &questid : unit->end_quests())
			{
				const auto *quest = m_application.getProject().quests.getById(questid);
				if (quest)
				{
					m_ui->unitEndQuestWidget->addItem(
						QString("%1 %2 [%3]").arg(questid, 5, 10, QLatin1Char('0')).arg(quest->name().c_str()).arg(static_cast<Int32>(quest->questlevel())));
				}
			}

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

			m_ui->skinLootView->clear();
			if (!unit->skinninglootentry())
			{
				m_ui->skinLootLine->setText("- NO LOOT -");
				m_ui->skinLootToolButton->setDisabled(true);
				m_ui->skinLootSimulatorButton->setDisabled(true);
			}
			else
			{
				QIcon groupIcon;
				groupIcon.addFile(QStringLiteral(":/Items.png"), QSize(), QIcon::Normal, QIcon::Off);

				size_t groupIndex = 0;
				const auto *lootEntry = m_application.getProject().skinningLoot.getById(unit->skinninglootentry());
				if (lootEntry)
				{
					for (const auto &group : lootEntry->groups())
					{
						// Add group
						QTreeWidgetItem *groupItem = new QTreeWidgetItem();
						groupItem->setIcon(0, groupIcon);
						m_ui->skinLootView->addTopLevelItem(groupItem);

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

				m_ui->skinLootLine->setText(QString("Loot Entry %1").arg(unit->skinninglootentry()));
				m_ui->skinLootToolButton->setDisabled(false);
				m_ui->skinLootSimulatorButton->setDisabled(false);
			}

			// Add unit properties
#define WOWPP_NUM_PROPERTY(name, type, ref, prop, readonly) { \
			auto getBinder = [unit]() -> type { return unit->prop(); }; \
			auto setBinder = [unit](type value) { unit->set_##prop(value); }; \
			m_properties.push_back(PropertyPtr(new NumericProperty(name, ref(getBinder, setBinder), readonly))); }
#define WOWPP_INDEXED_NUM_PROPERTY(name, type, ref, prop, index, readonly) { \
			auto getBinder = [unit]() -> type { return unit->prop(index); }; \
			auto setBinder = [unit](type value) { unit->set_##prop(index, value); }; \
			m_properties.push_back(PropertyPtr(new NumericProperty(name, ref(getBinder, setBinder), readonly))); }
#define WOWPP_STR_PROPERTY(name, prop, readonly) { \
			auto getBinder = [unit]() -> String { return unit->prop(); }; \
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
			WOWPP_INDEXED_NUM_PROPERTY("Holy Resistance", UInt32, UInt32Ref, resistances, 0, false);
			WOWPP_INDEXED_NUM_PROPERTY("Fire Resistance", UInt32, UInt32Ref, resistances, 1, false);
			WOWPP_INDEXED_NUM_PROPERTY("Nature Resistance", UInt32, UInt32Ref, resistances, 2, false);
			WOWPP_INDEXED_NUM_PROPERTY("Frost Resistance", UInt32, UInt32Ref, resistances, 3, false);
			WOWPP_INDEXED_NUM_PROPERTY("Shadow Resistance", UInt32, UInt32Ref, resistances, 4, false);
			WOWPP_INDEXED_NUM_PROPERTY("Arcane Resistance", UInt32, UInt32Ref, resistances, 5, false);
			WOWPP_MIN_MAX_PROPERTY("Loot Gold", UInt32, UInt32Ref, lootgold, false);
			WOWPP_MIN_MAX_PROPERTY("Experience", UInt32, UInt32Ref, levelxp, false);
			//WOWPP_NUM_PROPERTY("Mechanic Immunity", UInt32, UInt32Ref, mechanicimmunity, false);
			//WOWPP_NUM_PROPERTY("School Immunity", UInt32, UInt32Ref, schoolimmunity, false);
			WOWPP_NUM_PROPERTY("Kill Credit", UInt32, UInt32Ref, killcredit, false);

#undef WOWPP_MIN_MAX_PROPERTY
#undef WOWPP_STR_PROPERTY
#undef WOWPP_NUM_PROPERTY

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

			m_ui->unitVariableWidget->clear();
			for (const auto &variableId : unit->variables())
			{
				const auto *variableEntry = m_application.getProject().variables.getById(variableId);
				if (variableEntry)
				{
					m_ui->unitVariableWidget->addItem(
						QString(variableEntry->name().c_str()));
				}
			}

			m_ui->treeWidget->clear();
			for (const auto &creatureSpell : unit->creaturespells())
			{
				addSpellEntry(creatureSpell);
			}

			for (size_t i = 0; i < game::spell_school::End; ++i)
			{
				UInt32 flag = 1 << i;

				QCheckBox *box = m_ui->schoolImmunityBox->findChild<QCheckBox*>(QString("school_flag_%1").arg(i + 1));
				if (box)
				{
					const bool hasAttribute = (unit->schoolimmunity() & flag) != 0;
					box->setChecked(hasAttribute);
				}
			}
			for (size_t i = 0; i < 30; ++i)
			{
				UInt32 flag = 1 << i;

				QCheckBox *box = m_ui->mechanicImmunityBox->findChild<QCheckBox*>(QString("mech_flag_%1").arg(i + 1));
				if (box)
				{
					const bool hasAttribute = (unit->mechanicimmunity() & flag) != 0;
					box->setChecked(hasAttribute);
				}
			}

			m_selectionChanging = false;
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

		void ObjectEditor::on_objectPropertyWidget_doubleClicked(QModelIndex index)
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
			Property &prop = *m_objectProperties.at(row);
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
					m_objectViewModel->layoutChanged();
				}
			}
		}

		void ObjectEditor::on_treeWidget_doubleClicked(QModelIndex index)
		{
			// Find the selected entry
			if (!index.isValid() || !m_selectedUnit)
			{
				return;
			}

			auto *entry = m_selectedUnit->mutable_creaturespells(index.row());

			CreatureSpellDialog dialog(m_application, entry);
			auto result = dialog.exec();
			if (result == QDialog::Accepted)
			{
				if (!dialog.getSelectedSpell())
				{
					return;
				}

				entry->set_spellid(dialog.getSelectedSpell()->id());
				entry->set_priority(dialog.getPriority());
				entry->set_mincooldown(dialog.getMinCooldown());
				entry->set_maxcooldown(dialog.getMaxCooldown());
				entry->set_mininitialcooldown(dialog.getMinInitialCooldown());
				entry->set_maxinitialcooldown(dialog.getMaxInitialCooldown());
				entry->set_target(dialog.getTarget());
				entry->set_repeated(dialog.getRepeated());
				entry->set_probability(dialog.getProbability());

				// Update UI
				QTreeWidgetItem *item = m_ui->treeWidget->currentItem();
				applySpellToTreeItem(*item, *entry, *dialog.getSelectedSpell());
			}
		}

		void ObjectEditor::onSpellSelectionChanged(const QItemSelection& selection, const QItemSelection& old)
		{
			// Get the selected unit
			m_selectedSpell = nullptr;
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

			m_selectionChanging = true;
			m_selectedSpell = spell;

			m_ui->spellIdField->setText(QString::number(spell->id()));
			m_ui->spellNameField->setText(spell->name().c_str());
			m_ui->spellMechanicField->setText(QString::number(spell->mechanic()));
			m_ui->spellRankField->setText(QString::number(spell->rank()));
			m_ui->spellPositiveBox->setChecked(spell->positive() != 0);

			// Determine the cast time of this spell
			Int64 castTime = spell->casttime();

			m_ui->castTimeField->setText(QString::number(castTime));
			m_ui->cooldownField->setText(QString::number(spell->cooldown()));
			m_ui->resourceField->setCurrentIndex(spell->powertype());
			m_ui->costField->setText(QString::number(spell->cost()));
			m_ui->lineEdit_6->setText(QString::number(spell->family()));
			m_ui->lineEdit_7->setText(QString("0x") + QString::number(spell->familyflags(), 16).toUpper().rightJustified(16, QLatin1Char('0')));
			m_ui->spellSkillField->setText(QString::number(spell->skill()));

			const bool hasBaseId = (spell->id() != spell->baseid());
			m_ui->txtBaseSpell->setText(QString("%1").arg(spell->baseid()));
			m_ui->btnBaseSpell->setEnabled(hasBaseId);

			// Interrupt flags
			for (size_t i = 0; i < 6; ++i)
			{
				UInt32 flag = 1 << i;

				QCheckBox *box = m_ui->spellBaseBox_2->findChild<QCheckBox*>(QString("int_flag_%1").arg(flag));
				if (box)
				{
					const bool hasAttribute = (spell->interruptflags() & flag) != 0;
					box->setChecked(hasAttribute);
				}
			}

			// Aura interrupt flags
			for (size_t i = 0; i < 32; ++i)
			{
				UInt32 flag = 1 << i;

				QCheckBox *box = m_ui->spellBaseBox_3->findChild<QCheckBox*>(QString("aura_int_flag_%1").arg(flag));
				if (box)
				{
					const bool hasAttribute = (spell->aurainterruptflags() & flag) != 0;
					box->setChecked(hasAttribute);
				}
			}

			// Channel interrupt flags
			for (size_t i = 0; i < 32; ++i)
			{
				UInt32 flag = 1 << i;

				QCheckBox *box = m_ui->spellBaseBox_4->findChild<QCheckBox*>(QString("channel_int_flag_%1").arg(flag));
				if (box)
				{
					const bool hasAttribute = (spell->channelinterruptflags() & flag) != 0;
					box->setChecked(hasAttribute);
				}
			}

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

			UInt32 mapIndex = 0;
			for (const auto &map : m_application.getProject().maps.getTemplates().entry())
			{
				if (map.id() == spell->targetmap())
				{
					break;
				}

				mapIndex++;
			}

			m_ui->spellTeleportMapBox->setCurrentIndex(mapIndex);
			m_ui->spellTeleportXBox->setValue(spell->targetx());
			m_ui->spellTeleportYBox->setValue(spell->targety());
			m_ui->spellTeleportZBox->setValue(spell->targetz());
			m_ui->spellTeleportOBox->setValue(spell->targeto());

			// Add linked spells
			m_ui->spellLinkedSpellList->clear();
			for (const auto &spellId : spell->additionalspells())
			{
				const auto *linkedSpell = m_application.getProject().spells.getById(spellId);
				if (linkedSpell)
				{
					m_ui->spellLinkedSpellList->addItem(QString("%1 - %2").arg(spellId).arg(linkedSpell->name().c_str()));
				}
			}

			// Add spell reagents
			m_ui->spellReagentList->clear();
			for (const auto &reagent : spell->reagents())
			{
				const auto *reagentItem = m_application.getProject().items.getById(reagent.item());
				if (reagentItem)
				{
					m_ui->spellReagentList->addItem(QString("%1x %2 [%3]").arg(reagent.count()).arg(reagentItem->name().c_str()).arg(reagent.item()));
				}
			}

			// Race list
			for (UInt32 i = 1; i <= game::race::Max; ++i)
			{
				QCheckBox *box = m_ui->spellRaceBox->findChild<QCheckBox*>(QString("race_%1").arg(i));
				if (box)
				{
					const bool hasAttribute = spell->racemask() == 0 || (spell->racemask() & (1 << (i - 1))) != 0;
					box->setChecked(hasAttribute);
				}
			}

			// Class list
			for (UInt32 i = 1; i <= game::char_class::Max; ++i)
			{
				QCheckBox *box = m_ui->spellClassBox->findChild<QCheckBox*>(QString("class_%1").arg(i));
				if (box)
				{
					const bool hasAttribute = spell->classmask() == 0 || (spell->classmask() & (1 << (i - 1))) != 0;
					box->setChecked(hasAttribute);
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
						button->setEnabled(true);
						const Int32 comboPoints = 0;

						Int32 level = 1;
						if (level > spell->maxlevel() && spell->maxlevel() > 0)
						{
							level = spell->maxlevel();
						}
						else if (level < spell->baselevel())
						{
							level = spell->baselevel();
						}
						level -= spell->spelllevel();

						QString baseName = game::constant_literal::spellEffectNames.getName(
							static_cast<game::SpellEffect>(spell->effects(i).type())).c_str();
						if (spell->effects(i).aura() != 0)
						{
							baseName = QString("%1: %2").arg(baseName).arg(
								game::constant_literal::auraTypeNames.getName(
									static_cast<game::AuraType>(spell->effects(i).aura())).c_str());
						}

						// Calculate the damage done
						const float basePointsPerLevel = spell->effects(i).pointsperlevel();
						const float randomPointsPerLevel = spell->effects(i).diceperlevel();
						const Int32 basePoints = spell->effects(i).basepoints() + level * basePointsPerLevel;
						const Int32 randomPoints = spell->effects(i).diesides() + level * randomPointsPerLevel;
						const Int32 comboDamage = spell->effects(i).pointspercombopoint() * comboPoints;
						if (spell->effects(i).basedice() >= randomPoints)
						{
							effectName = QString("%1 (%2)").arg(baseName).arg(basePoints + spell->effects(i).basedice() + comboDamage);
						}
						else
						{
							effectName = QString("%1 (%2 - %3)").arg(baseName).arg(
									basePoints + spell->effects(i).basedice() + comboDamage).arg(basePoints + randomPoints + comboDamage);
						}
					}
					else
					{
						button->setEnabled(false);
					}

					button->setText(effectName);
				}
			}

			m_selectionChanging = false;
		}

		void ObjectEditor::onSkillSelectionChanged(const QItemSelection & selection, const QItemSelection & old)
		{
			// Get the selected unit
			m_selectedSkill = nullptr;
			if (selection.isEmpty())
				return;

			QItemSelection source = m_skillFilter->mapSelectionToSource(selection);
			if (source.isEmpty())
				return;

			int index = source.indexes().first().row();
			if (index < 0)
			{
				return;
			}

			// Get skill entry
			auto *skill = m_application.getProject().skills.getTemplates().mutable_entry(index);
			if (!skill)
				return;

			m_selectedSkill = skill;

			m_ui->skillIdField->setText(QString::number(skill->id()));
			m_ui->skillNameField->setText(skill->name().c_str());

			// Add linked spells
			m_ui->skillLearnedSpellList->clear();
			for (const auto &spellId : skill->spells())
			{
				const auto *linkedSpell = m_application.getProject().spells.getById(spellId);
				if (linkedSpell)
				{
					m_ui->skillLearnedSpellList->addItem(QString("%1 - %2").arg(spellId).arg(linkedSpell->name().c_str()));
				}
			}
		}

		void ObjectEditor::onItemSelectionChanged(const QItemSelection& selection, const QItemSelection& old)
		{
			// Get the selected unit
			m_selectedItem = nullptr;
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

			m_selectedItem = item;

			m_ui->itemQuestWidget->clear();
			/*for (const auto &questid : item->quests())
			{
				const auto *quest = m_application.getProject().quests.getById(questid);
				if (quest)
				{
					m_ui->objectQuestWidget->addItem(
						QString("%1 %2 [%3]").arg(questid, 5, 10, QLatin1Char('0')).arg(quest->name().c_str()).arg(static_cast<Int32>(quest->questlevel())));
				}
			}*/

			m_ui->itemLootView->clear();
			if (!item->lootentry())
			{
				m_ui->itemLootLine->setText("- NO LOOT -");
				m_ui->itemLootToolButton->setDisabled(true);
				m_ui->itemLootSimulatorButton->setDisabled(true);
			}
			else
			{
				QIcon groupIcon;
				groupIcon.addFile(QStringLiteral(":/Items.png"), QSize(), QIcon::Normal, QIcon::Off);

				size_t groupIndex = 0;
				const auto *lootEntry = m_application.getProject().itemLoot.getById(item->lootentry());
				if (lootEntry)
				{
					for (const auto &group : lootEntry->groups())
					{
						// Add group
						QTreeWidgetItem *groupItem = new QTreeWidgetItem();
						groupItem->setIcon(0, groupIcon);
						m_ui->itemLootView->addTopLevelItem(groupItem);

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

				m_ui->itemLootLine->setText(QString("Loot Entry %1").arg(item->lootentry()));
				m_ui->itemLootToolButton->setDisabled(false);
				m_ui->itemLootSimulatorButton->setDisabled(false);
			}
		}

		void ObjectEditor::onQuestSelectionChanged(const QItemSelection & selection, const QItemSelection & old)
		{
			m_selectedQuest = nullptr;

			// Get the selected unit
			if (selection.isEmpty())
				return;

			QItemSelection source = m_questFilter->mapSelectionToSource(selection);
			if (source.isEmpty())
				return;

			int index = source.indexes().first().row();
			if (index < 0)
			{
				return;
			}

			// Get quest entry
			auto *quest = m_application.getProject().quests.getTemplates().mutable_entry(index);
			if (!quest)
				return;

			m_selectedQuest = quest;

			m_ui->questIdField->setText(QString("%1").arg(m_selectedQuest->id()));
			m_ui->questNameField->setText(m_selectedQuest->name().c_str());
			m_ui->questMethodField->setText(QString("%1").arg(m_selectedQuest->method()));
			m_ui->questMinLevelField->setText(QString("%1").arg(m_selectedQuest->minlevel()));
			m_ui->questLevelField->setText(QString("%1").arg(m_selectedQuest->questlevel()));
			m_ui->questTimerField->setText(QString("%1 sec.").arg(m_selectedQuest->timelimit()));
			m_ui->questPlayerField->setText(QString("%1").arg(m_selectedQuest->suggestedplayers()));

			const std::map<UInt32, QString> questTypes = { 
				{1, "Group"}, 
				{21, "Life"}, 
				{41, "PvP"}, 
				{62, "Raid"}, 
				{81, "Dungeon"}, 
				{82, "World Event"}, 
				{83, "Legendary"}, 
				{84, "Escort"}, 
				{85, "Heroic"} 
			};

			auto typeIt = questTypes.find(m_selectedQuest->type());
			if (typeIt != questTypes.end())
			{
				m_ui->lineEdit_5->setText(typeIt->second);
			}
			else 
			{
				m_ui->lineEdit_5->setText(m_selectedQuest->type() == 0 ? "None" : "UNKNOWN");
			}

			for (size_t i = 1; i <= 13; ++i)
			{
				QCheckBox *box = m_ui->questFlagsBox->findChild<QCheckBox*>(QString("quest_flag_%1").arg(i));
				if (box)
				{
					const bool hasAttribute = (m_selectedQuest->flags() & (1 << (i - 1))) != 0;
					box->setChecked(hasAttribute);
				}
			}
			
			const auto *srcItem = m_selectedQuest->srcitemid() ?
				m_application.getProject().items.getById(m_selectedQuest->srcitemid()) : nullptr;
			m_ui->questSourceItemButton->setText(
				QString("%1").arg(srcItem ? QString("%1x %2 (%3)").arg(m_selectedQuest->srcitemcount()).arg(srcItem->name().c_str()).arg(srcItem->id()) : "NONE"));

			m_ui->questDetailsTextField->setText(m_selectedQuest->detailstext().c_str());
			m_ui->questObjectivesTextField->setText(m_selectedQuest->objectivestext().c_str());
			m_ui->questOfferRewardTextField->setText(m_selectedQuest->offerrewardtext().c_str());
			m_ui->questRequestItemsTextField->setText(m_selectedQuest->requestitemstext().c_str());
			m_ui->questEndTextField->setText(m_selectedQuest->endtext().c_str());

			const auto *rewardSpell = m_selectedQuest->rewardspell() ? m_application.getProject().spells.getById(m_selectedQuest->rewardspell()) : nullptr;
			const auto *rewardCast = m_selectedQuest->rewardspellcast() ? m_application.getProject().spells.getById(m_selectedQuest->rewardspellcast()) : nullptr;
			m_ui->pushButton->setText(QString("%1").arg(rewardSpell ? QString("%1 %2").arg(rewardSpell->id()).arg(rewardSpell->name().c_str()) : "NONE"));
			m_ui->pushButton_2->setText(QString("%1").arg(rewardCast ? QString("%1 %2").arg(rewardCast->id()).arg(rewardCast->name().c_str()) : "NONE"));

			m_ui->lineEdit->setText(QString("%1").arg(m_selectedQuest->rewardmoney()));
			m_ui->lineEdit_2->setText(QString("%1").arg(m_selectedQuest->rewardmoneymaxlevel()));
			m_ui->lineEdit_3->setText(QString("%1").arg(m_selectedQuest->rewardmailtemplate()));
			m_ui->lineEdit_4->setText(QString("%1 sec.").arg(m_selectedQuest->rewardmaildelaysecs()));

			m_ui->questChoosableItemWidget->clear();
			for (const auto &entry : quest->rewarditemschoice())
			{
				const auto *item = m_application.getProject().items.getById(entry.itemid());
				if (item)
				{
					QTreeWidgetItem *treeitem = new QTreeWidgetItem();
					treeitem->setText(0, QString("%1 %2").arg(entry.itemid(), 5, 10, QLatin1Char('0')).arg(item->name().c_str()));
					treeitem->setText(1, QString("%1x").arg(entry.count()));
					applyItemColorToWidget(treeitem, 0, item->quality());
					m_ui->questChoosableItemWidget->addTopLevelItem(treeitem);
				}
			}
			m_ui->questAdditionalItemWidget->clear();
			for (const auto &entry : quest->rewarditems())
			{
				const auto *item = m_application.getProject().items.getById(entry.itemid());
				if (item)
				{
					QTreeWidgetItem *treeitem = new QTreeWidgetItem();
					treeitem->setText(0, QString("%1 %2").arg(entry.itemid(), 5, 10, QLatin1Char('0')).arg(item->name().c_str()));
					treeitem->setText(1, QString("%1x").arg(entry.count()));
					applyItemColorToWidget(treeitem, 0, item->quality());
					m_ui->questAdditionalItemWidget->addTopLevelItem(treeitem);
				}
			}
			m_ui->questReputationWidget->clear();
			for (const auto &entry : quest->rewardreputations())
			{
				const auto *faction = m_application.getProject().factions.getById(entry.factionid());
				if (faction)
				{
					QTreeWidgetItem *treeitem = new QTreeWidgetItem();
					treeitem->setText(0, QString("%1 %2").arg(entry.factionid(), 4, 10, QLatin1Char('0')).arg(faction->name().c_str()));
					treeitem->setText(1, QString("%1").arg(entry.value()));
					m_ui->questReputationWidget->addTopLevelItem(treeitem);
				}
			}

			m_ui->questRequirementWidget->clear();
			for (const auto &entry : quest->requirements())
			{
				const auto *item = entry.itemid() ? m_application.getProject().items.getById(entry.itemid()) : nullptr;
				const auto *source = entry.sourceid() ? m_application.getProject().items.getById(entry.sourceid()) : nullptr;
				const auto *spellcast = entry.spellcast() ? m_application.getProject().spells.getById(entry.spellcast()) : nullptr;
				const auto *creature = entry.creatureid() ? m_application.getProject().units.getById(entry.creatureid()) : nullptr;
				const auto *object = entry.objectid() ? m_application.getProject().objects.getById(entry.objectid()) : nullptr;

				QTreeWidgetItem *treeitem = new QTreeWidgetItem();
				
				if (item)
				{
					treeitem->setText(0, QString("%1x %2 (%3)").arg(entry.itemcount()).arg(item->name().c_str()).arg(item->id()));
					applyItemColorToWidget(treeitem, 0, item->quality());
				}
				if (source)
				{
					treeitem->setText(1, QString("%1x %2 (%3)").arg(entry.sourcecount()).arg(source->name().c_str()).arg(source->id()));
					applyItemColorToWidget(treeitem, 1, source->quality());
				}
				if (creature)
				{
					treeitem->setText(2, QString("%1x %2 (%3)").arg(entry.creaturecount()).arg(creature->name().c_str()).arg(creature->id()));
				}
				if (object)
				{
					treeitem->setText(3, QString("%1x %2 (%3)").arg(entry.objectcount()).arg(object->name().c_str()).arg(object->id()));
				}
				if (spellcast)
				{
					treeitem->setText(4, QString("%1 (%2)").arg(spellcast->name().c_str()).arg(spellcast->id()));
				}
				treeitem->setText(5, entry.text().c_str());

				m_ui->questRequirementWidget->addTopLevelItem(treeitem);
			}

			m_ui->questAcceptTriggerBox->clear();
			for (const auto &triggerId : quest->starttriggers())
			{
				const auto *triggerEntry = m_application.getProject().triggers.getById(triggerId);
				if (triggerEntry)
				{
					m_ui->questAcceptTriggerBox->addItem(
						QString(triggerEntry->name().c_str()));
				}
			}

			m_ui->questFailureTriggerBox->clear();
			for (const auto &triggerId : quest->failtriggers())
			{
				const auto *triggerEntry = m_application.getProject().triggers.getById(triggerId);
				if (triggerEntry)
				{
					m_ui->questFailureTriggerBox->addItem(
						QString(triggerEntry->name().c_str()));
				}
			}

			m_ui->questRewardTriggerBox->clear();
			for (const auto &triggerId : quest->rewardtriggers())
			{
				const auto *triggerEntry = m_application.getProject().triggers.getById(triggerId);
				if (triggerEntry)
				{
					m_ui->questRewardTriggerBox->addItem(
						QString(triggerEntry->name().c_str()));
				}
			}
		}

		void ObjectEditor::onObjectSelectionChanged(const QItemSelection & selection, const QItemSelection & old)
		{
			// Get the selected unit
			m_selectedObject = nullptr;
			if (selection.isEmpty())
			{
				return;
			}

			QItemSelection source = m_objectFilter->mapSelectionToSource(selection);
			if (source.isEmpty())
			{
				return;
			}

			int index = source.indexes().first().row();
			if (index < 0)
			{
				return;
			}

			// Get object entry
			auto *object = m_application.getProject().objects.getTemplates().mutable_entry(index);
			m_selectedObject = object;
			if (!object)
				return;

			m_ui->objectQuestWidget->clear();
			for (const auto &questid : object->quests())
			{
				const auto *quest = m_application.getProject().quests.getById(questid);
				if (quest)
				{
					m_ui->objectQuestWidget->addItem(
						QString("%1 %2 [%3]").arg(questid, 5, 10, QLatin1Char('0')).arg(quest->name().c_str()).arg(static_cast<Int32>(quest->questlevel())));
				}
			}

			m_ui->objectEndQuestWidget->clear();
			for (const auto &questid : object->end_quests())
			{
				const auto *quest = m_application.getProject().quests.getById(questid);
				if (quest)
				{
					m_ui->objectEndQuestWidget->addItem(
						QString("%1 %2 [%3]").arg(questid, 5, 10, QLatin1Char('0')).arg(quest->name().c_str()).arg(static_cast<Int32>(quest->questlevel())));
				}
			}

			m_ui->objectLootView->clear();
			if (!object->objectlootentry())
			{
				m_ui->objectLootLine->setText("- NO LOOT -");
				m_ui->objectLootToolButton->setDisabled(true);
				m_ui->objectLootSimulatorButton->setDisabled(true);
			}
			else
			{
				QIcon groupIcon;
				groupIcon.addFile(QStringLiteral(":/Items.png"), QSize(), QIcon::Normal, QIcon::Off);

				size_t groupIndex = 0;
				const auto *lootEntry = m_application.getProject().objectLoot.getById(object->objectlootentry());
				if (lootEntry)
				{
					for (const auto &group : lootEntry->groups())
					{
						// Add group
						QTreeWidgetItem *groupItem = new QTreeWidgetItem();
						groupItem->setIcon(0, groupIcon);
						m_ui->objectLootView->addTopLevelItem(groupItem);

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

				m_ui->objectLootLine->setText(QString("Loot Entry %1").arg(object->objectlootentry()));
				m_ui->objectLootToolButton->setDisabled(false);
				m_ui->objectLootSimulatorButton->setDisabled(false);
			}

			m_ui->objectTriggerWidget->clear();
			for (const auto &triggerId : object->triggers())
			{
				const auto *triggerEntry = m_application.getProject().triggers.getById(triggerId);
				if (triggerEntry)
				{
					m_ui->objectTriggerWidget->addItem(
						QString(triggerEntry->name().c_str()));
				}
			}

			// Update properties
			m_objectProperties.clear();

			// Add unit properties
#define WOWPP_NUM_PROPERTY(name, type, ref, prop, readonly) { \
			auto getBinder = [object]() -> type { return object->prop(); }; \
			auto setBinder = [object](type value) { object->set_##prop(value); }; \
			m_objectProperties.push_back(PropertyPtr(new NumericProperty(name, ref(getBinder, setBinder), readonly))); }
#define WOWPP_STR_PROPERTY(name, prop, readonly) { \
			auto getBinder = [object]() -> String { return object->prop(); }; \
			auto setBinder = [object](const String &value) { object->set_##prop(value.c_str()); }; \
			m_objectProperties.push_back(PropertyPtr(new StringProperty(name, getBinder, setBinder, readonly))); }
#define WOWPP_MIN_MAX_PROPERTY(name, type, ref, prop, readonly) { \
			auto getMinBinder = [object]() -> type { return object->min##prop(); }; \
			auto setMinBinder = [object](type value) { object->set_min##prop(value); }; \
			auto getMaxBinder = [object]() -> type { return object->max##prop(); }; \
			auto setMaxBinder = [object](type value) { object->set_max##prop(value); }; \
			m_objectProperties.push_back(PropertyPtr(new MinMaxProperty(name, ref(getMinBinder, setMinBinder), ref(getMaxBinder, setMaxBinder), readonly))); }

			WOWPP_NUM_PROPERTY("Entry", UInt32, UInt32Ref, id, true);
			WOWPP_STR_PROPERTY("Name", name, false);
			WOWPP_STR_PROPERTY("Caption", caption, false);
			WOWPP_NUM_PROPERTY("Faction", UInt32, UInt32Ref, factionid, false);
			WOWPP_NUM_PROPERTY("Type", UInt32, UInt32Ref, type, false);
			WOWPP_NUM_PROPERTY("Flags", UInt32, UInt32Ref, flags, false);
			WOWPP_MIN_MAX_PROPERTY("Loot Gold", UInt32, UInt32Ref, gold, false);

#undef WOWPP_MIN_MAX_PROPERTY
#undef WOWPP_STR_PROPERTY
#undef WOWPP_NUM_PROPERTY

			m_ui->objectVariableWidget->clear();
			for (const auto &variableId : object->variables())
			{
				const auto *variableEntry = m_application.getProject().variables.getById(variableId);
				if (variableEntry)
				{
					m_ui->objectVariableWidget->addItem(
						QString(variableEntry->name().c_str()));
				}
			}

			// Update the view 
			m_objectViewModel->layoutChanged();
		}

		void ObjectEditor::on_unitAddTriggerBtn_clicked()
		{
			if (!m_selectedUnit)
				return;

			ChooseTriggerDialog dialog(m_application);
			auto result = dialog.exec();
			if (result == QDialog::Accepted)
			{
				const auto *newTrigger = dialog.getSelectedTrigger();
				if (newTrigger)
				{
					auto it = std::find_if(m_selectedUnit->triggers().begin(), m_selectedUnit->triggers().end(), [&newTrigger](const UInt32 &trigger) -> bool
					{
						return (trigger == newTrigger->id());
					});

					if (it == m_selectedUnit->triggers().end())
					{
						m_selectedUnit->mutable_triggers()->Add(newTrigger->id());
						m_ui->unitTriggerWidget->addItem(
							QString(newTrigger->name().c_str()));
					}
				}
			}
		}

		void ObjectEditor::on_unitRemoveTriggerBtn_clicked()
		{
			if (!m_selectedUnit)
				return;

			// Find selected trigger
			auto index = m_ui->unitTriggerWidget->currentIndex();
			if (index.isValid())
			{
				int row = index.row();
				if (row < 0 || row >= m_selectedUnit->triggers_size())
					return;

				if (m_selectedUnit->triggers_size() > 1 &&
					row != m_selectedUnit->triggers_size() - 1)
				{
					m_selectedUnit->mutable_triggers()->SwapElements(row, m_selectedUnit->triggers_size() - 1);
				}
				m_selectedUnit->mutable_triggers()->RemoveLast();

				auto *taken = m_ui->unitTriggerWidget->takeItem(row);
				delete taken;
			}
		}

		void ObjectEditor::on_questAcceptAddTriggerBtn_clicked()
		{
			if (!m_selectedQuest)
				return;

			ChooseTriggerDialog dialog(m_application);
			auto result = dialog.exec();
			if (result == QDialog::Accepted)
			{
				const auto *newTrigger = dialog.getSelectedTrigger();
				if (newTrigger)
				{
					auto it = std::find_if(m_selectedQuest->starttriggers().begin(), m_selectedQuest->starttriggers().end(), [&newTrigger](const UInt32 &trigger) -> bool
					{
						return (trigger == newTrigger->id());
					});

					if (it == m_selectedQuest->starttriggers().end())
					{
						m_selectedQuest->mutable_starttriggers()->Add(newTrigger->id());
						m_ui->questAcceptTriggerBox->addItem(
							QString(newTrigger->name().c_str()));
					}
				}
			}
		}

		void ObjectEditor::on_questAcceptRemoveTriggerBtn_clicked()
		{
		}

		void ObjectEditor::on_questFailureAddTriggerBtn_clicked()
		{
			if (!m_selectedQuest)
				return;

			ChooseTriggerDialog dialog(m_application);
			auto result = dialog.exec();
			if (result == QDialog::Accepted)
			{
				const auto *newTrigger = dialog.getSelectedTrigger();
				if (newTrigger)
				{
					auto it = std::find_if(m_selectedQuest->failtriggers().begin(), m_selectedQuest->failtriggers().end(), [&newTrigger](const UInt32 &trigger) -> bool
					{
						return (trigger == newTrigger->id());
					});

					if (it == m_selectedQuest->failtriggers().end())
					{
						m_selectedQuest->mutable_failtriggers()->Add(newTrigger->id());
						m_ui->questFailureTriggerBox->addItem(
							QString(newTrigger->name().c_str()));
					}
				}
			}
		}

		void ObjectEditor::on_questFailureRemoveTriggerBtn_clicked()
		{
		}

		void ObjectEditor::on_questRewardAddTriggerBtn_clicked()
		{
			if (!m_selectedQuest)
				return;

			ChooseTriggerDialog dialog(m_application);
			auto result = dialog.exec();
			if (result == QDialog::Accepted)
			{
				const auto *newTrigger = dialog.getSelectedTrigger();
				if (newTrigger)
				{
					auto it = std::find_if(m_selectedQuest->rewardtriggers().begin(), m_selectedQuest->rewardtriggers().end(), [&newTrigger](const UInt32 &trigger) -> bool
					{
						return (trigger == newTrigger->id());
					});

					if (it == m_selectedQuest->rewardtriggers().end())
					{
						m_selectedQuest->mutable_rewardtriggers()->Add(newTrigger->id());
						m_ui->questRewardTriggerBox->addItem(
							QString(newTrigger->name().c_str()));
					}
				}
			}
		}

		void ObjectEditor::on_questRewardRemoveTriggerBtn_clicked()
		{
		}

		void ObjectEditor::on_objectAddTriggerBtn_clicked()
		{
			if (!m_selectedObject)
				return;

			ChooseTriggerDialog dialog(m_application);
			auto result = dialog.exec();
			if (result == QDialog::Accepted)
			{
				const auto *newTrigger = dialog.getSelectedTrigger();
				if (newTrigger)
				{
					auto it = std::find_if(m_selectedObject->triggers().begin(), m_selectedObject->triggers().end(), [&newTrigger](const UInt32 &trigger) -> bool
					{
						return (trigger == newTrigger->id());
					});

					if (it == m_selectedObject->triggers().end())
					{
						m_selectedObject->mutable_triggers()->Add(newTrigger->id());
						m_ui->objectTriggerWidget->addItem(
							QString(newTrigger->name().c_str()));
					}
				}
			}
		}

		void ObjectEditor::on_unitAddVarBtn_clicked()
		{
			if (!m_selectedUnit)
				return;

			ChooseVariablesDialog dialog(m_application);
			auto result = dialog.exec();
			if (result == QDialog::Accepted)
			{
				const auto &selected  = dialog.getSelected();
				for (const auto *newVar : selected)
				{
					if (newVar)
					{
						auto it = std::find_if(m_selectedUnit->variables().begin(), m_selectedUnit->variables().end(), [&newVar](const UInt32 &variable) -> bool
						{
							return (variable == newVar->id());
						});

						if (it == m_selectedUnit->variables().end())
						{
							m_selectedUnit->mutable_variables()->Add(newVar->id());
							m_ui->unitVariableWidget->addItem(
								QString(newVar->name().c_str()));
						}
					}
				}
			}
		}

		void ObjectEditor::on_objectAddVarBtn_clicked()
		{
			if (!m_selectedObject)
				return;

			ChooseVariablesDialog dialog(m_application);
			auto result = dialog.exec();
			if (result == QDialog::Accepted)
			{
				const auto &selected = dialog.getSelected();
				for (const auto *newVar : selected)
				{
					if (newVar)
					{
						auto it = std::find_if(m_selectedObject->variables().begin(), m_selectedObject->variables().end(), [&newVar](const UInt32 &variable) -> bool
						{
							return (variable == newVar->id());
						});

						if (it == m_selectedObject->variables().end())
						{
							m_selectedObject->mutable_variables()->Add(newVar->id());
							m_ui->objectVariableWidget->addItem(
								QString(newVar->name().c_str()));
						}
					}
				}
			}
		}

		void ObjectEditor::onRaceClassChanged(int state)
		{
			if (!m_selectedSpell)
				return;

			if (m_selectionChanging)
				return;

			bool hasMissingEntry = false;

			// Build spells race mask
			UInt32 raceMask = 0;
			for (UInt32 i = 1; i <= game::race::Max; ++i)
			{
				QCheckBox *box = m_ui->spellRaceBox->findChild<QCheckBox*>(QString("race_%1").arg(i));
				if (box)
				{
					const bool hasAttribute = box->isChecked();
					if (hasAttribute)
					{
						raceMask |= (1 << (i - 1));
					}
					else
					{
						hasMissingEntry = true;
					}
				}
			}

			if (!hasMissingEntry)
				raceMask = 0;

			hasMissingEntry = false;

			// Build spells class mask
			UInt32 classMask = 0;
			for (UInt32 i = 1; i <= game::char_class::Max; ++i)
			{
				QCheckBox *box = m_ui->spellClassBox->findChild<QCheckBox*>(QString("class_%1").arg(i));
				if (box)
				{
					const bool hasAttribute = box->isChecked();
					if (hasAttribute)
					{
						classMask |= (1 << (i - 1));
					}
					else
					{
						hasMissingEntry = true;
					}
				}
			}

			if (!hasMissingEntry)
				classMask = 0;

			// Update spell
			m_selectedSpell->set_racemask(raceMask);
			m_selectedSpell->set_classmask(classMask);
		}

		void ObjectEditor::onMechanicImmunityChanged(int state)
		{
			if (!m_selectedUnit)
				return;

			if (m_selectionChanging)
				return;

			// Build mask
			UInt32 mask = 0;
			for (UInt32 i = 1; i <= 30; ++i)
			{
				QCheckBox *box = m_ui->mechanicImmunityBox->findChild<QCheckBox*>(QString("mech_flag_%1").arg(i));
				if (box)
				{
					const bool hasAttribute = box->isChecked();
					if (hasAttribute)
					{
						mask |= (1 << (i - 1));
					}
				}
			}

			m_selectedUnit->set_mechanicimmunity(mask);
		}

		void ObjectEditor::onSchoolImmunityChanged(int state)
		{
			if (!m_selectedUnit)
				return;

			if (m_selectionChanging)
				return;

			// Build mask
			UInt32 mask = 0;
			for (UInt32 i = 1; i <= game::spell_school::End; ++i)
			{
				QCheckBox *box = m_ui->schoolImmunityBox->findChild<QCheckBox*>(QString("school_flag_%1").arg(i));
				if (box)
				{
					const bool hasAttribute = box->isChecked();
					if (hasAttribute)
					{
						mask |= (1 << (i - 1));
					}
				}
			}

			m_selectedUnit->set_schoolimmunity(mask);
		}

		void ObjectEditor::on_spellPositiveBox_stateChanged(int state)
		{
			if (!m_selectedSpell)
				return;

			if (m_selectionChanging)
				return;

			m_selectedSpell->set_positive(state == Qt::Checked ? 1 : 0);
		}

		void ObjectEditor::on_objectRemoveTriggerBtn_clicked()
		{
			if (!m_selectedObject)
				return;

			// Find selected trigger
			auto index = m_ui->objectTriggerWidget->currentIndex();
			if (index.isValid())
			{
				int row = index.row();
				if (row < 0 || row >= m_selectedObject->triggers_size())
					return;

				if (m_selectedObject->triggers_size() > 1 &&
					row != m_selectedObject->triggers_size() - 1)
				{
					m_selectedObject->mutable_triggers()->SwapElements(row, m_selectedObject->triggers_size() - 1);
				}
				m_selectedObject->mutable_triggers()->RemoveLast();

				auto *taken = m_ui->objectTriggerWidget->takeItem(row);
				delete taken;
			}
		}

		void ObjectEditor::on_lootSimulatorButton_clicked()
		{
			if (!m_selectedUnit)
				return;

			if (!m_selectedUnit->unitlootentry())
				return;

			const auto *loot = m_application.getProject().unitLoot.getById(m_selectedUnit->unitlootentry());
			if (!loot)
				return;

			LootDialog dialog(m_application.getProject(), *loot);
			dialog.exec();
		}

		void ObjectEditor::on_objectLootSimulatorButton_clicked()
		{
			if (!m_selectedObject)
				return;

			if (!m_selectedObject->objectlootentry())
				return;

			const auto *loot = m_application.getProject().objectLoot.getById(m_selectedObject->objectlootentry());
			if (!loot)
				return;

			LootDialog dialog(m_application.getProject(), *loot);
			dialog.exec();
		}

		void ObjectEditor::on_itemLootSimulatorButton_clicked()
		{
			if (!m_selectedItem)
				return;

			if (!m_selectedItem->lootentry())
				return;

			const auto *loot = m_application.getProject().itemLoot.getById(m_selectedItem->lootentry());
			if (!loot)
				return;

			LootDialog dialog(m_application.getProject(), *loot);
			dialog.exec();
		}

		void ObjectEditor::on_skinLootSimulatorButton_clicked()
		{
			if (!m_selectedUnit)
				return;

			if (!m_selectedUnit->skinninglootentry())
				return;

			const auto *loot = m_application.getProject().skinningLoot.getById(m_selectedUnit->skinninglootentry());
			if (!loot)
				return;

			LootDialog dialog(m_application.getProject(), *loot);
			dialog.exec();
		}

		void ObjectEditor::on_actionImport_triggered()
		{
			// Counter variables used by the import method
			UInt32 lastEntry = 0;
			UInt32 lastGroup = 0;
			UInt32 groupIndex = 0;

			// Prepare the import task
			ImportTask task;
			task.countQuery = "(SELECT COUNT(*) FROM `wowpp_creature_loot_template` WHERE `active` != 0) UNION (SELECT COUNT(*) FROM `wowpp_creatureloot` WHERE `active` != 0);";
			task.selectQuery = "(SELECT `entry`, `item`, `ChanceOrQuestChance`, `groupid`, `mincountOrRef`, `maxcount`,`lootcondition`,`condition_value1`,`condition_value2` FROM `wowpp_creature_loot_template` WHERE `active` != 0)"
				" UNION "
				" (SELECT `entry`, `item`, `ChanceOrQuestChance`, `groupid`, `mincountOrRef`, `maxcount`,`lootcondition`,`condition_value1`,`condition_value2` FROM `wowpp_creatureloot` WHERE `active` != 0) "
				" ORDER BY `entry`, `groupid`;";
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
				UInt32 entry = 0, itemId = 0, groupId = 0, minCount = 0, maxCount = 0, cond = 0, conda = 0, condb = 0;
				float dropChance = 0.0f;
				row.getField(0, entry);
				row.getField(1, itemId);
				row.getField(2, dropChance);
				row.getField(3, groupId);
				row.getField(4, minCount);
				row.getField(5, maxCount);
				row.getField(6, cond);
				row.getField(7, conda);
				row.getField(8, condb);

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
					m_application.getProject().unitLoot.add(entry);
					
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
					lootEntry->add_groups();
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
				def->set_conditiontype(cond);
				def->set_conditionvala(conda);
				def->set_conditionvalb(condb);

				return true;
			};
			
			// Do import job
			ImportDialog dialog(m_application, std::move(task));
			dialog.exec();
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
					m_application.getProject().vendors.add(entry);
					
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
			dialog.exec();
		}
		void ObjectEditor::on_actionImport_Trainers_triggered()
		{
			UInt32 lastEntry = 0;

			ImportTask task;
			task.countQuery = "SELECT COUNT(*) FROM `wowpp_npc_trainer_template`;";
			task.selectQuery = "SELECT `entry`, `spell`, `spellcost`, `reqskill`, `reqskillvalue`,`reqlevel` FROM `wowpp_npc_trainer_template` ORDER BY `entry`;";
			task.beforeImport = [this]() {
				for (int i = 0; i < m_application.getProject().units.getTemplates().entry_size(); ++i)
				{
					auto *unit = m_application.getProject().units.getTemplates().mutable_entry(i);
					unit->set_trainerentry(0);
				}
				m_application.getProject().trainers.clear();
			};
			task.onImport = [this, &lastEntry](wowpp::MySQL::Row &row) -> bool {
				UInt32 entry = 0, spellId = 0, spellcost = 0, reqskill = 0, reqskillvalue = 0, reqlevel = 1;
				row.getField(0, entry);
				row.getField(1, spellId);
				row.getField(2, spellcost);
				row.getField(3, reqskill);
				row.getField(4, reqskillvalue);
				row.getField(5, reqlevel);

				// Find referenced spell
				const auto *spellEntry = m_application.getProject().spells.getById(spellId);
				if (!spellEntry)
				{
					ELOG("Could not find referenced spell " << spellId << " (referenced in trainer entry " << entry << ")");
					return false;
				}

				// Create a new trainer entry
				bool created = false;
				if (entry > lastEntry)
				{
					m_application.getProject().trainers.add(entry);
					
					lastEntry = entry;
					created = true;
				}

				auto *trainerEntry = m_application.getProject().trainers.getById(entry);
				if (!trainerEntry)
				{
					ELOG("Trainer entry " << entry << " not found!");
					return false;
				}

				// Add spells
				auto *addedSpell = trainerEntry->add_spells();
				addedSpell->set_spell(spellId);
				addedSpell->set_spellcost(spellcost);
				addedSpell->set_reqskill(reqskill);
				addedSpell->set_reqskillval(reqskillvalue);
				addedSpell->set_reqlevel(reqlevel);

				return true;
			};

			// Do import job
			ImportDialog dialog(m_application, std::move(task));
			dialog.exec();
		}
		void ObjectEditor::on_actionImport_Quests_triggered()
		{
			ImportTask task;
			task.countQuery = "SELECT COUNT(*) FROM `wowpp_quests`;";
			task.selectQuery = "SELECT `entry`, `Title`, `Method`, `MinLevel`, `QuestLevel`, `Details`, `Objectives`,`OfferRewardText`,`RequestItemsText`,`EndText`, "
				"`RewChoiceItemId1`, `RewChoiceItemCount1`, `RewChoiceItemId2`, `RewChoiceItemCount2`,`RewChoiceItemId3`, `RewChoiceItemCount3`,`RewChoiceItemId4`, `RewChoiceItemCount4`, `RewChoiceItemId5`, `RewChoiceItemCount5`, `RewChoiceItemId6`, `RewChoiceItemCount6`,"
				"`RewItemId1`, `RewItemCount1`, `RewItemId2`, `RewItemCount2`,`RewItemId3`, `RewItemCount3`,`RewItemId4`, `RewItemCount4`,"
				"`RewRepFaction1`, `RewRepValue1`, `RewRepFaction2`, `RewRepValue2`,`RewRepFaction3`, `RewRepValue3`,`RewRepFaction4`, `RewRepValue4`,`RewRepFaction5`, `RewRepValue5`,"
				"`RewHonorableKills`, `RewOrReqMoney`, `RewMoneyMaxLevel`, `RewSpell`,`RewSpellCast`, `RewMailTemplateId`,`RewMailDelaySecs`,"
				"`ObjectiveText1`, `ReqItemId1`, `ReqItemCount1`, `ReqSourceId1`,`ReqSourceCount1`, `ReqCreatureOrGOId1`,`ReqCreatureOrGOCount1`,`ReqSpellCast1`,"
				"`ObjectiveText2`, `ReqItemId2`, `ReqItemCount2`, `ReqSourceId2`,`ReqSourceCount2`, `ReqCreatureOrGOId2`,`ReqCreatureOrGOCount2`,`ReqSpellCast2`,"
				"`ObjectiveText3`, `ReqItemId3`, `ReqItemCount3`, `ReqSourceId3`,`ReqSourceCount3`, `ReqCreatureOrGOId3`,`ReqCreatureOrGOCount3`,`ReqSpellCast3`,"
				"`ObjectiveText4`, `ReqItemId4`, `ReqItemCount4`, `ReqSourceId4`,`ReqSourceCount4`, `ReqCreatureOrGOId4`,`ReqCreatureOrGOCount4`,`ReqSpellCast4`,"
				"`PrevQuestId`, `NextQuestId`, `ExclusiveGroup`, `NextQuestInChain`,"
				"`QuestFlags`, `SpecialFlags`, `Type`, `ZoneOrSort`, `SuggestedPlayers`, `LimitTime`, `SrcItemId`, `SrcItemCount`, `SrcSpell`, `CharTitleId`,"
				"`PointMapId`, `PointX`, `PointY`, `PointOpt`, `RequiredRaces`, `RequiredClasses`, `RequiredSkill`, `RequiredSkillValue`"
				" FROM `wowpp_quests` ORDER BY `entry`;";
			task.beforeImport = [this]() {
				m_application.getProject().quests.clear();
			};
			task.onImport = [this](wowpp::MySQL::Row &row) -> bool {
				UInt32 entry = 0, method = 0, minlevel = 0, questlevel = 0;
				String title, details, objectives, offerreward, requestitems, end;
				UInt32 index = 0;
				row.getField(index++, entry);
				row.getField(index++, title);
				row.getField(index++, method);

				row.getField(index++, minlevel);
				row.getField(index++, questlevel);

				row.getField(index++, details);
				row.getField(index++, objectives);
				row.getField(index++, offerreward);
				row.getField(index++, requestitems);
				row.getField(index++, end);

				auto *added = m_application.getProject().quests.add(entry);
				added->set_name(title.c_str());
				added->set_method(method);
				added->set_minlevel(minlevel);
				added->set_questlevel(questlevel);

				if (!details.empty()) added->set_detailstext(details);
				if (!objectives.empty()) added->set_objectivestext(objectives);
				if (!offerreward.empty()) added->set_offerrewardtext(offerreward);
				if (!requestitems.empty()) added->set_requestitemstext(requestitems);
				if (!end.empty()) added->set_endtext(end);

				for (UInt32 i = 0; i < 6; ++i)
				{
					UInt32 choiceItemId = 0, choiceItemCount = 0;
					row.getField(index++, choiceItemId);
					row.getField(index++, choiceItemCount);
					if (choiceItemId > 0)
					{
						// Check if item exists
						if (m_application.getProject().items.getById(choiceItemId))
						{
							auto *addedChoice = added->add_rewarditemschoice();
							addedChoice->set_itemid(choiceItemId);
							addedChoice->set_count(choiceItemCount);
						}
					}
				}

				for (UInt32 i = 0; i < 4; ++i)
				{
					UInt32 itemId = 0, itemCount = 0;
					row.getField(index++, itemId);
					row.getField(index++, itemCount);
					if (itemId > 0)
					{
						// Check if item exists
						if (m_application.getProject().items.getById(itemId))
						{
							auto *addedItem = added->add_rewarditems();
							addedItem->set_itemid(itemId);
							addedItem->set_count(itemCount);
						}
					}
				}

				for (UInt32 i = 0; i < 5; ++i)
				{
					UInt32 factionId = 0;
					Int32 repValue = 0;
					row.getField(index++, factionId);
					row.getField(index++, repValue);
					if (factionId > 0)
					{
						// Check if faction exists
						if (m_application.getProject().factions.getById(factionId))
						{
							auto *addedRep = added->add_rewardreputations();
							addedRep->set_factionid(factionId);
							addedRep->set_value(repValue);
						}
					}
				}

				UInt32 rewHonorKills = 0, rewMoneyMaxLevel = 0, rewSpell = 0, rewSpellCast = 0, rewMailTemplate = 0, rewMailDelaySecs = 0;
				Int32 rewOrReqMoney = 0;
				row.getField(index++, rewHonorKills);
				row.getField(index++, rewOrReqMoney);
				row.getField(index++, rewMoneyMaxLevel);
				row.getField(index++, rewSpell);
				row.getField(index++, rewSpellCast);
				row.getField(index++, rewMailTemplate);
				row.getField(index++, rewMailDelaySecs);
				added->set_rewardhonorkills(rewHonorKills);
				if (rewOrReqMoney > 0) added->set_rewardmoney(rewOrReqMoney);
				if (rewMoneyMaxLevel > 0) added->set_rewardmoneymaxlevel(rewMoneyMaxLevel);
				if (rewSpell && m_application.getProject().spells.getById(rewSpell)) added->set_rewardspell(rewSpell);
				if (rewSpellCast && m_application.getProject().spells.getById(rewSpellCast)) added->set_rewardspellcast(rewSpellCast);
				added->set_rewardmailtemplate(rewMailTemplate);
				added->set_rewardmaildelaysecs(rewMailDelaySecs);

				for (UInt32 i = 0; i < 4; ++i)
				{
					//"`ObjectiveText1`, `ReqItemId1`, `ReqItemCount1`, `ReqSourceId1`,`ReqSourceCount1`, `ReqCreatureOrGOId1`,`ReqCreatureOrGOCount1`,`ReqSpellCast1`,"

					std::string text;
					UInt32 itemId = 0, itemCount = 0, sourceId = 0, sourceCount = 0, creatureGoCount = 0, spellCast = 0;
					Int32 creatureGo = 0;

					row.getField(index++, text);
					row.getField(index++, itemId);
					row.getField(index++, itemCount);
					row.getField(index++, sourceId);
					row.getField(index++, sourceCount);
					row.getField(index++, creatureGo);
					row.getField(index++, creatureGoCount);
					row.getField(index++, spellCast);
					if (!text.empty() ||
						itemId != 0 ||
						itemCount != 0 ||
						sourceId != 0 ||
						sourceCount != 0 ||
						creatureGo != 0 ||
						creatureGoCount != 0 ||
						spellCast != 0)
					{
						if (itemId && !m_application.getProject().items.getById(itemId)) itemId = 0;
						if (sourceId && !m_application.getProject().items.getById(sourceId)) sourceId = 0;
						if (creatureGo > 0 && !m_application.getProject().units.getById(creatureGo))
						{
							creatureGo = 0;
							creatureGoCount = 0;
						}
						if (creatureGo < 0 && !m_application.getProject().objects.getById(-creatureGo))
						{
							creatureGo = 0;
							creatureGoCount = 0;
						}
						if (spellCast && !m_application.getProject().spells.getById(spellCast)) spellCast = 0;

						auto *addedReq = added->add_requirements();
						addedReq->set_itemid(itemId);
						addedReq->set_itemcount(itemCount);
						addedReq->set_sourceid(sourceId);
						addedReq->set_sourcecount(sourceCount);
						addedReq->set_creatureid(creatureGo > 0 ? creatureGo : 0);
						addedReq->set_creaturecount(creatureGo > 0 ? creatureGoCount : 0);
						addedReq->set_objectid(creatureGo < 0 ? -creatureGo : 0);
						addedReq->set_objectcount(creatureGo < 0 ? creatureGoCount : 0);
						addedReq->set_spellcast(spellCast);
						addedReq->set_text(text);
					}
				}

				Int32 prevQuestId = 0, nextQuestId = 0, exclusiveGroup = 0, nextQuestInChain = 0;
				row.getField(index++, prevQuestId);
				row.getField(index++, nextQuestId);
				row.getField(index++, exclusiveGroup);
				row.getField(index++, nextQuestInChain);
				if (prevQuestId == entry) prevQuestId = 0;
				if (nextQuestId == entry) nextQuestId = 0;
				if (nextQuestInChain == entry) nextQuestInChain = 0;
				added->set_prevquestid(prevQuestId);
				added->set_nextquestid(nextQuestId);
				added->set_exclusivegroup(exclusiveGroup);
				added->set_nextchainquestid(nextQuestInChain);

				UInt32 flags = 0, specialFlags = 0, type = 0, suggestedPlayers = 0, limitTime = 0, srcItemId = 0;
				UInt32 srcItemCount = 0, srcSpell = 0, charTitleId = 0, pointMapId = 0, pointOpt = 0, races = 0, classes = 0, skill = 0, skillval = 0;
				Int32 zoneOrSort = 0;
				float pointX = 0.0f, pointY = 0.0f;
				row.getField(index++, flags);
				row.getField(index++, specialFlags);
				row.getField(index++, type);
				row.getField(index++, zoneOrSort);
				row.getField(index++, suggestedPlayers);
				row.getField(index++, limitTime);
				row.getField(index++, srcItemId);
				row.getField(index++, srcItemCount);
				row.getField(index++, srcSpell);
				row.getField(index++, charTitleId);
				row.getField(index++, pointMapId);
				row.getField(index++, pointX);
				row.getField(index++, pointY);
				row.getField(index++, pointOpt);
				row.getField(index++, races);
				row.getField(index++, classes);
				row.getField(index++, skill);
				row.getField(index++, skillval);
				added->set_flags(flags);
				added->set_extraflags(specialFlags);
				added->set_type(type);
				added->set_zone(zoneOrSort);
				added->set_suggestedplayers(suggestedPlayers);
				added->set_timelimit(limitTime);
				added->set_srcitemid(srcItemId);
				added->set_srcitemcount(srcItemCount);
				added->set_srcspell(srcSpell);
				added->set_rewtitleid(charTitleId);
				added->set_pointmapid(pointMapId);
				added->set_pointx(pointX);
				added->set_pointy(pointY);
				added->set_pointopt(pointOpt);
				added->set_requiredraces(races);
				added->set_requiredclasses(classes);
				added->set_requiredskill(skill);
				added->set_requiredskillval(skillval);

				return true;
			};
			task.afterImport = [this]()
			{

			};

			// Do import job
			ImportDialog dialog(m_application, std::move(task));
			dialog.exec();
		}
		void ObjectEditor::on_actionImport_Object_Loot_triggered()
		{
			// Counter variables used by the import method
			UInt32 lastEntry = 0;
			UInt32 lastGroup = 0;
			UInt32 groupIndex = 0;

			// Prepare the import task
			ImportTask task;
			task.countQuery = "SELECT COUNT(*) FROM `wowpp_gameobject_loot_template` WHERE `active` != 0;";
			task.selectQuery = "SELECT `entry`, `item`, `ChanceOrQuestChance`, `groupid`, `mincountOrRef`, `maxcount`,`lootcondition`,`condition_value1`,`condition_value2` FROM `wowpp_gameobject_loot_template` WHERE `active` != 0"
				" ORDER BY `entry`, `groupid`;";
			task.beforeImport = [this]() {
				// Remove old object loot
				for (int i = 0; i < m_application.getProject().objects.getTemplates().entry_size(); ++i)
				{
					auto *object = m_application.getProject().objects.getTemplates().mutable_entry(i);
					object->set_objectlootentry(0);
				}
				m_application.getProject().objectLoot.clear();
			};
			task.onImport = [this, &lastEntry, &lastGroup, &groupIndex](wowpp::MySQL::Row &row) -> bool {
				UInt32 entry = 0, itemId = 0, groupId = 0, minCount = 0, maxCount = 0, cond = 0, conda = 0, condb = 0;
				float dropChance = 0.0f;
				row.getField(0, entry);
				row.getField(1, itemId);
				row.getField(2, dropChance);
				row.getField(3, groupId);
				row.getField(4, minCount);
				row.getField(5, maxCount);
				row.getField(6, cond);
				row.getField(7, conda);
				row.getField(8, condb);

				// Find referenced item
				const auto *itemEntry = m_application.getProject().items.getById(itemId);
				if (!itemEntry)
				{
					ELOG("Could not find referenced item " << itemId << " (referenced in object loot entry " << entry << " - group " << groupId << ")");
					return false;
				}

				// Create a new loot entry
				bool created = false;
				if (entry > lastEntry)
				{
					m_application.getProject().objectLoot.add(entry);
					
					lastEntry = entry;
					lastGroup = groupId;
					groupIndex = 0;
					created = true;
				}

				auto *lootEntry = m_application.getProject().objectLoot.getById(entry);
				if (!lootEntry)
				{
					// Error
					ELOG("Loot entry " << entry << " found, but no object to assign found");
					return false;
				}

				if (created)
				{
					auto *objectEntry = m_application.getProject().objects.getById(entry);
					if (!objectEntry)
					{
						WLOG("No object with entry " << entry << " found - object loot template will not be assigned!");
					}
					else
					{
						objectEntry->set_objectlootentry(lootEntry->id());
					}
				}

				// If there are no loot groups yet, create a new one
				if (lootEntry->groups().empty() || groupId > lastGroup)
				{
					lootEntry->add_groups();
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
				// We don't need condition value 9 in object loot!
				if (cond == 9)
				{
					cond = 0;
					conda = 0;
					condb = 0;
				}
				def->set_conditiontype(cond);
				def->set_conditionvala(conda);
				def->set_conditionvalb(condb);
				return true;
			};

			// Do import job
			ImportDialog dialog(m_application, std::move(task));
			dialog.exec();
		}
		void ObjectEditor::on_actionImportSkinningLoot_triggered()
		{
			// Counter variables used by the import method
			UInt32 lastEntry = 0;
			UInt32 lastGroup = 0;
			UInt32 groupIndex = 0;

			// Prepare the import task
			ImportTask task;
			task.countQuery = "(SELECT COUNT(*) FROM `wowpp_skinning_loot_template` WHERE `active` != 0);";
			task.selectQuery = "SELECT `entry`, `item`, `ChanceOrQuestChance`, `groupid`, `mincountOrRef`, `maxcount`,`lootcondition`,`condition_value1`,`condition_value2` FROM `wowpp_skinning_loot_template` WHERE `active` != 0"
				" ORDER BY `entry`, `groupid`;";
			task.beforeImport = [this]() {
				// Remove old unit loot
				for (int i = 0; i < m_application.getProject().units.getTemplates().entry_size(); ++i)
				{
					auto *unit = m_application.getProject().units.getTemplates().mutable_entry(i);
					unit->set_skinninglootentry(0);
				}
				m_application.getProject().skinningLoot.clear();
			};
			task.onImport = [this, &lastEntry, &lastGroup, &groupIndex](wowpp::MySQL::Row &row) -> bool {
				UInt32 entry = 0, itemId = 0, groupId = 0, minCount = 0, maxCount = 0, cond = 0, conda = 0, condb = 0;
				float dropChance = 0.0f;
				row.getField(0, entry);
				row.getField(1, itemId);
				row.getField(2, dropChance);
				row.getField(3, groupId);
				row.getField(4, minCount);
				row.getField(5, maxCount);
				row.getField(6, cond);
				row.getField(7, conda);
				row.getField(8, condb);

				// Find referenced item
				const auto *itemEntry = m_application.getProject().items.getById(itemId);
				if (!itemEntry)
				{
					ELOG("Could not find referenced item " << itemId << " (referenced in skinning loot entry " << entry << " - group " << groupId << ")");
					return false;
				}

				// Create a new loot entry
				bool created = false;
				if (entry > lastEntry)
				{
					m_application.getProject().skinningLoot.add(entry);
					
					lastEntry = entry;
					lastGroup = groupId;
					groupIndex = 0;
					created = true;
				}

				auto *lootEntry = m_application.getProject().skinningLoot.getById(entry);
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
						WLOG("No unit with entry " << entry << " found - creature skinning loot template will not be assigned!");
					}
					else
					{
						unitEntry->set_skinninglootentry(lootEntry->id());
					}
				}

				// If there are no loot groups yet, create a new one
				if (lootEntry->groups().empty() || groupId > lastGroup)
				{
					lootEntry->add_groups();
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
				def->set_conditiontype(cond);
				def->set_conditionvala(conda);
				def->set_conditionvalb(condb);

				return true;
			};

			// Do import job
			ImportDialog dialog(m_application, std::move(task));
			dialog.exec();
		}
		void ObjectEditor::on_actionImport_Item_Loot_triggered()
		{
			// Counter variables used by the import method
			UInt32 lastEntry = 0;
			UInt32 lastGroup = 0;
			UInt32 groupIndex = 0;

			// Prepare the import task
			ImportTask task;
			task.countQuery = "SELECT COUNT(*) FROM `wowpp_item_loot_template` WHERE `active` != 0;";
			task.selectQuery = "SELECT `entry`, `item`, `ChanceOrQuestChance`, `groupid`, `mincountOrRef`, `maxcount`,`lootcondition`,`condition_value1`,`condition_value2` FROM `wowpp_item_loot_template` WHERE `active` != 0"
				" ORDER BY `entry`, `groupid`;";
			task.beforeImport = [this]() {
				// Remove old object loot
				for (int i = 0; i < m_application.getProject().items.getTemplates().entry_size(); ++i)
				{
					auto *object = m_application.getProject().items.getTemplates().mutable_entry(i);
					object->set_lootentry(0);
				}
				m_application.getProject().itemLoot.clear();
			};
			task.onImport = [this, &lastEntry, &lastGroup, &groupIndex](wowpp::MySQL::Row &row) -> bool {
				UInt32 entry = 0, itemId = 0, groupId = 0, minCount = 0, maxCount = 0, cond = 0, conda = 0, condb = 0;
				float dropChance = 0.0f;
				row.getField(0, entry);
				row.getField(1, itemId);
				row.getField(2, dropChance);
				row.getField(3, groupId);
				row.getField(4, minCount);
				row.getField(5, maxCount);
				row.getField(6, cond);
				row.getField(7, conda);
				row.getField(8, condb);

				// Find referenced item
				const auto *itemEntry = m_application.getProject().items.getById(itemId);
				if (!itemEntry)
				{
					ELOG("Could not find referenced item " << itemId << " (referenced in item loot entry " << entry << " - group " << groupId << ")");
					return false;
				}

				// Create a new loot entry
				bool created = false;
				if (entry > lastEntry)
				{
					m_application.getProject().itemLoot.add(entry);
					
					lastEntry = entry;
					lastGroup = groupId;
					groupIndex = 0;
					created = true;
				}

				auto *lootEntry = m_application.getProject().itemLoot.getById(entry);
				if (!lootEntry)
				{
					// Error
					ELOG("Loot entry " << entry << " found, but no object to assign found");
					return false;
				}

				if (created)
				{
					auto *objectEntry = m_application.getProject().items.getById(entry);
					if (!objectEntry)
					{
						WLOG("No item with entry " << entry << " found - item loot template will not be assigned!");
					}
					else
					{
						objectEntry->set_lootentry(lootEntry->id());
					}
				}

				// If there are no loot groups yet, create a new one
				if (lootEntry->groups().empty() || groupId > lastGroup)
				{
					lootEntry->add_groups();
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
				// We don't need condition value 9 in object loot!
				if (cond == 9)
				{
					cond = 0;
					conda = 0;
					condb = 0;
				}
				def->set_conditiontype(cond);
				def->set_conditionvala(conda);
				def->set_conditionvalb(condb);
				return true;
			};

			// Do import job
			ImportDialog dialog(m_application, std::move(task));
			dialog.exec();
		}
		void ObjectEditor::on_actionImport_Units_triggered()
		{
			ImportTask task;
			task.countQuery = "SELECT COUNT(*) FROM `creature_template`;";
			task.selectQuery = "SELECT `entry`, `resistance1`, `resistance2`, `resistance3`, `resistance4`, `resistance5`, `resistance6`, `mechanic_immune_mask` FROM `creature_template` ORDER BY `entry`;";
			task.beforeImport = [this]() {
				
			};
			task.onImport = [this](wowpp::MySQL::Row &row) -> bool {
				UInt32 entry = 0;

				UInt32 index = 0;
				row.getField(index++, entry);

				// Find creature
				auto *unit = m_application.getProject().units.getById(entry);
				if (!unit)
				{
					ELOG("Skipping unit " << entry << " as it couldn't be found!");
					return true;
				}

				UInt32 schoolImmunity = 0;

				// Import all resistances
				for (int n = 0; n < unit->resistances_size(); ++n)
				{
					Int32 res = 0;
					row.getField(index++, res);

					if (res < 0)
					{
						// Immunity against this school
						schoolImmunity |= (1 << (n+1));
					}
					else
					{
						// Apply resistance
						unit->set_resistances(n, UInt32(res));
					}
				}

				// Update school immunity
				unit->set_schoolimmunity(schoolImmunity);

				// Mechanic immunity
				UInt32 mechanicImmunity = 0;
				row.getField(index++, mechanicImmunity);
				unit->set_mechanicimmunity(mechanicImmunity);

				return true;
			};

			// Do import job
			ImportDialog dialog(m_application, std::move(task));
			dialog.exec();
		}

		void ObjectEditor::on_actionImport_Gold_Loot_triggered()
		{
			ImportTask task;
			task.countQuery = "SELECT COUNT(*) FROM `wowpp_creature_coindrops`;";
			task.selectQuery = "SELECT `Entry`, `min`, `max` FROM `wowpp_creature_coindrops` WHERE `min` > 0 OR `max` > 0 ORDER BY `Entry`;";
			task.beforeImport = [this]() {
				for (int i = 0; i < m_application.getProject().units.getTemplates().entry_size(); ++i)
				{
					auto *unit = m_application.getProject().units.getTemplates().mutable_entry(i);
					unit->clear_minlootgold();
					unit->clear_maxlootgold();
				}
			};
			task.onImport = [this](wowpp::MySQL::Row &row) -> bool {
				UInt32 entry = 0, minGold = 0, maxGold = 0;
				UInt32 index = 0;
				row.getField(index++, entry);
				row.getField(index++, minGold);
				row.getField(index++, maxGold);

				// Find creature
				auto *unit = m_application.getProject().units.getById(entry);
				if (!unit)
				{
					ELOG("Skipping unit " << entry << " as it couldn't be found!");
					return true;
				}

				if (unit->minlootgold() != minGold || unit->maxlootgold() != maxGold)
				{
					unit->set_minlootgold(minGold);
					unit->set_maxlootgold(maxGold);
				}
				return true;
			};

			// Do import job
			ImportDialog dialog(m_application, std::move(task));
			dialog.exec();
		}

		void ObjectEditor::on_actionImport_Items_triggered()
		{
			ImportTask task;
			task.countQuery = "SELECT COUNT(*) FROM `item_template`;";
			task.selectQuery = "SELECT `entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyCount`, `BuyPrice`,"
				"`SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`,"
				"`RequiredLevel`, `RequiredSkill`, `RequiredSkillRank`, `requiredspell`, `requiredhonorrank`,"
				"`RequiredCityRank`, `RequiredReputationFaction`, `RequiredReputationRank`, `maxcount`, `stackable`,"
				"`ContainerSlots`, `stat_type1`, `stat_value1`, `stat_type2`, `stat_value2`,"
				"`stat_type3`, `stat_value3`, `stat_type4`, `stat_value4`, `stat_type5`,"
				"`stat_value5`, `stat_type6`, `stat_value6`, `stat_type7`, `stat_value7`,"
				"`stat_type8`, `stat_value8`, `stat_type9`, `stat_value9`, `stat_type10`,"
				"`stat_value10`, `dmg_min1`, `dmg_max1`, `dmg_type1`, `dmg_min2`,"
				"`dmg_max2`, `dmg_type2`, `dmg_min3`, `dmg_max3`, `dmg_type3`,"
				"`dmg_min4`, `dmg_max4`, `dmg_type4`, `dmg_min5`, `dmg_max5`,"
				"`dmg_type5`, `armor`, `holy_res`, `fire_res`, `nature_res`,"
				"`frost_res`, `shadow_res`, `arcane_res`, `delay`, `ammo_type`,"
				"`RangedModRange`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `spellppmRate_1`,"
				"`spellcooldown_1`, `spellcategory_1`, `spellcategorycooldown_1`,"
				"`spellid_2`, `spelltrigger_2`, `spellcharges_2`, `spellppmRate_2`,"
				"`spellcooldown_2`, `spellcategory_2`, `spellcategorycooldown_2`,"
				"`spellid_3`, `spelltrigger_3`, `spellcharges_3`, `spellppmRate_3`,"
				"`spellcooldown_3`, `spellcategory_3`, `spellcategorycooldown_3`,"
				"`spellid_4`, `spelltrigger_4`, `spellcharges_4`, `spellppmRate_4`,"
				"`spellcooldown_4`, `spellcategory_4`, `spellcategorycooldown_4`,"
				"`spellid_5`, `spelltrigger_5`, `spellcharges_5`, `spellppmRate_5`,"
				"`spellcooldown_5`, `spellcategory_5`, `spellcategorycooldown_5`,"
				"`bonding`, `description`, `PageText`, `LanguageID`, `PageMaterial`,"
				"`startquest`, `lockid`, `Material`, `sheath`, `RandomProperty`,"
				"`RandomSuffix`, `block`, `itemset`, `MaxDurability`, `area`,"
				"`Map`, `BagFamily`, `TotemCategory`, `socketColor_1`, `socketContent_1`,"
				"`socketColor_2`, `socketContent_2`, `socketColor_3`, `socketContent_3`,"
				"`socketBonus`, `GemProperties`, `RequiredDisenchantSkill`, `ArmorDamageModifier`, `ScriptName`,"
				"`DisenchantID`, `FoodType`, `minMoneyLoot`, `maxMoneyLoot`, `Duration`, `ExtraFlags`"
				" FROM `item_template`;";
			task.beforeImport = [this]() {
			};
			task.onImport = [this](wowpp::MySQL::Row &row) -> bool {
				UInt32 entry = 0;
				UInt32 index = 0;
				row.getField(index++, entry);

				// Find item by entry
				auto *itemEntry = m_application.getProject().items.getById(entry);
				if (!itemEntry)
				{
					// Item doesn't exist yet
					itemEntry = m_application.getProject().items.add(entry);
					ILOG("New item added: " << entry);
				}

				Int32 iTmp = 0;
				String sVal;

#define WOWPP_IMPORT_INT_VAL(name) \
				iTmp = 0; \
				row.getField(index++, iTmp); \
				itemEntry->set_##name(iTmp);
#define WOWPP_IMPORT_STR_VAL(name) \
				sVal.clear(); \
				row.getField(index++, sVal); \
				itemEntry->set_##name(sVal);

				WOWPP_IMPORT_INT_VAL(itemclass);
				WOWPP_IMPORT_INT_VAL(subclass);
				WOWPP_IMPORT_STR_VAL(name);
				WOWPP_IMPORT_INT_VAL(displayid);
				WOWPP_IMPORT_INT_VAL(quality);
				WOWPP_IMPORT_INT_VAL(flags);
				WOWPP_IMPORT_INT_VAL(buycount);
				WOWPP_IMPORT_INT_VAL(buyprice);
				WOWPP_IMPORT_INT_VAL(sellprice);
				WOWPP_IMPORT_INT_VAL(inventorytype);
				WOWPP_IMPORT_INT_VAL(allowedclasses);
				WOWPP_IMPORT_INT_VAL(allowedraces);
				WOWPP_IMPORT_INT_VAL(itemlevel);
				WOWPP_IMPORT_INT_VAL(requiredlevel);
				WOWPP_IMPORT_INT_VAL(requiredskill);
				WOWPP_IMPORT_INT_VAL(requiredskillrank);
				WOWPP_IMPORT_INT_VAL(requiredspell);
				WOWPP_IMPORT_INT_VAL(requiredhonorrank);
				WOWPP_IMPORT_INT_VAL(requiredcityrank);
				WOWPP_IMPORT_INT_VAL(requiredrep);
				WOWPP_IMPORT_INT_VAL(requiredreprank);
				WOWPP_IMPORT_INT_VAL(maxcount);
				WOWPP_IMPORT_INT_VAL(maxstack);
				WOWPP_IMPORT_INT_VAL(containerslots);
				index += 2 * 10;	// Stats
				index += 3 * 5;		// Damage
				WOWPP_IMPORT_INT_VAL(armor);
				WOWPP_IMPORT_INT_VAL(holyres);
				WOWPP_IMPORT_INT_VAL(fireres);
				WOWPP_IMPORT_INT_VAL(natureres);
				WOWPP_IMPORT_INT_VAL(frostres);
				WOWPP_IMPORT_INT_VAL(shadowres);
				WOWPP_IMPORT_INT_VAL(arcaneres);
				WOWPP_IMPORT_INT_VAL(delay);
				WOWPP_IMPORT_INT_VAL(ammotype);
				index++;			// RangedModRange
				index += 7 * 5;		// Spells
				WOWPP_IMPORT_INT_VAL(bonding);
				WOWPP_IMPORT_STR_VAL(description);
				index++;			// PageText
				index++;			// LanguageID
				index++;			// PageMaterial
				WOWPP_IMPORT_INT_VAL(questentry);
				WOWPP_IMPORT_INT_VAL(lockid);
				WOWPP_IMPORT_INT_VAL(material);
				WOWPP_IMPORT_INT_VAL(sheath);
				WOWPP_IMPORT_INT_VAL(randomproperty);
				WOWPP_IMPORT_INT_VAL(randomsuffix);
				WOWPP_IMPORT_INT_VAL(block);
				WOWPP_IMPORT_INT_VAL(itemset);
				WOWPP_IMPORT_INT_VAL(durability);
				WOWPP_IMPORT_INT_VAL(area);
				WOWPP_IMPORT_INT_VAL(map);
				WOWPP_IMPORT_INT_VAL(bagfamily);
				WOWPP_IMPORT_INT_VAL(totemcategory);
				index += 2 * 3;		// Sockets
				WOWPP_IMPORT_INT_VAL(socketbonus);
				WOWPP_IMPORT_INT_VAL(gemproperties);
				WOWPP_IMPORT_INT_VAL(disenchantskillval);
				index++;			// ArmorDamageModified (float)
				index++;			// ScriptName
				WOWPP_IMPORT_INT_VAL(disenchantid);
				WOWPP_IMPORT_INT_VAL(foodtype);
				WOWPP_IMPORT_INT_VAL(minlootgold);
				WOWPP_IMPORT_INT_VAL(maxlootgold);
				WOWPP_IMPORT_INT_VAL(duration);
				WOWPP_IMPORT_INT_VAL(extraflags);

#undef WOWPP_IMPORT_STR_VAL
#undef WOWPP_IMPORT_INT_VAL

				return true;
			};
			task.afterImport = [this]() {
				emit m_application.getItemListModel()->dataChanged(QModelIndex(), QModelIndex());
			};

			// Do import job
			ImportDialog dialog(m_application, std::move(task));
			dialog.exec();
		}

		void ObjectEditor::on_actionImport_Object_Spawns_triggered()
		{
			ImportTask task;
			task.countQuery = "SELECT COUNT(*) FROM `wowpp_gameobject` WHERE `active` != 0;";
			task.selectQuery = "SELECT `id`, `map`, `position_x`, `position_y`, `position_z`, `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`, `state`, `name` FROM `wowpp_gameobject`  WHERE `active` != 0 ORDER BY `id`;";
			task.beforeImport = [this]() {
				for (int i = 0; i < m_application.getProject().maps.getTemplates().entry_size(); ++i)
				{
					auto *map = m_application.getProject().maps.getTemplates().mutable_entry(i);
					map->clear_objectspawns();
				}
			};
			task.onImport = [this](wowpp::MySQL::Row &row) -> bool {
				UInt32 entry = 0, map = 0, animprogress = 0, state = 0;
				UInt64 spawnTime = 0;
				float x = 0.0f, y = 0.0f, z = 0.0f, o = 0.0f, rot0 = 0.0f, rot1 = 0.0f, rot2 = 0.0f, rot3 = 0.0f;
				String name;

				UInt32 index = 0;
				row.getField(index++, entry);
				row.getField(index++, map);
				row.getField(index++, x);
				row.getField(index++, y);
				row.getField(index++, z);
				row.getField(index++, o);
				row.getField(index++, rot0);
				row.getField(index++, rot1);
				row.getField(index++, rot2);
				row.getField(index++, rot3);
				row.getField(index++, spawnTime);
				row.getField(index++, animprogress);
				row.getField(index++, state);
				row.getField(index++, name);
				spawnTime *= constants::OneSecond;	// Time in Milliseconds

				// Find referenced map
				auto *mapEntry = m_application.getProject().maps.getById(map);
				if (!mapEntry)
				{
					ELOG("Could not find referenced map " << map << " (referenced in object spawn entry " << entry << ")");
					return false;
				}

				if (!m_application.getProject().objects.getById(entry))
				{
					ELOG("Could not find object template by entry: " << entry);
					return false;
				}

				// Create new object spawn
				auto *added = mapEntry->add_objectspawns();
				if (!added)
				{
					ELOG("Could not add object spawn");
					return false;
				}

				if (!name.empty()) added->set_name(name.c_str());
				added->set_objectentry(entry);
				added->set_isactive(true);
				added->set_animprogress(animprogress);
				added->set_respawn(true);
				added->set_respawndelay(spawnTime);
				added->set_positionx(x);
				added->set_positiony(y);
				added->set_positionz(z);
				added->set_orientation(o);
				added->set_rotationw(rot0);
				added->set_rotationx(rot1);
				added->set_rotationy(rot2);
				added->set_rotationz(rot3);
				added->set_state(state);
				added->set_maxcount(1);
				added->set_radius(0.0f);

				return true;
			};

			// Do import job
			ImportDialog dialog(m_application, std::move(task));
			dialog.exec();
		}

		void ObjectEditor::on_addUnitSpellButton_clicked()
		{
			if (!m_selectedUnit)
				return;

			CreatureSpellDialog dialog(m_application);
			auto result = dialog.exec();
			if (result == QDialog::Accepted)
			{
				if (!dialog.getSelectedSpell())
				{
					return;
				}

				auto *added = m_selectedUnit->add_creaturespells();
				added->set_spellid(dialog.getSelectedSpell()->id());
				added->set_priority(dialog.getPriority());
				added->set_mincooldown(dialog.getMinCooldown());
				added->set_maxcooldown(dialog.getMaxCooldown());
				added->set_mininitialcooldown(dialog.getMinInitialCooldown());
				added->set_maxinitialcooldown(dialog.getMaxInitialCooldown());
				added->set_target(dialog.getTarget());
				added->set_repeated(dialog.getRepeated());
				added->set_probability(dialog.getProbability());
				addSpellEntry(*added);
			}
		}
		void ObjectEditor::on_removeUnitSpellButton_clicked()
		{
			if (!m_selectedUnit)
				return;

			auto rows = m_ui->treeWidget->selectionModel()->selectedRows(0);
			if (!rows.empty())
			{
				for (auto &index : rows)
				{
					// Erase spell entry
					m_selectedUnit->mutable_creaturespells()->erase(
						m_selectedUnit->mutable_creaturespells()->begin() + index.row());
				}
			}

			for (auto &item : m_ui->treeWidget->selectedItems())
			{
				delete item;
			}
		}
		void ObjectEditor::on_effectButton1_clicked()
		{
			if (!m_selectedSpell)
				return;

			if (m_selectedSpell->effects_size() > 0)
				showEffectDialog(m_selectedSpell->effects(0));
		}
		void ObjectEditor::on_effectButton2_clicked()
		{
			if (!m_selectedSpell)
				return;

			if (m_selectedSpell->effects_size() > 1)
				showEffectDialog(m_selectedSpell->effects(1));
		}
		void ObjectEditor::on_effectButton3_clicked()
		{
			if (!m_selectedSpell)
				return;

			if (m_selectedSpell->effects_size() > 2)
				showEffectDialog(m_selectedSpell->effects(2));
		}
	}
}
