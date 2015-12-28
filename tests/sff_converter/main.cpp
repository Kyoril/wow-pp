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

#include <iostream>
#include <fstream>
#include <string>
#include "common/typedefs.h"
#include "common/clock.h"
#include "log/default_log.h"
#include "log/log_std_stream.h"
#include "log/default_log_levels.h"
#include "simple_file_format/sff_write_file.h"
#include "simple_file_format/sff_write_array.h"
#include "simple_file_format/sff_write_array.h"
#include "simple_file_format/sff_load_file.h"
#include "simple_file_format/sff_write.h"
#include "simple_file_format/sff_read_tree.h"
#include "simple_file_format/sff_save_file.h"
#include "data/project.h"
#include "mysql_wrapper/mysql_connection.h"
#include "mysql_wrapper/mysql_row.h"
#include "mysql_wrapper/mysql_select.h"
#include "mysql_wrapper/mysql_statement.h"
#include "common/make_unique.h"
#include "common/linear_set.h"
#include "virtual_directory/file_system_reader.h"
#include "proto_data/project_loader.h"
#include "proto_data/project_saver.h"
#include "proto_data/project.h"

/// Procedural entry point of the application.
int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// Add cout to the list of log output streams
	wowpp::g_DefaultLog.signal().connect(std::bind(
		wowpp::printLogEntry,
		std::ref(std::cout), std::placeholders::_1, wowpp::g_DefaultConsoleLogOptions));
	
	// Create a new proto project
	wowpp::proto::Project protoProject;

#if 0
	// Load existing project
	if (!protoProject.load("./"))
	{
		return 1;
	}
