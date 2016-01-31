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
#include "common/constants.h"
#include "simple_file_format/sff_write_file.h"
#include "simple_file_format/sff_write_array.h"
#include "simple_file_format/sff_write_array.h"
#include "simple_file_format/sff_write.h"
#include "simple_file_format/sff_read_tree.h"
#include "simple_file_format/sff_load_file.h"
#include "log/default_log.h"
#include "log/log_std_stream.h"
#include "log/default_log_levels.h"
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
#include "game/defines.h"
using namespace wowpp;

namespace wowpp
{
	/// Manages the realm server configuration.
	struct Configuration
	{
		/// Config file version: used to detect new configuration files
		static const UInt32 PatcherConfigVersion;

		/// Path to the client data
		String dataPath;

		/// The port to be used for a mysql connection.
		NetPort mysqlPort;
		/// The mysql server host address (ip or dns).
		String mysqlHost;
		/// The mysql user to be used.
		String mysqlUser;
		/// The mysql user password to be used.
		String mysqlPassword;
		/// The mysql database to be used.
		String mysqlDatabase;

		/// Indicates whether or not file logging is enabled.
		bool isLogActive;
		/// File name of the log file.
		String logFileName;
		/// If enabled, the log contents will be buffered before they are written to
		/// the file, which could be more efficient..
		bool isLogFileBuffering;

		/// Initializes a new instance of the Configuration class using the default
		/// values.
		explicit Configuration()
			: dataPath("./")
			, mysqlPort(wowpp::constants::DefaultMySQLPort)
			, mysqlHost("127.0.0.1")
			, mysqlUser("username")
			, mysqlPassword("password")
			, mysqlDatabase("dbc")
			, isLogActive(true)
			, logFileName("wowpp_patcher.log")
			, isLogFileBuffering(false)
		{
		}

		/// Loads the configuration values from a specific file.
		/// @param fileName Name of the configuration file.
		/// @returns true if the config file was loaded successfully, false otherwise.
		bool load(const String &fileName)
		{
			typedef String::const_iterator Iterator;
			typedef sff::read::tree::Table<Iterator> Table;

			Table global;
			std::string fileContent;

			std::ifstream file(fileName, std::ios::binary);
			if (!file)
			{
				if (save(fileName))
				{
					ILOG("Saved default settings as " << fileName);
				}
				else
				{
					ELOG("Could not save default settings as " << fileName);
				}

				return false;
			}

			try
			{
				sff::loadTableFromFile(global, fileContent, file);

				// Read config version
				UInt32 fileVersion = 0;
				if (!global.tryGetInteger("version", fileVersion) ||
					fileVersion != PatcherConfigVersion)
				{
					file.close();

					if (save(fileName))
					{
ILOG("Saved updated settings with default values as " << fileName);
					}
					else
					{
						ELOG("Could not save updated default settings as " << fileName);
					}

					return false;
				}

				if (const Table *const mysqlDatabaseTable = global.getTable("mysqlDatabase"))
				{
					mysqlPort = mysqlDatabaseTable->getInteger("port", mysqlPort);
					mysqlHost = mysqlDatabaseTable->getString("host", mysqlHost);
					mysqlUser = mysqlDatabaseTable->getString("user", mysqlUser);
					mysqlPassword = mysqlDatabaseTable->getString("password", mysqlPassword);
					mysqlDatabase = mysqlDatabaseTable->getString("database", mysqlDatabase);
				}

				if (const Table *const log = global.getTable("log"))
				{
					isLogActive = log->getInteger("active", static_cast<unsigned>(isLogActive)) != 0;
					logFileName = log->getString("fileName", logFileName);
					isLogFileBuffering = log->getInteger("buffering", static_cast<unsigned>(isLogFileBuffering)) != 0;
				}

				if (const Table *const game = global.getTable("game"))
				{
					dataPath = game->getString("dataPath", dataPath);
				}
			}
			catch (const sff::read::ParseException<Iterator> &e)
			{
				const auto line = std::count<Iterator>(fileContent.begin(), e.position.begin, '\n');
				ELOG("Error in config: " << e.what());
				ELOG("Line " << (line + 1) << ": " << e.position.str());
				return false;
			}

			return true;
		}
		/// Saves the configuration values to a specific file. Overwrites the whole
		/// file!
		/// @param fileName Name of the configuration file.
		/// @returns true if the config file was saved successfully, false otherwise.
		bool save(const String &fileName)
		{
			std::ofstream file(fileName.c_str());
			if (!file)
			{
				return false;
			}

			typedef char Char;
			sff::write::File<Char> global(file, sff::write::MultiLine);

			// Save file version
			global.addKey("version", PatcherConfigVersion);
			global.writer.newLine();

			{
				sff::write::Table<Char> mysqlDatabaseTable(global, "mysqlDatabase", sff::write::MultiLine);
				mysqlDatabaseTable.addKey("port", mysqlPort);
				mysqlDatabaseTable.addKey("host", mysqlHost);
				mysqlDatabaseTable.addKey("user", mysqlUser);
				mysqlDatabaseTable.addKey("password", mysqlPassword);
				mysqlDatabaseTable.addKey("database", mysqlDatabase);
				mysqlDatabaseTable.finish();
			}

			global.writer.newLine();

			{
				sff::write::Table<Char> log(global, "log", sff::write::MultiLine);
				log.addKey("active", static_cast<unsigned>(isLogActive));
				log.addKey("fileName", logFileName);
				log.addKey("buffering", isLogFileBuffering);
				log.finish();
			}

			global.writer.newLine();

			{
				sff::write::Table<Char> game(global, "game", sff::write::MultiLine);
				game.addKey("dataPath", dataPath);
				game.finish();
			}

			return true;
		}
	};

