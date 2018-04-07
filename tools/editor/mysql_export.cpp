#include "pch.h"
#include "mysql_export.h"
#include "proto_data/project.h"
#include "mysql_wrapper/mysql_connection.h"
#include "cppformat/cppformat/format.h"

// This macro initializes some variables used for batching multiple mysql insert commands
#define WOWPP_INIT_BATCH() \
	UInt32 entryCount = 0, batch = 0; \
	std::stringstream buffer;

// Prepares a new batch entry by either adding the INSERT statement or a separator and incrementing the counters.
#define WOWPP_PREPARE_BATCH_ENTRY(query) \
	if (batch == 0) buffer << query; \
	else if (batch > 0) buffer << ","; \
	entryCount++; batch++;

// This macro submits the batched entries if the limit has been reached and resets the counters
#define WOWPP_SUBMIT_BATCH(count) \
	if (batch >= BatchCount || entryCount >= static_cast<UInt32>(count)) \
	{	\
		if (conn.execute(buffer.str()))	\
		{	\
			watcher.reportProgressChange(static_cast<float>(entryCount) / static_cast<float>(templates.entry_size())); \
			buffer.str(String()); \
			buffer.clear(); \
			batch = 0; \
			continue; \
		}	\
		watcher.reportError(conn.getErrorMessage()); \
		return false; \
	}


namespace wowpp
{
	namespace editor
	{
		/// Number of INSERT statements that will be combined into one single statement for performance.
		static constexpr UInt32 BatchCount = 100;

