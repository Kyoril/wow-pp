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

#define WOWPP_ENTRY_OR_NULL(param) \
	(param) ? std::to_string((param)) : "NULL"

#define WOWPP_ARRAY_OR_NULL(index, param) \
	entry.param##_size() > index ? std::to_string(entry.param(index)) : "NULL"

#define WOWPP_ARRAY_ELEM_OR_NULL(index, param, elem) \
	entry.param##_size() > index ? std::to_string(entry.param(index).elem()) : "NULL"


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
						, WOWPP_ARRAY_OR_NULL(0, friends), WOWPP_ARRAY_OR_NULL(1, friends), WOWPP_ARRAY_OR_NULL(2, friends), WOWPP_ARRAY_OR_NULL(3, friends)
						, WOWPP_ARRAY_OR_NULL(0, enemies), WOWPP_ARRAY_OR_NULL(1, enemies), WOWPP_ARRAY_OR_NULL(2, enemies), WOWPP_ARRAY_OR_NULL(3, enemies)
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
						, WOWPP_ARRAY_ELEM_OR_NULL(0, baserep, racemask), WOWPP_ARRAY_ELEM_OR_NULL(0, baserep, classmask), WOWPP_ARRAY_ELEM_OR_NULL(0, baserep, value), WOWPP_ARRAY_ELEM_OR_NULL(0, baserep, flags)
						, WOWPP_ARRAY_ELEM_OR_NULL(1, baserep, racemask), WOWPP_ARRAY_ELEM_OR_NULL(1, baserep, classmask), WOWPP_ARRAY_ELEM_OR_NULL(1, baserep, value), WOWPP_ARRAY_ELEM_OR_NULL(1, baserep, flags)
						, WOWPP_ARRAY_ELEM_OR_NULL(2, baserep, racemask), WOWPP_ARRAY_ELEM_OR_NULL(2, baserep, classmask), WOWPP_ARRAY_ELEM_OR_NULL(2, baserep, value), WOWPP_ARRAY_ELEM_OR_NULL(2, baserep, flags)
						, WOWPP_ARRAY_ELEM_OR_NULL(3, baserep, racemask), WOWPP_ARRAY_ELEM_OR_NULL(3, baserep, classmask), WOWPP_ARRAY_ELEM_OR_NULL(3, baserep, value), WOWPP_ARRAY_ELEM_OR_NULL(3, baserep, flags)
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

		TransferTask exportMaps(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting maps...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `maps`;");
				conn.execute("SET SESSION sql_mode='NO_AUTO_VALUE_ON_ZERO';");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				const auto& templates = project.maps.getTemplates();
				for (const auto& entry : templates.entry())
				{
					WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `maps` (`id`,`name`,`directory`,`type`) VALUES ");
					buffer << fmt::format("({0},'{1}','{2}',{3})"
						, entry.id(), conn.escapeString(entry.name()), conn.escapeString(entry.directory()), entry.instancetype()
					);
					WOWPP_SUBMIT_BATCH(templates.entry_size());
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportMapUnitSpawns(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting map unit spawns...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `map_unit_spawns`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				UInt32 maxCount = 0;

				const auto& templates = project.maps.getTemplates();
				for (const auto& map : templates.entry())
				{
					maxCount += map.unitspawns_size();
					for (const auto& entry : map.unitspawns())
					{
						WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `map_unit_spawns` (`map_id`,`name`,`respawn`,`respawndelay`,`x`,`y`,`z`,`rotation`,`radius`,`maxcount`,`unitentry`,`defaultemote`,`isactive`,`movement`,`standstate`) VALUES ");
						buffer << fmt::format("({0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13},{14})"
							, map.id()
							, entry.has_name() ? "'" + conn.escapeString(entry.name()) + "'" : "NULL"
							, entry.respawn(), entry.respawndelay(), entry.positionx(), entry.positiony(), entry.positionz(), entry.rotation()
							, entry.radius(), entry.maxcount(), entry.unitentry(), entry.defaultemote(), entry.isactive(), entry.movement()
							, entry.standstate()
						);
						WOWPP_SUBMIT_BATCH(maxCount);
					}
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportMapObjectSpawns(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting map object spawns...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `map_object_spawns`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				UInt32 maxCount = 0;

				const auto& templates = project.maps.getTemplates();
				for (const auto& map : templates.entry())
				{
					maxCount += map.objectspawns_size();
					for (const auto& entry : map.objectspawns())
					{
						WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `map_object_spawns` (`map_id`,`name`,`respawn`,`respawndelay`,`position_x`,`position_y`,`position_z`,`rotation_w`,`rotation_x`,`rotation_y`,`rotation_z`,`radius`,`anim_progress`,`state`,`maxcount`,`objectentry`,`isactive`,`orientation`) VALUES ");
						buffer << fmt::format("({0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13},{14},{15},{16},{17})"
							, map.id()
							, entry.has_name() ? "'" + conn.escapeString(entry.name()) + "'" : "NULL"
							, entry.respawn(), entry.respawndelay(), entry.positionx(), entry.positiony(), entry.positionz()
							, entry.rotationw(), entry.rotationx(), entry.rotationy(), entry.rotationz()
							, entry.radius(), entry.animprogress(), entry.state(), entry.maxcount(), entry.objectentry()
							, entry.isactive(), entry.orientation()
						);
						WOWPP_SUBMIT_BATCH(maxCount);
					}
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportSpells(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting spells...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `spells`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				try
				{
					const auto& templates = project.spells.getTemplates();
					for (const auto& entry : templates.entry())
					{
						WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `spells` ("
							"`id`,`name`,`attributes_1`,`attributes_2`,`attributes_3`,`attributes_4`,`attributes_5`,`attributes_6`,`attributes_7`,"
							"`effect_0`,`basepoints_0`,`diesides_0`,`diceperlevel_0`,`pointsperlevel_0`,`mechanic_0`,`targeta_0`,`targetb_0`,`radius_0`,`aura_0`,`amplitude_0`,`multiplevalue_0`,`chaintarget_0`,`itemtype_0`,`miscvaluea_0`,`miscvalueb_0`,`triggerspell_0`,`summonunit_0`,`pointspercombopoint_0`,`affectmask_0`,`dmgmultiplier_0`,"
							"`effect_1`,`basepoints_1`,`diesides_1`,`diceperlevel_1`,`pointsperlevel_1`,`mechanic_1`,`targeta_1`,`targetb_1`,`radius_1`,`aura_1`,`amplitude_1`,`multiplevalue_1`,`chaintarget_1`,`itemtype_1`,`miscvaluea_1`,`miscvalueb_1`,`triggerspell_1`,`summonunit_1`,`pointspercombopoint_1`,`affectmask_1`,`dmgmultiplier_1`,"
							"`effect_2`,`basepoints_2`,`diesides_2`,`diceperlevel_2`,`pointsperlevel_2`,`mechanic_2`,`targeta_2`,`targetb_2`,`radius_2`,`aura_2`,`amplitude_2`,`multiplevalue_2`,`chaintarget_2`,`itemtype_2`,`miscvaluea_2`,`miscvalueb_2`,`triggerspell_2`,`summonunit_2`,`pointspercombopoint_2`,`affectmask_2`,`dmgmultiplier_2`,"
							"`cooldown`,`casttime`,`powertype`,`cost`,`costpct`,"
							"`maxlevel`,`baselevel`,`spelllevel`,`speed`,`schoolmask`,"
							"`dmgclass`,`itemclass`,`itemsubclassmask`,`facing`,`duration`,"
							"`maxduration`,`interruptflags`,`aurainterruptflags`,`minrange`,`maxrange`,"
							"`rangetype`,`targetmap`,`targetx`,`targety`,`targetz`,"
							"`targeto`,`maxtargets`,`talentcost`,`procflags`,`procchance`,"
							"`proccharges`,`mechanic`,`category`,`categorycooldown`,`dispel`,"
							"`family`,`familyflags`,`focusobject`,`procschool`,`procfamily`,"
							"`procfamilyflags`,`procexflags`,`procpermin`,`proccooldown`,`proccustomflags`,"
							"`baseid`,`channelinterruptflags`,`stackamount`,`skill`,`trivialskilllow`,"
							"`trivialskillhigh`,`racemask`,`classmask`,"
							"`item_0`,`count_0`,`item_1`,`count_1`,`item_2`,`count_2`,`item_3`,`count_3`,`item_4`,`count_4`,"
							"`rank`,`prevspell`,`nextspell`,`positive`"
							") VALUES ");
						buffer << fmt::format("({0},'{1}',{2},{3},{4},{5},{6},{7},{8},"
							"{9},{10},{11},{12},{13},{14},{15},{16},{17},{18},{19},{20},{21},{22},{23},{24},{25},{26},{27},{28},{29},{30},"
							"{31},{32},{33},{34},{35},{36},{37},{38},{39},{40},{41},{42},{43},{44},{45},{46},{47},{48},{49},{50},{51},{52},"
							"{53},{54},{55},{56},{57},{58},{59},{60},{61},{62},{63},{64},{65},{66},{67},{68},{69},{70},{71},{72},{73},{74},"
							"{75},{76},{77},{78},{79},{80},{81},{82},{83},{84},{85},{86},{87},{88},{89},{90},{91},{92},{93},{94},"
							"{95},{96},{97},{98},{99},{100},{101},{102},{103},{104},{105},{106},{107},{108},{109},{110},{111},{112},"
							"{113},{114},{115},{116},{117},{118},{119},{120},{121},{122},{123},{124},{125},{126},{127},{128},{129},{130},{131},"
							"{132},{133},{134},{135},{136},{137},{138})"
							, entry.id(), conn.escapeString(entry.name()), entry.attributes(0), entry.attributes(1), entry.attributes(2)
							, entry.attributes(3), entry.attributes(4), entry.attributes(5), entry.attributes(6)
							// Effect 0
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, type)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, basepoints)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, diesides)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, diceperlevel)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, pointsperlevel)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, mechanic)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, targeta)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, targetb)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, radius)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, aura)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, amplitude)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, multiplevalue)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, chaintarget)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, itemtype)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, miscvaluea)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, miscvalueb)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, triggerspell)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, summonunit)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, pointspercombopoint)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, affectmask)
							, WOWPP_ARRAY_ELEM_OR_NULL(0, effects, dmgmultiplier)
							// Effect 1
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, type)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, basepoints)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, diesides)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, diceperlevel)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, pointsperlevel)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, mechanic)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, targeta)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, targetb)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, radius)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, aura)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, amplitude)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, multiplevalue)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, chaintarget)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, itemtype)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, miscvaluea)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, miscvalueb)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, triggerspell)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, summonunit)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, pointspercombopoint)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, affectmask)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, effects, dmgmultiplier)
							// Effect 2
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, type)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, basepoints)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, diesides)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, diceperlevel)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, pointsperlevel)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, mechanic)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, targeta)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, targetb)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, radius)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, aura)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, amplitude)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, multiplevalue)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, chaintarget)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, itemtype)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, miscvaluea)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, miscvalueb)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, triggerspell)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, summonunit)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, pointspercombopoint)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, affectmask)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, effects, dmgmultiplier)
							// Other attributes
							, entry.cooldown(), entry.casttime(), entry.powertype(), entry.cost(), entry.costpct()
							, entry.maxlevel(), entry.baselevel(), entry.spelllevel(), entry.speed(), entry.schoolmask()
							, entry.dmgclass(), entry.itemclass(), entry.itemsubclassmask(), entry.facing(), entry.duration()
							, entry.maxduration(), entry.interruptflags(), entry.aurainterruptflags(), entry.minrange(), entry.maxrange()
							, entry.rangetype(), entry.targetmap(), entry.targetx(), entry.targety(), entry.targetz()
							, entry.targeto(), entry.maxtargets(), entry.talentcost(), entry.procflags(), entry.procchance()
							, entry.proccharges(), entry.mechanic(), entry.category(), entry.categorycooldown(), entry.dispel()
							, entry.family(), entry.familyflags(), entry.focusobject(), entry.procschool(), entry.procfamily()
							, entry.procfamilyflags(), entry.procexflags(), entry.procpermin(), entry.proccooldown(), entry.proccustomflags()
							, entry.baseid(), entry.channelinterruptflags(), entry.stackamount(), entry.skill(), entry.trivialskilllow()
							, entry.trivialskillhigh(), entry.racemask(), entry.classmask()
							// Regent array
							, WOWPP_ARRAY_ELEM_OR_NULL(0, reagents, item), WOWPP_ARRAY_ELEM_OR_NULL(0, reagents, count)
							, WOWPP_ARRAY_ELEM_OR_NULL(1, reagents, item), WOWPP_ARRAY_ELEM_OR_NULL(1, reagents, count)
							, WOWPP_ARRAY_ELEM_OR_NULL(2, reagents, item), WOWPP_ARRAY_ELEM_OR_NULL(2, reagents, count)
							, WOWPP_ARRAY_ELEM_OR_NULL(3, reagents, item), WOWPP_ARRAY_ELEM_OR_NULL(3, reagents, count)
							, WOWPP_ARRAY_ELEM_OR_NULL(4, reagents, item), WOWPP_ARRAY_ELEM_OR_NULL(4, reagents, count)
							// Other attributes
							, entry.rank(), entry.prevspell(), entry.nextspell(), entry.positive()
						);

						WOWPP_SUBMIT_BATCH(templates.entry_size());
					}
				}
				catch (const std::exception& ex)
				{
					watcher.reportError(ex.what());
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportTalents(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting talents...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `talents`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				const auto& templates = project.talents.getTemplates();
				for (const auto& entry : templates.entry())
				{
					WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `talents` (`id`,`tab`,`row`,`column`,`rank_1`,`rank_2`,`rank_3`,`rank_4`,`rank_5`,`dependson`,`dependsonrank`,`dependsonspell`) VALUES ");
					buffer << fmt::format("({0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11})"
						, entry.id(), entry.tab(), entry.row(), entry.column()
						// Ranks array
						, WOWPP_ARRAY_OR_NULL(0, ranks), WOWPP_ARRAY_OR_NULL(1, ranks), WOWPP_ARRAY_OR_NULL(2, ranks), WOWPP_ARRAY_OR_NULL(3, ranks), WOWPP_ARRAY_OR_NULL(4, ranks)
						, entry.dependson(), entry.dependsonrank(), entry.dependsonspell()
					);
					WOWPP_SUBMIT_BATCH(templates.entry_size());
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportTrainers(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting trainers...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `trainers`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				const auto& templates = project.trainers.getTemplates();
				for (const auto& entry : templates.entry())
				{
					WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `trainers` (`id`,`type`,`classid`,`title`) VALUES ");
					buffer << fmt::format("({0},{1},{2},'{3}')"
						, entry.id(), entry.type(), entry.classid(), conn.escapeString(entry.title())
					);
					WOWPP_SUBMIT_BATCH(templates.entry_size());
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportTrainerSpells(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting trainer spells...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `trainer_spells`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				UInt32 maxCount = 0;

				const auto& templates = project.trainers.getTemplates();
				for (const auto& trainer : templates.entry())
				{
					maxCount += trainer.spells_size();
					for (const auto& entry : trainer.spells())
					{
						WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `trainer_spells` (`trainer_id`,`spell`,`spellcost`,`reqskill`,`reqskillval`,`reqlevel`) VALUES ");
						buffer << fmt::format("({0},{1},{2},{3},{4},{5})"
							, trainer.id(), entry.spell(), entry.spellcost(), entry.reqskill(), entry.reqskillval(), entry.reqlevel()
						);
						WOWPP_SUBMIT_BATCH(maxCount);
					}
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportTriggers(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting triggers...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `triggers`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				const auto& templates = project.triggers.getTemplates();
				for (const auto& entry : templates.entry())
				{
					WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `triggers` (`id`,`name`,`flags`,`categoryid`,`probability`) VALUES ");
					buffer << fmt::format("({0},'{1}',{2},{3},{4})"
						, entry.id(), conn.escapeString(entry.name()), entry.flags(), entry.categoryid(), entry.probability()
					);
					WOWPP_SUBMIT_BATCH(templates.entry_size());
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportTriggerEvents(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting trigger events...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `trigger_events`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				UInt32 maxCount = 0;

				const auto& templates = project.triggers.getTemplates();
				for (const auto& trigger : templates.entry())
				{
					maxCount += trigger.newevents_size();
					for (const auto& entry : trigger.newevents())
					{
						WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `trigger_events` (`trigger_id`,`type`,`data_1`,`data_2`,`data_3`,`data_4`) VALUES ");
						buffer << fmt::format("({0},{1},{2},{3},{4},{5})"
							, trigger.id(), entry.type()
							// data array
							, WOWPP_ARRAY_OR_NULL(0, data), WOWPP_ARRAY_OR_NULL(1, data), WOWPP_ARRAY_OR_NULL(2, data), WOWPP_ARRAY_OR_NULL(3, data)
						);
						WOWPP_SUBMIT_BATCH(maxCount);
					}
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportTriggerActions(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting trigger actions...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `trigger_actions`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				UInt32 maxCount = 0;

				const auto& templates = project.triggers.getTemplates();
				for (const auto& trigger : templates.entry())
				{
					maxCount += trigger.actions_size();
					for (const auto& entry : trigger.actions())
					{
						WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `trigger_actions` (`trigger_id`,`action`,`target`,`targetname`,`text_1`,`text_2`,`text_3`,`text_4`,`data_1`,`data_2`,`data_3`,`data_4`) VALUES ");
						buffer << fmt::format("({0},{1},{2},'{3}',{4},{5},{6},{7},{8},{9},{10},{11})"
							, trigger.id(), entry.action(), entry.target()
							, conn.escapeString(entry.targetname())
							// Texts array
							, entry.texts_size() > 0 ? "'" + conn.escapeString(entry.texts(0)) + "'" : "NULL"
							, entry.texts_size() > 1 ? "'" + conn.escapeString(entry.texts(1)) + "'" : "NULL"
							, entry.texts_size() > 2 ? "'" + conn.escapeString(entry.texts(2)) + "'" : "NULL"
							, entry.texts_size() > 3 ? "'" + conn.escapeString(entry.texts(3)) + "'" : "NULL"
							// Data array
							, WOWPP_ARRAY_OR_NULL(0, data), WOWPP_ARRAY_OR_NULL(1, data), WOWPP_ARRAY_OR_NULL(2, data), WOWPP_ARRAY_OR_NULL(3, data)
						);
						WOWPP_SUBMIT_BATCH(maxCount);
					}
				}

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
						, entry.rangedweapon(), entry.attackpower(), entry.rangedattackpower()
						, entry.unitlootentry() != 0 ? std::to_string(entry.unitlootentry()) : "NULL"
						, entry.vendorentry() != 0 ? std::to_string(entry.vendorentry()) : "NULL"
						, entry.trainerentry() != 0 ? std::to_string(entry.trainerentry()) : "NULL"
						, entry.mechanicimmunity(), entry.schoolimmunity(), entry.skinninglootentry(), entry.killcredit());
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
							buffer << fmt::format("({0},{1})", entry.id(), questid);
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
							buffer << fmt::format("({0},{1})", entry.id(), questid);
							WOWPP_SUBMIT_BATCH(maxCount);
						}
					}
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportVendors(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting vendors...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `vendors`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				const auto& templates = project.vendors.getTemplates();
				for (const auto& entry : templates.entry())
				{
					WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `vendors` (`id`) VALUES ");
					buffer << fmt::format("({0})", entry.id());
					WOWPP_SUBMIT_BATCH(templates.entry_size());
				}

				return true;
			};

			return std::move(task);
		}

		TransferTask exportVendorItems(const proto::Project & project)
		{
			TransferTask task;
			task.taskName = "Exporting vendor items...";
			task.beforeTransfer = [](wowpp::MySQL::Connection& conn) {
				conn.execute("DELETE FROM `vendor_items`;");
			};
			task.doWork = [&project](wowpp::MySQL::Connection &conn, ITransferProgressWatcher& watcher) -> bool {
				WOWPP_INIT_BATCH();

				UInt32 maxCount = 0;

				const auto& templates = project.vendors.getTemplates();
				for (const auto& vendor : templates.entry())
				{
					maxCount += vendor.items_size();
					for (const auto& entry : vendor.items())
					{
						WOWPP_PREPARE_BATCH_ENTRY("INSERT INTO `vendor_items` (`vendor_id`,`item`,`maxcount`,`interval`,`extendedcost`,`isactive`) VALUES ");
						buffer << fmt::format("({0},{1},{2},{3},{4},{5})"
							, vendor.id(), entry.item(), entry.maxcount(), entry.interval(), entry.extendedcost(), entry.isactive()
						);
						WOWPP_SUBMIT_BATCH(maxCount);
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