	const UInt32 Configuration::PatcherConfigVersion = 0x01;
}

namespace wowpp
{
	static bool addSpellLinks(proto::Project &project)
	{
		auto *spellEntries = project.spells.getTemplates().mutable_entry();
		if (spellEntries)
		{
			auto it = spellEntries->begin();
			while (it != spellEntries->end())
			{
				// Remove all additional spell entries
				it->clear_additionalspells();

				// Check this spell
				switch (it->id())
				{
					//////////////////////////////////////////////////////////////////////////
					// Power Word: Shield
					case 17:	// Rank 1
					case 592:	// Rank 2
					case 600:	// Rank 3
					case 3747:	// Rank 4
					case 6065:	// Rank 5
					case 6066:	// Rank 6
					case 10898:	// Rank 7
					case 10899:	// Rank 8
					case 10900: // Rank 9
					case 10901:	// Rank 10
					case 25217:	// Rank 11
					case 25218:	// Rank 12
						it->add_additionalspells(6788);	// Weakened Soul
						break;
					//////////////////////////////////////////////////////////////////////////
					// Holy Nova
					case 15237:	// Rank 1
						it->add_additionalspells(23455);
						break;	
					case 15430:	// Rank 2
						it->add_additionalspells(23458);
						break;	
					case 15431:	// Rank 3
						it->add_additionalspells(23459);
						break;	
					case 27799:	// Rank 4
						it->add_additionalspells(27803);
						break;	
					case 27800:	// Rank 5
						it->add_additionalspells(27804);
						break;	
					case 27801:	// Rank 6
						it->add_additionalspells(27805);
						break;	
					case 25331:	// Rank 7
						it->add_additionalspells(25329);
						break;
					//////////////////////////////////////////////////////////////////////////
					// Iceblock
					case 45438:
						it->add_additionalspells(41425);
						break;
					// Arcane blast
					case 36032:
						it->add_additionalspells(36032);
						break;
					//////////////////////////////////////////////////////////////////////////
					// Bandages
					case 746:
					case 1159:
					case 3267:
					case 3268:
					case 7926:
					case 7927:
					case 10838:
					case 10839:
					case 18608:
					case 18610:
					case 23567:
					case 23568:
					case 23569:
					case 23696:
					case 24412:
					case 24413:
					case 24414:
					case 27030:
					case 27031:
					case 30020:
						it->add_additionalspells(11196);
						break;
					//////////////////////////////////////////////////////////////////////////
					// Blood Fury
					case 20572:
					case 33697:
					case 33702:
						it->add_additionalspells(23230);
						break;
					//////////////////////////////////////////////////////////////////////////
					// Paladin bubble debuff
					// Divine Protection
					case 498:	// Rank 1
					case 5573:	// Rank 2
					// Divine Shield
					case 642:	// Rank 1
					case 1020:	// Rank 2
					// Hand of Protection
					case 1022:	// Rank 1
					case 5599:	// Rank 2
					case 10278:	// Rank 3
						it->add_additionalspells(25771);
						break;
					//////////////////////////////////////////////////////////////////////////
					// Charge
					case 100:	// Rank 1
					case 6178:	// Rank 2
					case 11578:	// Rank 3
					case 35754:	// unknown npc spell
						for (auto &eff : *it->mutable_effects())
						{
							// Instead of "DUMMY" effect, we make this a "ENERGIZE" effect, since this is all it does.
							if ((eff.type() == game::spell_effects::Dummy || eff.type() == game::spell_effects::Energize) && 
								eff.basepoints() != 0)
							{
								eff.set_type(game::spell_effects::Energize);
								eff.set_miscvaluea(1);	// Energize Rage
								break;
							}
						}
						break;

					//////////////////////////////////////////////////////////////////////////
					/////////////////////// AURA TRIGGER SPELLS //////////////////////////////
					//////////////////////////////////////////////////////////////////////////
						
					////////////////////////////// Warrior ///////////////////////////////////
						
					// Sweeping Strikes (fury talent)
					case 12328:
					case 18765:
					case 35429:
						it->mutable_effects(0)->set_triggerspell(26654);
						break;
					// Retaliation
					case 20230:
						it->mutable_effects(0)->set_triggerspell(22858);
						break;
					// Second Wind
					case 29834:	// Rank 1
						it->mutable_effects(0)->set_triggerspell(29841);
						break;
					case 29838:	// Rank 2
						it->mutable_effects(0)->set_triggerspell(29842);
						break;
						
					////////////////////////////// Paladin ///////////////////////////////////
						
					// Eye for an Eye
					case 9799:	// Rank 1
					case 25988:	// Rank 2
						it->mutable_effects(0)->set_triggerspell(25997);
						break;
					// Judgement of Light
                    case 20185:	// Rank 1
						it->mutable_effects(0)->set_triggerspell(20267);
						break;
                    case 20344:	// Rank 2
						it->mutable_effects(0)->set_triggerspell(20341);
						break;
                    case 20345:	// Rank 3
						it->mutable_effects(0)->set_triggerspell(20342);
						break;
                    case 20346:	// Rank 4
						it->mutable_effects(0)->set_triggerspell(20343);
						break;
                    case 27162:	// Rank 5
						it->mutable_effects(0)->set_triggerspell(27163);
						break;
                    // Judgement of Wisdom
                    case 20186:	// Rank 1
						it->mutable_effects(0)->set_triggerspell(20268);
						break;
                    case 20354:	// Rank 2
						it->mutable_effects(0)->set_triggerspell(20352);
						break;
                    case 20355:	// Rank 3
						it->mutable_effects(0)->set_triggerspell(20353);
						break;
                    case 27164:	// Rank 4
						it->mutable_effects(0)->set_triggerspell(27165);
						break;
					// Seal of Blood
					case 31892:	// Rank 1
						it->mutable_effects(0)->set_triggerspell(31893);
						it->mutable_effects(1)->set_triggerspell(32221);
						break;
					// Spiritual Attunement
					case 31785: // Rank 1
					case 33776: // Rank 2
						it->mutable_effects(0)->set_triggerspell(31786);
						break;
					// Seal of Vengeance
					case 31801: // Rank 1
						it->mutable_effects(0)->set_triggerspell(31803);
						break;
						
					////////////////////////////// Hunter ////////////////////////////////////
						
					// Thrill of the Hunt
					case 34497:
					case 34498:
					case 34499:
						it->mutable_effects(0)->set_triggerspell(34720);
						break;
						
					////////////////////////////// Rogue /////////////////////////////////////
						
					// Clean Escape - T1 set bonus
					case 23582:
						it->mutable_effects(0)->set_triggerspell(23583);
						break;
					// Quick Recovery - talent
					case 31244:
					case 31245:
						it->mutable_effects(0)->set_triggerspell(31663);
						break;
						
					////////////////////////////// Priest ////////////////////////////////////
						
					// Mana Leech (Passive) (Priest Pet Aura)
					case 28305:
						it->mutable_effects(0)->set_triggerspell(34650);
						break;
					// Vampiric Touch
					case 34914:
					case 34916:
					case 34917:
						it->mutable_effects(0)->set_triggerspell(34919);
						break;
					// Vampiric Embrace
					case 15286:
						it->mutable_effects(0)->set_triggerspell(15290);
						break;
					// Oracle Healing Bonus ("Garments of the Oracle" set)
					case 26169:
						it->mutable_effects(0)->set_triggerspell(26170);
						break;
					// Priest T3 set bonus
					case 28809:
						it->mutable_effects(0)->set_triggerspell(28810);
						break;
						
					////////////////////////////// Shaman ////////////////////////////////////
						
					// Lightning Shield
					case 324:	// Rank 1
						it->mutable_effects(0)->set_triggerspell(26364);	// FIRST effect needs to be fixed: New spell id
						break;
					case 325:	// Rank 2
						it->mutable_effects(0)->set_triggerspell(26365);
						break;
					case 905:	// Rank 3
						it->mutable_effects(0)->set_triggerspell(26366);
						break;
					case 945:	// Rank 4
						it->mutable_effects(0)->set_triggerspell(26367);
						break;
					case 8134:	// Rank 5
						it->mutable_effects(0)->set_triggerspell(26369);
						break;
					case 10431:	// Rank 6
						it->mutable_effects(0)->set_triggerspell(26370);
						break;
					case 10432:	// Rank 7
						it->mutable_effects(0)->set_triggerspell(26363);
						break;
					case 25469:	// Rank 8
						it->mutable_effects(0)->set_triggerspell(26371);
						break;
					case 25472:	// Rank 9
						it->mutable_effects(0)->set_triggerspell(26372);
						break;
					case 23551:	// Shaman Tier2 set bonus effect
						it->mutable_effects(0)->set_triggerspell(27635);
						break;
					// Totem of Flowing Water Relic
					case 28849:
						it->mutable_effects(0)->set_triggerspell(28850);
						break;
					// Earth Shield
					case 974:	// Rank 1
					case 32593:	// Rank 2
					case 32594:	// Rank 3
					case 32734: // npc spell
						it->mutable_effects(0)->set_triggerspell(379);
						break;
						
					////////////////////////////// Mage //////////////////////////////////////
						
					// Magic Absorption (arcan talent)
					case 29441:	// Rank 1
					case 29444:	// Rank 2
					case 29445:	// Rank 3
					case 29446:	// Rank 4
					case 29447:	// Rank 5
						it->mutable_effects(0)->set_triggerspell(29442);
						break;
					// Master of Elements (fire talent)
					case 29074:	// Rank 1
					case 29075:	// Rank 2
					case 29076:	// Rank 3
						it->mutable_effects(0)->set_triggerspell(29077);
						break;
					// Ignite (fire talent)
					case 11119:	// Rank 1
					case 11120:	// Rank 2
					case 12846:	// Rank 3
					case 12847:	// Rank 4
					case 12848:	// Rank 5
						it->mutable_effects(0)->set_triggerspell(12654);
						break;
					// Combustion (fire talent)
					case 11129:
						it->mutable_effects(0)->set_triggerspell(28682);
						break;
						
					////////////////////////////// Warlock ///////////////////////////////////
						
					// Seed of Corruption
					case 27243:	// Rank 1
						it->mutable_effects(1)->set_triggerspell(27285);
						break;
					case 32863:	// tbc dungeon trash spells
					case 36123:
					case 38252:
					case 39367:
					case 44141:
						it->mutable_effects(1)->set_triggerspell(32865);
						break;
					// Nightfall
					case 18094:	// Rank 1
					case 18095:	// Rank 2
						it->mutable_effects(0)->set_triggerspell(17941);
						break;
					// Soul Leech
					case 30293:	// Rank 1
					case 30295:	// Rank 2
					case 30296:	// Rank 3
						it->mutable_effects(0)->set_triggerspell(30294);
						break;
					// Shadowflame (T4 set bonus)
					case 37377:
						it->mutable_effects(0)->set_triggerspell(37379);
						break;
					// Shadowflame Hellfire (T4 set bonus)
					case 39437:
						it->mutable_effects(0)->set_triggerspell(37378);
						break;
					// Pet Healing (T5 set bonus of warlock and hunter)
					case 37381:
						it->mutable_effects(0)->set_triggerspell(37382);
						break;
						
					////////////////////////////// Druid /////////////////////////////////////
					
					// T3 set bonus
					case 28719:
						it->mutable_effects(0)->set_triggerspell(28742);
						break;
					// Idol of Longevity
					case 28847:
						it->mutable_effects(0)->set_triggerspell(28848);
						break;
					// T4 set bonus
					case 37288:
					case 37295:
						it->mutable_effects(0)->set_triggerspell(37238);
						break;
					// Maim Interrupt
					case 44835:	// druid pvp set bonus
					case 32748:	// rogue pvp hand effect
						it->mutable_effects(0)->set_triggerspell(32747);
						break;
						
					////////////////////////////// Creature Spells ///////////////////////////
					
					// Twisted Reflection
					case 21063:	// Doom Lord Kazzak boss spell
						it->mutable_effects(0)->set_triggerspell(21064);
						break;
					// Mark of Malice
					case 33493:	// schlabby trash spell
						it->mutable_effects(0)->set_triggerspell(33494);
						break;
					// Vampiric Aura
					case 38196:	// Anetheron boss spell
						it->mutable_effects(0)->set_triggerspell(31285);
						break;
						
					////////////////////////////// Item Effects //////////////////////////////
					
					// Hakkar Quest trinket effects
					case 24574:	// Brittle Armor - use effect of trinket (19948)
						it->mutable_effects(0)->set_triggerspell(24575);
						break;
					case 24658:	// Unstable Power - use effect of trinket (19950)
						it->mutable_effects(0)->set_triggerspell(24659);
						break;
					case 24661:	// Restless Strength - use effect of trinket (19949)
						it->mutable_effects(0)->set_triggerspell(24662);
						break;
					// Frozen Shadoweave (Shadow's Embrace set)
					case 39372:
						it->mutable_effects(0)->set_triggerspell(39373);
						break;
						
					///////////////////////// Fix Item Stats Effects /////////////////////////
						
					case 41684:	// fix for scarlet crusade set bonus
						auto effect = it->mutable_effects(1);
						effect->set_basepoints(14);
						effect->set_basedice(1);
						effect->set_diesides(1);
						effect->set_targeta(1);
						break;
						
					// PROBLEM: 28764 mage T3 set effect trigger: 28765 | 28766 | 28768 | 28769 | 28770	
					// PROBLEM: 27539 Obsidian Armor effect trigger: 27533 | 27534 | 27535 | 27536 | 27538 | 27540
					// PROBLEM: 39446 Aura of Madness (Darkmoon Card: Madness trinket)
					// PROBLEM: 45481 Sunwell Exalted Caster Neck (Shattered Sun Pendant of Acumen neck)
					// PROBLEM: 45482 Sunwell Exalted Melee Neck (Shattered Sun Pendant of Might neck)
					// PROBLEM: 45483 Sunwell Exalted Tank Neck (Shattered Sun Pendant of Resolve neck)
					// PROBLEM: 45484 Sunwell Exalted Healer Neck (Shattered Sun Pendant of Restoration neck)
					// PROBLEM: 46569 Sunwell Exalted Caster Neck (??? neck) // old unused version
					// PROBLEM: 40438 Priest T6 Trinket (Ashtongue Talisman) trigger: 40440 | 40441
					// PROBLEM: 40442 Druid T6 Trinket (Ashtongue Talisman) trigger: 40445 | 40446 | 40452
					// PROBLEM: 28789 Paladin T3 set bonus trigger: 28795 | 28793 | 28791 | 28790
					// PROBLEM: 40470 Paladin T6 trinket trigger: 40471 | 40472
					// PROBLEM: 28823 Shaman T3 set bonus trigger: 28824 | 28825 | 28826 | 28827
					// PROBLEM: 33757 Shaman windfury
					// PROBLEM: 40463 Shaman T6 trinket trigger: 40465 | 40465 | 40466
					// PROBLEM: Shaman: Lightning Overload
				}

				it++;
			}
		}

		return true;
	}