		TransferTask exportAreaTriggers(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting area triggers...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `area_triggers`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				const auto& templates = project.areaTriggers.getTemplates();
				for (const auto& trigger : templates.entry())
				{
					WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `area_triggers` (`id`,`name`,`map`,`x`,`y`,`z`,`radius`,`box_x`,`box_y`,`box_z`,`box_o`,`targetmap`,`target_x`,`target_y`,`target_z`,`target_o`,`questid`,`tavern`) VALUES ");
					buffer << fmt::format("({0},'{1}',{2},{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13},{14},{15},{16},{17})"
						, trigger.id(), conn.escapeString(trigger.name()), trigger.map(), trigger.x(), trigger.y(), trigger.z()
						, trigger.radius(), trigger.box_x(), trigger.box_y(), trigger.box_z(), trigger.box_o(), trigger.targetmap()
						, trigger.target_x(), trigger.target_y(), trigger.target_z(), trigger.target_o(), trigger.questid()
						, trigger.tavern() ? 1 : 0);
					WOWPP_SUBMIT_BATCH(templates.entry_size());
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportEmotes(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting emotes...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `emotes`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				const auto& templates = project.emotes.getTemplates();
				for (const auto& entry : templates.entry())
				{
					WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `emotes` (`id`,`name`,`textid`) VALUES ");
					buffer << fmt::format("({0},'{1}',{2})"
						, entry.id(), conn.escapeString(entry.name()), entry.textid());
					WOWPP_SUBMIT_BATCH(templates.entry_size());
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportFactionTemplates(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting faction templates...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `faction_templates`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				const auto& templates = project.factionTemplates.getTemplates();
				for (const auto& entry : templates.entry())
				{
					WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `faction_templates` (`id`,`flags`,`selfmask`,`friendmask`,`enemymask`,`faction`,`friend_1`,`friend_2`,`friend_3`,`friend_4`,`enemy_1`,`enemy_2`,`enemy_3`,`enemy_4`) VALUES ");
					buffer << fmt::format("({0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13})"
						, entry.id(), entry.flags(), entry.selfmask(), entry.friendmask(), entry.enemymask(), entry.faction()
						, entry.friends_size() >= 1 ? std::to_string(entry.friends(0)) : "NULL"
						, entry.friends_size() >= 2 ? std::to_string(entry.friends(1)) : "NULL"
						, entry.friends_size() >= 3 ? std::to_string(entry.friends(2)) : "NULL"
						, entry.friends_size() >= 4 ? std::to_string(entry.friends(3)) : "NULL"
						, entry.enemies_size() >= 1 ? std::to_string(entry.enemies(0)) : "NULL"
						, entry.enemies_size() >= 2 ? std::to_string(entry.enemies(1)) : "NULL"
						, entry.enemies_size() >= 3 ? std::to_string(entry.enemies(2)) : "NULL"
						, entry.enemies_size() >= 4 ? std::to_string(entry.enemies(3)) : "NULL"
					);
					WOWPP_SUBMIT_BATCH(templates.entry_size());
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportFactions(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting factions...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `factions`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				const auto& templates = project.factions.getTemplates();
				for (const auto& entry : templates.entry())
				{
					WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `factions` (`id`,`name`,`replistid`,`racemask_1`,`classmask_1`,`value_1`,`flags_1`,`racemask_2`,`classmask_2`,`value_2`,`flags_2`,`racemask_3`,`classmask_3`,`value_3`,`flags_3`,`racemask_4`,`classmask_4`,`value_4`,`flags_4`) VALUES ");
					buffer << fmt::format("({0},'{1}',{2},{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13},{14},{15},{16},{17},{18})"
						, entry.id(), conn.escapeString(entry.name()), entry.replistid()
						, entry.baserep_size() > 0 ? std::to_string(entry.baserep(0).racemask()) : "NULL"
						, entry.baserep_size() > 0 ? std::to_string(entry.baserep(0).classmask()) : "NULL"
						, entry.baserep_size() > 0 ? std::to_string(entry.baserep(0).value()) : "NULL"
						, entry.baserep_size() > 0 ? std::to_string(entry.baserep(0).flags()) : "NULL"
						, entry.baserep_size() > 1 ? std::to_string(entry.baserep(1).racemask()) : "NULL"
						, entry.baserep_size() > 1 ? std::to_string(entry.baserep(1).classmask()) : "NULL"
						, entry.baserep_size() > 1 ? std::to_string(entry.baserep(1).value()) : "NULL"
						, entry.baserep_size() > 1 ? std::to_string(entry.baserep(1).flags()) : "NULL"
						, entry.baserep_size() > 2 ? std::to_string(entry.baserep(2).racemask()) : "NULL"
						, entry.baserep_size() > 2 ? std::to_string(entry.baserep(2).classmask()) : "NULL"
						, entry.baserep_size() > 2 ? std::to_string(entry.baserep(2).value()) : "NULL"
						, entry.baserep_size() > 2 ? std::to_string(entry.baserep(2).flags()) : "NULL"
						, entry.baserep_size() > 3 ? std::to_string(entry.baserep(3).racemask()) : "NULL"
						, entry.baserep_size() > 3 ? std::to_string(entry.baserep(3).classmask()) : "NULL"
						, entry.baserep_size() > 3 ? std::to_string(entry.baserep(3).value()) : "NULL"
						, entry.baserep_size() > 3 ? std::to_string(entry.baserep(3).flags()) : "NULL"
					);
					WOWPP_SUBMIT_BATCH(templates.entry_size());
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportFishingLoot(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting fishing loot...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `fishing_loot`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				// TODO

				return true;
			};

			return std::move(task);
		}

		TransferTask exportUnits(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting units...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `units`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				const auto& templates = project.units.getTemplates();
				for (const auto& entry : templates.entry())
				{
					WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `units` (`id`,`name`,`subname`,`minlevel`,`maxlevel`,`alliancefaction`,`hordefaction`,`maleModel`,`femaleModel`,`scale`,`type`,`family`,`regeneratehealth`,`npcflags`,`unitflags`,`dynamicflags`,`extraflags`,`creaturetypeflags`,"
						"`walkspeed`,`runspeed`,`unitclass`,`rank`,`minlevelhealth`,`maxlevelhealth`,`minlevelmana`,`maxlevelmana`,`minmeleedmg`,`maxmeleedmg`,`minrangeddmg`,`maxrangeddmg`,`armor`,`resistance_1`,`resistance_2`,`resistance_3`,`resistance_4`,`resistance_5`,"
						"`meleeattacktime`,`rangedattacktime`,`damageschool`,`minlootgold`,`maxlootgold`,`minlevelxp`,`maxlevelxp`,`mainhandweapon`,`offhandweapon`,`rangedweapon`,`attackpower`,`rangedattackpower`,`unitlootentry`,`vendorentry`,`trainerentry`,`mechanicimmunity`,"
						"`schoolimmunity`,`skinninglootentry`,`killcredit`"
						") VALUES ");
					buffer << fmt::format("({0},'{1}','{2}',{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13},{14},{15},{16},{17},"
						"{18},'{19}',{20},{21},{22},{23},{24},{25},{26},{27},{28},{29},{30},{31},{32},{33},{34},{35},"
						"{36},'{37}',{38},{39},{40},{41},{42},{43},{44},{45},{46},{47},{48},{49},{50},{51},{52},{53},{54})"
						, entry.id(), conn.escapeString(entry.name()), conn.escapeString(entry.subname()), entry.minlevel(), entry.maxlevel(), entry.alliancefaction()
						, entry.hordefaction(), entry.malemodel(), entry.femalemodel(), entry.scale(), entry.type(), entry.family()
						, entry.regeneratehealth(), entry.npcflags(), entry.unitflags(), entry.dynamicflags(), entry.extraflags()
						, entry.creaturetypeflags(), entry.walkspeed(), entry.runspeed(), entry.unitclass(), entry.rank(), entry.minlevelhealth()
						, entry.maxlevelhealth(), entry.minlevelmana(), entry.maxlevelmana(), entry.minmeleedmg(), entry.maxmeleedmg()
						, entry.minrangeddmg(), entry.maxrangeddmg(), entry.armor(), entry.resistances(0), entry.resistances(1), entry.resistances(2)
						, entry.resistances(3), entry.resistances(4), entry.meleeattacktime(), entry.rangedattacktime(), entry.damageschool()
						, entry.minlootgold(), entry.maxlootgold(), entry.minlevelxp(), entry.maxlevelxp(), entry.mainhandweapon(), entry.offhandweapon()
						, entry.rangedweapon(), entry.attackpower(), entry.rangedattackpower(), entry.unitlootentry(), entry.vendorentry()
						, entry.trainerentry(), entry.mechanicimmunity(), entry.schoolimmunity(), entry.skinninglootentry(), entry.killcredit());
					WOWPP_SUBMIT_BATCH(templates.entry_size());
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportUnitSpells(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting unit spells...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `unit_spells`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				UInt32 maxCount = 0;

				const auto& templates = project.units.getTemplates();
				for (const auto& entry : templates.entry())
				{
					// Increase counter
					maxCount += entry.creaturespells_size();
					if (entry.creaturespells_size() > 0)
					{
						for (const auto& spellentry : entry.creaturespells())
						{
							WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `unit_spells` (`unit_id`,`spell_id`,`priority`,`mininitialcooldown`,`maxinitialcooldown`,`mincooldown`,`maxcooldown`,`target`,`repeated`,`minrange`,`maxrange`,`probability`) VALUES ");
							buffer << fmt::format("({0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11})"
								, entry.id(), spellentry.spellid(), spellentry.priority(), spellentry.mininitialcooldown(), spellentry.maxinitialcooldown(), spellentry.mincooldown()
								, spellentry.maxcooldown(), spellentry.target(), spellentry.repeated(), spellentry.minrange(), spellentry.maxrange(), spellentry.probability());
							WOWPP_SUBMIT_BATCH(maxCount);
						}
					}
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportUnitQuests(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting unit quests...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `unit_quests`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				UInt32 maxCount = 0;

				const auto& templates = project.units.getTemplates();
				for (const auto& entry : templates.entry())
				{
					// Increase counter
					maxCount += entry.quests_size();
					if (entry.quests_size() > 0)
					{
						for (const auto& questid : entry.quests())
						{
							WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `unit_quests` (`unit_id`,`quest_id`) VALUES ");
							buffer << fmt::format("({0},{1})"
								, entry.id(), questid);
							WOWPP_SUBMIT_BATCH(maxCount);
						}
					}
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportUnitEndQuests(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting unit end quests...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `unit_endquests`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				UInt32 maxCount = 0;

				const auto& templates = project.units.getTemplates();
				for (const auto& entry : templates.entry())
				{
					// Increase counter
					maxCount += entry.end_quests_size();
					if (entry.end_quests_size() > 0)
					{
						for (const auto& questid : entry.end_quests())
						{
							WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `unit_endquests` (`unit_id`,`quest_id`) VALUES ");
							buffer << fmt::format("({0},{1})"
								, entry.id(), questid);
							WOWPP_SUBMIT_BATCH(maxCount);
						}
					}
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportZones(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting zones...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				// Delete all database values
				conn.execute("DELETE FROM `zones`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				const auto& templates = project.zones.getTemplates();
				for (const auto& entry : templates.entry())
				{
					WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `zones` (`id`,`name`,`parentzone`,`map`,`explore`,`flags`,`team`,`level`) VALUES ");
					buffer << fmt::format("({0},'{1}',{2},{3},{4},{5},{6},{7})"
						, entry.id(), conn.escapeString(entry.name()), entry.parentzone(), entry.map()
						, entry.explore(), entry.flags(), entry.team(), entry.level());
					WOWPP_SUBMIT_BATCH(templates.entry_size());
				}

				return true;
			};

			return std::move(task);
		}
	}
}

// Remove helper macros as they are only required here
#undef WOWPP_INIT_BATCH
#undef WOWPP_PREPARE_BATCH_ENTRY
#undef WOWPP_SUBMIT_BATCH