#else
	// Load old project file
	wowpp::Project proj;
	if (!proj.load("C:/Source/wowpp-data"))
	{
		return 1;
	}

	// Copy all data from the old text-based project to our new protobuf binary project

	// Copy units
	for (const auto &unit : proj.units.getTemplates())
	{
		auto *added = protoProject.units.add(unit->id);
		added->set_name(unit->name);
		if (!unit->subname.empty()) added->set_subname(unit->subname);
		
		added->set_minlevel(unit->minLevel);
		added->set_maxlevel(unit->maxLevel);
		added->set_alliancefaction(unit->allianceFaction->id);
		added->set_hordefaction(unit->hordeFaction->id);
		added->set_malemodel(unit->maleModel);
		added->set_femalemodel(unit->femaleModel);
		added->set_scale(unit->scale);
		added->set_type(unit->type);
		added->set_family(unit->family);
		added->set_regeneratehealth(unit->regeneratesHealth);
		added->set_npcflags(unit->npcFlags);
		added->set_unitflags(unit->unitFlags);
		added->set_dynamicflags(unit->dynamicFlags);
		added->set_extraflags(unit->extraFlags);
		added->set_creaturetypeflags(unit->creatureTypeFlags);
		added->set_walkspeed(unit->walkSpeed);
		added->set_runspeed(unit->runSpeed);
		added->set_unitclass(unit->unitClass);
		added->set_rank(unit->rank);
		added->set_minlevelhealth(unit->minLevelHealth);
		added->set_maxlevelhealth(unit->maxLevelHealth);
		added->set_minlevelmana(unit->minLevelMana);
		added->set_maxlevelmana(unit->maxLevelMana);
		added->set_minmeleedmg(unit->minMeleeDamage);
		added->set_maxmeleedmg(unit->maxMeleeDamage);
		added->set_minrangeddmg(unit->minRangedDamage);
		added->set_maxrangeddmg(unit->maxRangedDamage);
		added->set_armor(unit->armor);
		for (auto &res : unit->resistances)
		{
			added->add_resistances(res);
		}
		added->set_meleeattacktime(unit->meleeBaseAttackTime);
		added->set_rangedattacktime(unit->rangedBaseAttackTime);
		added->set_damageschool(unit->damageSchool);
		added->set_minlootgold(unit->minLootGold);
		added->set_maxlootgold(unit->maxLootGold);
		added->set_minlevelxp(unit->xpMin);
		added->set_maxlevelxp(unit->xpMax);
		for (auto &trigger : unit->triggers)
		{
			added->add_triggers(trigger->id);
		}
		if (unit->mainHand != nullptr) added->set_mainhandweapon(unit->mainHand->id);
		if (unit->offHand != nullptr) added->set_offhandweapon(unit->offHand->id);
		if (unit->ranged != nullptr) added->set_rangedweapon(unit->ranged->id);
		added->set_attackpower(unit->attackPower);
		added->set_rangedattackpower(unit->rangedAttackPower);
		if (unit->unitLootEntry != nullptr) added->set_unitlootentry(unit->unitLootEntry->id);
		if (unit->vendorEntry != nullptr) added->set_vendorentry(unit->vendorEntry->id);
		if (unit->trainerEntry != nullptr) added->set_trainerentry(unit->trainerEntry->id);
	}

	// Copy unit loot
	for (const auto &loot : proj.unitLoot.getTemplates())
	{
		auto *added = protoProject.unitLoot.add(loot->id);
		for (auto &group : loot->lootGroups)
		{
			auto *addedGroup = added->add_groups();

			for (auto &def : group)
			{
				auto *addedDef = addedGroup->add_definitions();
				addedDef->set_item(def.item->id);
				addedDef->set_mincount(def.minCount);
				addedDef->set_maxcount(def.maxCount);
				addedDef->set_dropchance(def.dropChance);
				if (!def.isActive) addedDef->set_isactive(def.isActive);
			}
		}
	}

	// Copy emotes
	for (const auto &emote : proj.emotes.getTemplates())
	{
		auto *added = protoProject.emotes.add(emote->id);
		added->set_name(emote->name);
		added->set_textid(emote->textId);
	}

	// Copy objects
	for (const auto &object : proj.objects.getTemplates())
	{
		auto *added = protoProject.objects.add(object->id);
		added->set_id(object->id);
		added->set_name(object->name);
		if (!object->caption.empty()) added->set_caption(object->caption);

		added->set_mingold(object->minGold);
		added->set_maxgold(object->maxGold);
		added->set_type(object->type);
		added->set_displayid(object->displayID);
		added->set_factionid(object->factionID);
		added->set_scale(object->scale);
		added->set_flags(object->flags);
		for (auto &data : object->data)
		{
			added->add_data(data);
		}
	}

	// Copy maps
	for (const auto &map : proj.maps.getTemplates())
	{
		auto *added = protoProject.maps.add(map->id);
		added->set_name(map->name);
		added->set_directory(map->directory);
		added->set_instancetype(static_cast<wowpp::proto::MapEntry_MapInstanceType>(map->instanceType));
		for (auto &unitSpawn : map->spawns)
		{
			auto *addedSpawn = added->add_unitspawns();
			if (!unitSpawn.name.empty()) addedSpawn->set_name(unitSpawn.name);
			addedSpawn->set_respawn(unitSpawn.respawn);
			addedSpawn->set_respawndelay(unitSpawn.respawnDelay);
			addedSpawn->set_positionx(unitSpawn.position[0]);
			addedSpawn->set_positiony(unitSpawn.position[1]);
			addedSpawn->set_positionz(unitSpawn.position[2]);
			addedSpawn->set_rotation(unitSpawn.rotation);
			addedSpawn->set_radius(unitSpawn.radius);
			addedSpawn->set_maxcount(unitSpawn.maxCount);
			if (unitSpawn.unit != nullptr) addedSpawn->set_unitentry(unitSpawn.unit->id);
			addedSpawn->set_defaultemote(unitSpawn.emote);
			addedSpawn->set_isactive(unitSpawn.active);
		}
		for (auto &objSpawn : map->objectSpawns)
		{
			auto *addedSpawn = added->add_objectspawns();
			if (!objSpawn.name.empty()) addedSpawn->set_name(objSpawn.name);
			addedSpawn->set_respawn(objSpawn.respawn);
			addedSpawn->set_respawndelay(objSpawn.respawnDelay);
			addedSpawn->set_positionx(objSpawn.position[0]);
			addedSpawn->set_positiony(objSpawn.position[1]);
			addedSpawn->set_positionz(objSpawn.position[2]);
			addedSpawn->set_rotationw(objSpawn.rotation[0]);
			addedSpawn->set_rotationx(objSpawn.rotation[1]);
			addedSpawn->set_rotationy(objSpawn.rotation[2]);
			addedSpawn->set_rotationz(objSpawn.rotation[3]);
			addedSpawn->set_radius(objSpawn.radius);
			addedSpawn->set_maxcount(objSpawn.maxCount);
			if (objSpawn.object != nullptr) addedSpawn->set_objectentry(objSpawn.object->id);
			addedSpawn->set_animprogress(objSpawn.animProgress);
			addedSpawn->set_state(objSpawn.state);
		}
	}

	// Copy spells
	for (const auto &spell : proj.spells.getTemplates())
	{
		auto *added = protoProject.spells.add(spell->id);
		added->set_name(spell->name);

		added->add_attributes(spell->attributes);
		for (auto &attr : spell->attributesEx)
		{
			added->add_attributes(attr);
		}
		wowpp::UInt32 effIndex = 0;
		for (auto &eff : spell->effects)
		{
			auto addedEff = added->add_effects();
			addedEff->set_index(effIndex++);
			addedEff->set_type(eff.type);
			addedEff->set_basepoints(eff.basePoints);
			addedEff->set_diesides(eff.dieSides);
			addedEff->set_basedice(eff.baseDice);
			addedEff->set_diceperlevel(eff.dicePerLevel);
			addedEff->set_pointsperlevel(eff.pointsPerLevel);
			addedEff->set_mechanic(eff.mechanic);
			addedEff->set_targeta(eff.targetA);
			addedEff->set_targetb(eff.targetB);
			addedEff->set_radius(eff.radius);
			addedEff->set_aura(eff.auraName);
			addedEff->set_amplitude(eff.amplitude);
			addedEff->set_multiplevalue(eff.multipleValue);
			addedEff->set_chaintarget(eff.chainTarget);
			addedEff->set_itemtype(eff.itemType);
			addedEff->set_miscvaluea(eff.miscValueA);
			addedEff->set_miscvalueb(eff.miscValueB);
			if (eff.triggerSpell != nullptr) addedEff->set_triggerspell(eff.triggerSpell->id);
			if (eff.summonEntry != nullptr) addedEff->set_summonunit(eff.summonEntry->id);
			addedEff->set_pointspercombopoint(eff.pointsPerComboPoint);
		}
		added->set_cooldown(spell->cooldown);

		wowpp::UInt32 castTimeMS = 0;
		const auto *castTime = proj.castTimes.getById(spell->castTimeIndex);
		if (castTime)
		{
			castTimeMS = static_cast<wowpp::UInt32>(castTime->castTime);
		}
		added->set_casttime(castTimeMS);
		added->set_powertype(spell->powerType);
		added->set_cost(spell->cost);
		added->set_costpct(spell->costPct);
		added->set_maxlevel(spell->maxLevel);
		added->set_baselevel(spell->baseLevel);
		added->set_spelllevel(spell->spellLevel);
		added->set_speed(spell->speed);
		added->set_schoolmask(spell->schoolMask);
		added->set_dmgclass(spell->dmgClass);
		added->set_itemclass(spell->itemClass);
		added->set_itemsubclassmask(spell->itemSubClassMask);
		for (auto &skill : spell->skillsOnLearnSpell)
		{
			added->add_skillsonlearnspell(skill->id);
		}
		added->set_facing(spell->facing);
		added->set_duration(spell->duration);
		added->set_maxduration(spell->maxDuration);
		added->set_interruptflags(spell->interruptFlags);
		added->set_aurainterruptflags(spell->auraInterruptFlags);
		added->set_minrange(spell->minRange);
		added->set_maxrange(spell->maxRange);
		added->set_rangetype(spell->rangeType);
		added->set_targetmap(spell->targetMap);
		added->set_targetx(spell->targetX);
		added->set_targety(spell->targetY);
		added->set_targetz(spell->targetZ);
		added->set_targeto(spell->targetO);
		added->set_maxtargets(spell->maxTargets);
		added->set_talentcost(spell->talentCost);
		added->set_procflags(spell->procFlags);
		added->set_procchance(spell->procChance);
		added->set_proccharges(spell->procCharges);
	}

	// Copy triggers
	for (const auto &trigger : proj.triggers.getTemplates())
	{
		auto *added = protoProject.triggers.add(trigger->id);
		added->set_name(trigger->name);
		if (!trigger->path.empty()) added->set_category(trigger->path);

		for (auto &e : trigger->events)
		{
			added->add_events(e);
		}
		for (auto &a : trigger->actions)
		{
			auto *addedAction = added->add_actions();
			addedAction->set_action(a.action);
			addedAction->set_target(a.target);
			addedAction->set_targetname(a.targetName);
			for (auto &t : a.texts)
			{
				addedAction->add_texts(t);
			}
			for (auto &d : a.data)
			{
				addedAction->add_data(d);
			}
		}
	}

	// Copy talents
	for (const auto &talent : proj.talents.getTemplates())
	{
		auto *added = protoProject.talents.add(talent->id);
		added->set_tab(talent->tab);
		added->set_row(talent->row);
		added->set_column(talent->column);
		for (auto &rank : talent->ranks)
		{
			added->add_ranks(rank->id);
		}
		if (talent->dependsOn) added->set_dependson(talent->dependsOn->id);
		added->set_dependsonrank(talent->dependsOnRank);
		if(talent->dependsOnSpell) added->set_dependsonspell(talent->dependsOnSpell->id);
	}

	// Copy skills
	for (const auto &skill : proj.skills.getTemplates())
	{
		auto *added = protoProject.skills.add(skill->id);
		added->set_name(skill->name);
		added->set_category(skill->category);
		added->set_cost(skill->cost);
	}

	// Copy levels
	for (const auto &level : proj.levels.getTemplates())
	{
		auto *added = protoProject.levels.add(level->id);
		added->set_nextlevelxp(level->nextLevelXP);
		added->set_explorationbasexp(level->explorationBaseXP);
		auto &map = *added->mutable_stats();
		for (auto &pair : level->stats)
		{
			auto &map2 = *map[pair.first].mutable_stats();
			for (auto &pair2 : pair.second)
			{
				map2[pair2.first].set_stat1(pair2.second[0]);
				map2[pair2.first].set_stat2(pair2.second[1]);
				map2[pair2.first].set_stat3(pair2.second[2]);
				map2[pair2.first].set_stat4(pair2.second[3]);
				map2[pair2.first].set_stat5(pair2.second[4]);

				auto it = level->regen.find(pair2.first);
				if (it != level->regen.end())
				{
					map2[pair2.first].set_regenhp(it->second[0]);
					map2[pair2.first].set_regenmp(it->second[1]);
				}
			}
		}
	}

	// Copy classes
	for (const auto &class_ : proj.classes.getTemplates())
	{
		auto *added = protoProject.classes.add(class_->id);
		added->set_name(class_->name);
		added->set_internalname(class_->internalName);
		added->set_powertype(static_cast<wowpp::proto::ClassEntry_PowerType>(class_->powerType));
		added->set_spellfamily(class_->spellFamily);
		added->set_flags(class_->flags);
		for (wowpp::UInt32 level = 1; level <= 70; ++level)
		{
			auto *addedLevel = added->add_levelbasevalues();
			const auto &it = class_->levelBaseValues.find(level);
			if (it != class_->levelBaseValues.end())
			{
				addedLevel->set_health(it->second.health);
				addedLevel->set_mana(it->second.mana);
			}
		}
	}

	// Copy races
	for (const auto &race : proj.races.getTemplates())
	{
		auto *added = protoProject.races.add(race->id);
		added->set_name(race->name);
		if (race->factionTemplate) added->set_faction(race->factionTemplate->id);
		added->set_malemodel(race->maleModel);
		added->set_femalemodel(race->femaleModel);
		added->set_baselanguage(race->baseLanguage);
		added->set_startingtaximask(race->startingTaxiMask);
		added->set_cinematic(race->cinematic);
		for (const auto &pair : race->initialSpells)
		{
			auto &map = *added->mutable_initialspells();
			for (const auto &spell : pair.second)
			{
				map[pair.first].add_spells(spell->id);
			}
		}
		for (const auto &pair : race->initialActionButtons)
		{
			auto &map = *added->mutable_initialactionbuttons();
			auto &map2 = *map[pair.first].mutable_actionbuttons();
			for (const auto &pair2 : pair.second)
			{
				map2[pair2.first].set_action(pair2.second.action);
				map2[pair2.first].set_misc(pair2.second.misc);
				map2[pair2.first].set_state(
					static_cast<wowpp::proto::ActionButton_ActionButtonUpdateState>(pair2.second.state));
				map2[pair2.first].set_type(pair2.second.type);
			}
		}
		for (const auto &pair : race->initialItems)
		{
			auto &map = *added->mutable_initialitems();
			if (!pair.second.empty())
			{
				const auto &items = pair.second.find(0);
				for (const auto &item : items->second)
				{
					map[pair.first].add_items(item->id);
				}
			}
		}
		added->set_startmap(race->startMap);
		added->set_startzone(race->startZone);
		added->set_startposx(race->startPosition[0]);
		added->set_startposy(race->startPosition[1]);
		added->set_startposz(race->startPosition[2]);
		added->set_startrotation(race->startRotation);
	}

	// Copy trainers
	for (const auto &trainer : proj.trainers.getTemplates())
	{
		auto *added = protoProject.trainers.add(trainer->id);
		added->set_id(trainer->id);
		added->set_type(static_cast<wowpp::proto::TrainerEntry_TrainerType>(trainer->trainerType));
		added->set_classid(trainer->classId);
		added->set_title(trainer->title);
		for (const auto &spell : trainer->spells)
		{
			auto *addedSpell = added->add_spells();
			addedSpell->set_spell(spell.spell->id);
			addedSpell->set_spellcost(spell.spellCost);
			if (spell.reqSkill != nullptr) addedSpell->set_reqskill(spell.reqSkill->id);
			addedSpell->set_reqskillval(spell.reqSkillValue);
			addedSpell->set_reqlevel(spell.reqLevel);
		}
	}

	// Copy vendors
	for (const auto &vendor : proj.vendors.getTemplates())
	{
		auto *added = protoProject.vendors.add(vendor->id);
		for (const auto &item : vendor->items)
		{
			auto *addedItem = added->add_items();
			addedItem->set_item(item.item->id);
			addedItem->set_maxcount(item.maxCount);
			addedItem->set_interval(item.interval);
			addedItem->set_extendedcost(item.extendedCost);
			addedItem->set_isactive(item.isActive);
		}
	}

	// Copy quests
	for (const auto &quest : proj.quests.getTemplates())
	{
		auto *added = protoProject.quests.add(quest->id);
		added->set_name(quest->name);

		// TODO
	}

	// Copy factions
	for (const auto &faction : proj.factions.getTemplates())
	{
		auto *added = protoProject.factions.add(faction->id);
		added->set_name(faction->name);
		added->set_replistid(faction->repListId);
		for (const auto &baseRep : faction->baseReputation)
		{
			auto *addedRep = added->add_baserep();
			addedRep->set_racemask(baseRep.raceMask);
			addedRep->set_classmask(baseRep.classMask);
			addedRep->set_value(baseRep.value);
			addedRep->set_flags(baseRep.flags);
		}
	}

	// Copy faction templates
	for (const auto &faction : proj.factionTemplates.getTemplates())
	{
		auto *added = protoProject.factionTemplates.add(faction->id);
		added->set_flags(faction->flags);
		added->set_selfmask(faction->selfMask);
		added->set_friendmask(faction->friendMask);
		added->set_enemymask(faction->enemyMask);
		if (faction->faction) added->set_faction(faction->faction->id);

		for (const auto &f : faction->friends)
		{
			added->add_friends(f->id);
		}
		for (const auto &e : faction->enemies)
		{
			added->add_enemies(e->id);
		}
	}

	// Copy items
	for (const auto &item : proj.items.getTemplates())
	{
		auto *added = protoProject.items.add(item->id);
		added->set_name(item->name);
		added->set_description(item->description);
		added->set_itemclass(item->itemClass);
		added->set_subclass(item->subClass);
		added->set_displayid(item->displayId);
		added->set_quality(item->quality);
		added->set_flags(item->flags);
		added->set_buycount(item->buyCount);
		added->set_buyprice(item->buyPrice);
		added->set_sellprice(item->sellPrice);
		added->set_inventorytype(item->inventoryType);
		added->set_allowedclasses(item->allowedClasses);
		added->set_allowedraces(item->allowedRaces);
		added->set_itemlevel(item->itemLevel);
		added->set_requiredlevel(item->requiredLevel);
		added->set_requiredskill(item->requiredSkill);
		added->set_requiredskillrank(item->requiredSkillRank);
		added->set_requiredspell(item->requiredSpell);
		added->set_requiredhonorrank(item->requiredHonorRank);
		added->set_requiredcityrank(item->requiredCityRank);
		added->set_requiredrep(item->requiredReputation);
		added->set_requiredreprank(item->requiredReputationRank);
		added->set_maxcount(item->maxCount);
		added->set_maxstack(item->maxStack);
		added->set_containerslots(item->containerSlots);
		for (const auto &stat : item->itemStats)
		{
			auto *addedStat = added->add_stats();
			addedStat->set_type(stat.statType);
			addedStat->set_value(stat.statValue);
		}
		for (const auto &dmg : item->itemDamage)
		{
			auto *addedDmg = added->add_damage();
			addedDmg->set_mindmg(dmg.min);
			addedDmg->set_maxdmg(dmg.max);
			addedDmg->set_type(dmg.type);
		}
		added->set_armor(item->armor);
		added->set_holyres(item->holyResistance);
		added->set_fireres(item->fireResistance);
		added->set_natureres(item->natureResistance);
		added->set_frostres(item->frostResistance);
		added->set_shadowres(item->shadowResistance);
		added->set_arcaneres(item->arcaneResistance);
		added->set_delay(item->delay);
		added->set_ammotype(item->ammoType);
		for (const auto &spell : item->itemSpells)
		{
			if (spell.spell != nullptr)
			{
				auto *addedSpell = added->add_spells();
				addedSpell->set_spell(spell.spell->id);
				addedSpell->set_trigger(spell.trigger);
				addedSpell->set_category(spell.category);
				addedSpell->set_categorycooldown(spell.categoryCooldown);
				addedSpell->set_charges(spell.charges);
				addedSpell->set_cooldown(spell.cooldown);
				addedSpell->set_procrate(spell.procRate);
			}
		}
		added->set_bonding(item->bonding);
		added->set_lockid(item->lockId);
		added->set_sheath(item->sheath);
		added->set_randomproperty(item->randomProperty);
		added->set_randomsuffix(item->randomSuffix);
		added->set_block(item->block);
		added->set_itemset(item->set);
		added->set_durability(item->durability);
		added->set_area(item->area);
		added->set_map(item->map);
		added->set_bagfamily(item->bagFamily);
		added->set_material(item->material);
		added->set_totemcategory(item->totemCategory);
		for (const auto &socket : item->itemSockets)
		{
			auto *addedSocket = added->add_sockets();
			addedSocket->set_color(socket.color);
			addedSocket->set_content(socket.content);
		}
		added->set_socketbonus(item->socketBonus);
		added->set_gemproperties(item->gemProperties);
		added->set_disenchantskillval(item->requiredDisenchantSkill);
		added->set_disenchantid(item->disenchantId);
		added->set_foodtype(item->foodType);
		added->set_minlootgold(item->minLootGold);
		added->set_maxlootgold(item->maxLootGold);
		added->set_durability(item->duration);
		added->set_extraflags(item->extraFlags);
	}

	// Copy zones
	for (const auto &zone : proj.zones.getTemplates())
	{
		auto *added = protoProject.zones.add(zone->id);
		added->set_name(zone->name);
		if (zone->parent != nullptr) added->set_parentzone(zone->parent->id);
		if (zone->map != nullptr) added->set_map(zone->map->id);
		added->set_explore(zone->explore);
		added->set_flags(zone->flags);
		added->set_team(zone->team);
		added->set_level(zone->level);
	}

	// Copy area triggerrs
	for (const auto &areaTrigger : proj.areaTriggers.getTemplates())
	{
		auto *added = protoProject.areaTriggers.add(areaTrigger->id);
		added->set_name(areaTrigger->name);
		added->set_map(areaTrigger->map);
		added->set_x(areaTrigger->x);
		added->set_y(areaTrigger->y);
		added->set_z(areaTrigger->z);
		added->set_radius(areaTrigger->radius);
		added->set_box_x(areaTrigger->box_x);
		added->set_box_y(areaTrigger->box_y);
		added->set_box_z(areaTrigger->box_z);
		added->set_box_o(areaTrigger->box_o);
		added->set_targetmap(areaTrigger->targetMap);
		added->set_target_x(areaTrigger->targetX);
		added->set_target_y(areaTrigger->targetY);
		added->set_target_z(areaTrigger->targetZ);
		added->set_target_o(areaTrigger->targetO);
	}

	// Save proto project
	if (!protoProject.save("./"))
	{
		return 1;
	}
#endif

	// Wait for user input to finish
	std::cin.get();

	// Shutdown protobuf and free all memory (optional)
	google::protobuf::ShutdownProtobufLibrary();

	return 0;
}