	static bool importSpellMechanics(proto::Project &project, MySQL::Connection &conn)
	{
		wowpp::MySQL::Select select(conn, "SELECT `Id`, `Mechanic` FROM `dbc_spell`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				// Get row data
				UInt32 id = 0, mechanic = 0;
				row.getField(0, id);
				row.getField(1, mechanic);

				// 0 = default mechanic
				if (mechanic != 0)
				{
					// Find spell by id
					auto * spell = project.spells.getById(id);
					if (spell)
					{
						spell->set_mechanic(mechanic);
					}
					else
					{
						WLOG("Unable to find spell by id: " << id);
					}
				}
				
				// Next row
				row = row.next(select);
			}
		}

		return true;
	}

	static bool importCategories(proto::Project &project, MySQL::Connection &conn)
	{
		project.spellCategories.clear();

		{
			wowpp::MySQL::Select select(conn, "SELECT DISTINCT `Category` FROM `dbc_spell`;");
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					UInt32 category = 0;
					row.getField(0, category);

					// Add category
					if (category != 0)
					{
						project.spellCategories.add(category);
					}

					row = row.next(select);
				}
			}
		}

		wowpp::MySQL::Select select(conn, "SELECT `Id`, `Category`, `CategoryRecoveryTime` FROM `dbc_spell`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				// Get row data
				UInt32 id = 0, category = 0, recoveryTime = 0;
				row.getField(0, id);
				row.getField(1, category);
				row.getField(2, recoveryTime);

				auto * spell = project.spells.getById(id);
				if (!spell)
				{
					WLOG("Unable to find spell by id: " << id);
					row = row.next(select);
					continue;
				}

				if (category != 0)
				{
					spell->set_category(category);

					// Add spells to category
					auto *cat = project.spellCategories.getById(category);
					if (!cat)
					{
						WLOG("Could not find category by id: " << category);
					}
					else
					{
						cat->add_spells(spell->id());
					}
				}
				if (recoveryTime != 0) spell->set_categorycooldown(recoveryTime);

				// Next row
				row = row.next(select);
			}
		}

		return true;
	}
}

/// Procedural entry point of the application.
int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// Add cout to the list of log output streams
	wowpp::g_DefaultLog.signal().connect(std::bind(
		wowpp::printLogEntry,
		std::ref(std::cout), std::placeholders::_1, wowpp::g_DefaultConsoleLogOptions));
	
	// Load the configuration
	wowpp::Configuration configuration;
	if (!configuration.load("proto_patcher.cfg"))
	{
		// Shutdown for now
		return false;
	}

	// The log files are written to in a special background thread
	std::ofstream logFile;
	LogStreamOptions logFileOptions = g_DefaultFileLogOptions;
	logFileOptions.alwaysFlush = !configuration.isLogFileBuffering;

	boost::signals2::scoped_connection genericLogConnection;
	if (configuration.isLogActive)
	{
		const String fileName = configuration.logFileName;
		logFile.open(fileName.c_str(), std::ios::app);
		if (logFile)
		{
			genericLogConnection = g_DefaultLog.signal().connect(
				[&logFile, &logFileOptions](const LogEntry & entry)
			{
				printLogEntry(
					logFile,
					entry,
					logFileOptions);
			});
		}
		else
		{
			ELOG("Could not open log file '" << fileName << "'");
		}
	}

	// Load existing project
	wowpp::proto::Project protoProject;
	if (!protoProject.load(configuration.dataPath))
	{
		return 1;
	}

	// Load all spell mechanics
	MySQL::DatabaseInfo connectionInfo(configuration.mysqlHost, configuration.mysqlPort, configuration.mysqlUser, configuration.mysqlPassword, configuration.mysqlDatabase);
	MySQL::Connection connection;
	if (!connection.connect(connectionInfo))
	{
		ELOG("Could not connect to the database");
		ELOG(connection.getErrorMessage());
		return 0;
	}
	else
	{
		ILOG("MySQL connection established!");
	}

	if (!importSpellMechanics(protoProject, connection))
	{
		ELOG("Failed to import spell mechanics");
		return 1;
	}

	if (!importCategories(protoProject, connection))
	{
		ELOG("Failed to import spell categories");
		return 1;
	}

	if (!addSpellLinks(protoProject))
	{
		ELOG("Failed to add spell links");
		return 1;
	}

	// Save project
	if (!protoProject.save(configuration.dataPath))
	{
		return 1;
	}

	// Wait for user input to finish
	std::cin.get();

	// Shutdown protobuf and free all memory (optional)
	google::protobuf::ShutdownProtobufLibrary();

	return 0;
}
