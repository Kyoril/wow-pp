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
#include <iomanip>
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
					// Blood Rage: Costs Health, not Mana
					case 2687:
						it->set_powertype(game::power_type::Health);
						break;

						//////////////////////////////////////////////////////////////////////////
						// Hemorrhage: Add Combo Point (Needed because this spell already has 
						// 3 / 3 effects, and there is no space left for the AddComboPoint effect)
					case 16511:	// Rank 1
					case 17347:	// Rank 2
					case 17348:	// Rank 3
					case 26864:	// Rank 4
						it->add_additionalspells(34071);	// Combo Point
						break;

						//////////////////////////////////////////////////////////////////////////
						// Mangle (Cat): Add Combo Point (Needed because this spell already has 
						// 3 / 3 effects, and there is no space left for the AddComboPoint effect)
					case 33876:	// Rank 1
					case 33982:	// Rank 2
					case 33983:	// Rank 3
						it->add_additionalspells(34071);	// Combo Point
						break;

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
						// Devastate
					case 20243:	// Rank 1
						it->add_additionalspells(11596);
						break;
					case 30016:	// Rank 2
						it->add_additionalspells(11597);
						break;
					case 30022:	// Rank 3
						it->add_additionalspells(25225);
						break;

						//////////////////////////////////////////////////////////////////////////
						/////////////////////////// SPELLS CHANGES ///////////////////////////////
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

						////////////////////////////// Shaman ////////////////////////////////////

						// Rockbiter Weapon Enchantment
					case  8017:	// Rank 1
						it->add_additionalspells(36494);
						break;
					case  8018:	// Rank 2
						it->add_additionalspells(36750);
						break;
					case  8019:	// Rank 3
						it->add_additionalspells(36755);
						break;
					case 10399:	// Rank 4
						it->add_additionalspells(36759);
						break;
					case 16314:	// Rank 5
						it->add_additionalspells(36763);
						break;
					case 16315:	// Rank 6
						it->add_additionalspells(36766);
						break;
					case 16316:	// Rank 7
						it->add_additionalspells(36771);
						break;
					case 25479:	// Rank 8
						it->add_additionalspells(36775);
						break;
					case 25485:	// Rank 9
						it->add_additionalspells(36499);
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
						// Blessed Recovery
					case 27811:
						it->mutable_effects(0)->set_triggerspell(27813);
						break;
					case 27815:
						it->mutable_effects(0)->set_triggerspell(27817);
						break;
					case 27816:
						it->mutable_effects(0)->set_triggerspell(27818);
						break;
						// Touch of Weakness
					case 2652:
						it->mutable_effects(0)->set_triggerspell(2943);
						break;
					case 19261:
						it->mutable_effects(0)->set_triggerspell(19249);
						break;
					case 19262:
						it->mutable_effects(0)->set_triggerspell(19251);
						break;
					case 19264:
						it->mutable_effects(0)->set_triggerspell(19252);
						break;
					case 19265:
						it->mutable_effects(0)->set_triggerspell(19253);
						break;
					case 19266:
						it->mutable_effects(0)->set_triggerspell(19254);
						break;
					case 25461:
						it->mutable_effects(0)->set_triggerspell(25460);
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
						// Blazing Speed (fire talent)
					case 31641:
					case 31642:
						it->mutable_effects(0)->set_triggerspell(31643);
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

						// Enrage
					case 5229:
						it->mutable_effects(1)->set_basedice(0);
						it->mutable_effects(1)->set_diesides(0);
						it->mutable_effects(1)->set_targeta(1);
						it->mutable_effects(1)->set_aura(142);
						it->mutable_effects(1)->set_miscvaluea(1);
						break;
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
						// Pounce
					case 9005:
					case 9823:
					case 9827:
					case 27006:
						it->set_attributes(2, 0);
						break;
						// Savage Fury
					case 16998:
					case 16999:
						it->mutable_effects(2)->set_affectmask(4398046511104);
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

					case 20606:
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

	static bool checkSpells(proto::Project &project, MySQL::Connection &conn)
	{
		wowpp::MySQL::Select select(conn, "SELECT `entry`,`effectId`, `SpellFamilyMask` FROM `mangos`.`spell_affect`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			auto *spellEntries = project.spells.getTemplates().mutable_entry();
			while (row)
			{
				// Get row data
				UInt32 entry = 0, effectId = 0, index = 0;
				UInt64 familyFlags = 0;
				row.getField(index++, entry);
				row.getField(index++, effectId);
				row.getField(index++, familyFlags);
				
				if (spellEntries)
				{
					auto it = spellEntries->begin();
					while (it != spellEntries->end())
					{
						for (const auto & effect : it->effects())
						{
							if (it->id() == entry &&
								effect.index() == effectId &&
								effect.affectmask() != familyFlags)
							{
								DLOG("difference is " << familyFlags);
								DLOG("id is " << it->id());
							}
						}
						it++;
					}
				}

				row = row.next(select);
			}
		}
		return true;
	}

	static bool addSpellBaseId(proto::Project &project)
	{
		auto &tpls = project.spells.getTemplates();
		for (int i = 0; i < tpls.entry_size(); ++i)
		{
			auto *entry = tpls.mutable_entry(i);
			if (entry)
			{
				entry->set_baseid(entry->id());
			}
		}

		return true;
	}

	static bool addItemSkill(proto::Project &project)
	{
		namespace st = game::skill_type;

		const UInt32 itemWeaponSkills[21] =
		{
			st::Axes,     st::TwoHAxes,  st::Bows,          st::Guns,      st::Maces,
			st::TwoHMaces, st::Polearms, st::Swords,        st::TwoHSwords, 0,
			st::Staves,   0,              0,                   st::Unarmed,   0,
			st::Daggers,  st::Thrown,   st::Assassination, st::Crossbows, st::Wands,
			st::Fishing
		};

		const UInt32 itemArmorSkills[10] =
		{
			0, st::Cloth, st::Leather, st::Mail, st::PlateMail, 0, st::Shield, 0, 0, 0
		};

		auto &tpls = project.items.getTemplates();
		for (int i = 0; i < tpls.entry_size(); ++i)
		{
			auto *entry = tpls.mutable_entry(i);
			if (entry)
			{
				switch (entry->itemclass())
				{
					case game::item_class::Weapon:
						if (entry->subclass() >= 21)
							entry->set_skill(0);
						else
							entry->set_skill(itemWeaponSkills[entry->subclass()]);
						break;
					case game::item_class::Armor:
						if (entry->subclass() >= 10)
							entry->set_skill(0);
						else
							entry->set_skill(itemArmorSkills[entry->subclass()]);
						break;
					default:
						entry->set_skill(0);
				}
			}
		}

		return true;
	}

	static UInt32 findMatchingRankOneSpell(const proto::SpellEntry &spell, const std::vector<proto::SpellEntry*> &rankOneSpells)
	{
		std::set<UInt32> blacklist;

		// Now check different things
		for (auto *check : rankOneSpells)
		{
			// Family name check
			if (check->family() != spell.family())
			{
				blacklist.insert(check->id());
				continue;
			}

			// Family flag check
			if (check->familyflags() != spell.familyflags())
			{
				blacklist.insert(check->id());
				continue;
			}

			// Compare number of effects
			if (check->effects_size() != spell.effects_size())
			{
				blacklist.insert(check->id());
				continue;
			}

			// Now compare effects details
			for (int i = 0; i < check->effects_size(); ++i)
			{
				auto &eff_a = check->effects(i);
				auto &eff_b = spell.effects(i);

				// Compare effect type
				if (eff_a.type() != eff_b.type())
				{
					blacklist.insert(check->id());
					continue;
				}

				// Compare aura name
				if (eff_a.aura() != eff_b.aura())
				{
					blacklist.insert(check->id());
					continue;
				}

				// Some checks left? ...
			}
		}

		// Check if all spells are blacklisted
		if (blacklist.size() >= rankOneSpells.size())
		{
			return 0;
		}

		// Check if we have multiple choices left
		if (blacklist.size() < rankOneSpells.size() - 1)
		{
			return 0;
		}

		// Check all remaining spells against the blacklist
		for (auto *check : rankOneSpells)
		{
			if (blacklist.find(check->id()) == blacklist.end())
			{
				return check->id();
			}
		}

		// Huh? Should be unreachable...
		return 0;
	}

	static bool importSpellBaseIds(proto::Project &project, MySQL::Connection &conn)
	{
		std::map<String, std::map<UInt32, std::vector<proto::SpellEntry*>>> spellRanks;

		wowpp::MySQL::Select select(conn, "SELECT Id AS `id`,SpellName4 AS `name`,CONVERT(RIGHT(Rank4, 2), UNSIGNED INTEGER) AS `rank` FROM dbc_spell WHERE LEFT(Rank4, 5) = \"Rang \" ORDER BY `name`,`rank`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				// Get row data
				UInt32 id = 0, rank = 0;
				String name;
				row.getField(0, id);
				row.getField(1, name);
				row.getField(2, rank);

				// Find spell by id
				auto * spell = project.spells.getById(id);
				if (spell)
				{
					spellRanks[name][rank].push_back(spell);
				}
				else
				{
					WLOG("Unable to find spell by id: " << id);
				}

				// Next row
				row = row.next(select);
			}
		}

		UInt32 spellCounter = 0;
		std::set<UInt32> fixedSpells;

		// Now output this (just for testings)
		std::ofstream strmOut("test.txt", std::ios::out);
		std::ofstream strmFixed("fixed.txt", std::ios::out);
		for (const auto &pair : spellRanks)
		{
			// Skip spells with only one rank or without rank 1
			auto rank1 = pair.second.find(1);
			if (rank1 == pair.second.end())
			{
				DLOG("Skipping spell " << pair.first << " because rank 1 is missing");
				continue;
			}
			else if (pair.second.size() == 1)
			{
				DLOG("Skipping spell " << pair.first << " because it only has one rank");
				continue;
			}

			// Next, fix all spells which do have exactly one spell for every rank
			// These are most likely perfect
			{
				bool hasMultipleSpells = false;
				for (auto &ranks : pair.second)
				{
					if (ranks.second.size() > 1)
					{
						hasMultipleSpells = true;
						break;
					}
				}

				if (!hasMultipleSpells)
				{
					// Fix all of these
					strmFixed << "Spell " << std::setw(5) << rank1->second[0]->id() << " Ranks: ";
					for (auto &ranks : pair.second)
					{
						strmFixed << std::setw(5) << ranks.second[0]->id() << ", ";
						ranks.second[0]->set_baseid(rank1->second[0]->id());
					}
					strmFixed << std::endl;

					continue;
				}
			}

			// Now, check remaining spells
			strmOut << "// Spell name: " << pair.first << std::endl;
			strmFixed << std::endl << "// Uncomplete or potentially wrong spells below! BE EXTREMELY CAUTIOS!" << std::endl;
			strmFixed << "Spell " << std::setw(5) << rank1->second[0]->id() << " Ranks: ";
			for (auto &ranks : pair.second)
			{
				bool rankWritten = false;
				// Write spell rank
				for (auto *spell : ranks.second)
				{
					// Check upper rang spells and find a base spell for these
					bool foundBaseRank = false;
					if (ranks.first > 1)
					{
						// Find rank 1 of this spell
						UInt32 baseRank = findMatchingRankOneSpell(*spell, rank1->second);
						if (baseRank != 0)
						{
							spell->set_baseid(baseRank);
							strmFixed << std::setw(5) << spell->id() << ", ";
							foundBaseRank = true;
						}
						else
						{
							if (!rankWritten)
							{
								strmOut << "\tRank " << ranks.first << std::endl;
								rankWritten = true;
							}
						}
					}
					else
					{
						strmFixed << std::setw(5) << ranks.second[0]->id() << ", ";
					}

					if (!foundBaseRank)
					{
						if (!rankWritten)
						{
							strmOut << "\tRank " << ranks.first << std::endl;
							rankWritten = true;
						}

						strmOut << "\t\tSpell " << spell->id() << "\t\t";
						strmOut << "\t\tFAM " << std::setw(3) << spell->family() << "\t\tFLAG " << std::setw(16) << std::hex << std::uppercase << spell->familyflags() << std::dec << "\t\tEFF ";
						for (const auto &e : spell->effects())
						{
							strmOut << e.type();
							if (e.type() == game::spell_effects::ApplyAura)
							{
								strmOut << "(" << e.aura() << ")";
							}
							strmOut << "\t";
						}
						strmOut << std::endl;
					}
				}
			}
			strmOut << std::endl;
			strmFixed << std::endl;

			spellCounter++;
		}

		strmOut << spellCounter << " entries";

		return true;
	}

	static bool importSpellMultipliers(proto::Project &project, MySQL::Connection &conn)
	{
		wowpp::MySQL::Select select(conn, "SELECT `Id`, `DmgMultiplier1`, `DmgMultiplier3`, `DmgMultiplier3` FROM `dbc_spell`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				// Get row data
				UInt32 id = 0;
				float m1, m2, m3;
				row.getField(0, id);
				row.getField(1, m1);
				row.getField(1, m2);
				row.getField(1, m3);

				// Find spell by id
				auto * spell = project.spells.getById(id);
				if (spell)
				{
					if (spell->effects_size() >= 1) spell->mutable_effects(0)->set_dmgmultiplier(m1);
					if (spell->effects_size() >= 2) spell->mutable_effects(1)->set_dmgmultiplier(m2);
					if (spell->effects_size() >= 3) spell->mutable_effects(2)->set_dmgmultiplier(m3);
				}
				else
				{
					WLOG("Unable to find spell by id: " << id);
				}

				// Next row
				row = row.next(select);
			}
		}

		return true;
	}

	static bool importSpellChannelInterrupt(proto::Project &project, MySQL::Connection &conn)
	{
		wowpp::MySQL::Select select(conn, "SELECT `Id`, `ChannelInterruptFlags` FROM `dbc_spell`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				// Get row data
				UInt32 id = 0, channelInterruptFlags = 0;
				row.getField(0, id);
				row.getField(1, channelInterruptFlags);

				// Find spell by id
				auto * spell = project.spells.getById(id);
				if (spell)
				{
					spell->set_channelinterruptflags(channelInterruptFlags);
				}
				else
				{
					WLOG("Unable to find spell by id: " << id);
				}

				// Next row
				row = row.next(select);
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

	static bool importUnitMechanicImmunities(proto::Project &project, MySQL::Connection &conn)
	{
		wowpp::MySQL::Select select(conn, "SELECT `entry`,`mechanic_immune_mask` FROM `tbcdb`.`creature_template`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				// Get row data
				UInt32 entry = 0, mask = 0;
				row.getField(0, entry);
				row.getField(1, mask);

				if (mask != 0)
				{
					// Find unit by id
					auto * unit = project.units.getById(entry);
					if (unit)
					{
						unit->set_mechanicimmunity(mask);
					}
					else
					{
						WLOG("Unable to find unit by id: " << entry);
					}
				}

				// Next row
				row = row.next(select);
			}
		}

		return true;
	}

	static bool importSpellStackAmount(proto::Project &project, MySQL::Connection &conn)
	{
		wowpp::MySQL::Select select(conn, "SELECT `Id`, `StackAmount` FROM `dbc_spell`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				// Get row data
				UInt32 id = 0, stackAmount = 0;
				row.getField(0, id);
				row.getField(1, stackAmount);

				// Find spell by id
				auto * spell = project.spells.getById(id);
				if (spell)
				{
					spell->set_stackamount(stackAmount);
				}
				else
				{
					WLOG("Unable to find spell by id: " << id);
				}

				// Next row
				row = row.next(select);
			}
		}

		return true;
	}

	static bool importAffectMask(proto::Project &project, MySQL::Connection &conn)
	{
		wowpp::MySQL::Select select(conn, "SELECT `entry`, `effectId`, `SpellFamilyMask` FROM `world`.`spell_affect`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				// Get row data
				UInt32 id = 0, effectId = 0;
				UInt64 familyFlags = 0;
				row.getField(0, id);
				row.getField(1, effectId);
				row.getField(2, familyFlags);

				// Find spell by id
				auto * spell = project.spells.getById(id);
				if (spell)
				{
					spell->mutable_effects(effectId)->set_affectmask(familyFlags);
				}
				else
				{
					WLOG("Unable to find spell by id: " << id);
				}

				// Next row
				row = row.next(select);
			}
		}

		return true;
	}

	static bool importCombatRatings(proto::Project &project, MySQL::Connection &conn)
	{
		wowpp::MySQL::Select select(conn, "SELECT `field0` FROM `tbcdb`.`dbc_gtcombatratings`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			UInt32 id = 0;
			while (row)
			{
				// Get row data
				float crMultiplier = 0;
				row.getField(0, crMultiplier);

				auto * combatRatings = project.combatRatings.getById(id);
				if (combatRatings)
				{
					combatRatings->clear_ratingsperlevel();
					combatRatings->set_ratingsperlevel(crMultiplier);
				}
				else
				{
					WLOG("Unable to combatRating by id: " << id);
				}

				// Next row
				id++;
				row = row.next(select);
			}
		}

		return true;
	}

	static bool importMeleeCritChance(proto::Project &project, MySQL::Connection &conn)
	{
		wowpp::MySQL::Select select(conn, "SELECT `field0` FROM `tbcdb`.`dbc_gtchancetomeleecrit`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);

			UInt32 id = 0;
			float meleeCritChance = 0;
			std::array<float, 11> meleeCritChanceBase = 
			{
				0.0114f,
				0.00652f,
				-0.01532f,
				-0.00295f,
				0.03183f,
				0.2f,
				0.01675f,
				0.034575f,
				0.02f,
				0.2f,
				0.00961f
			};

			for (int i = 0; i < 11; ++i)
			{
				for (int j = 0; j < 100; ++j)
				{
					project.meleeCritChance.getById(i * 100 + j)->set_basechanceperlevel(meleeCritChanceBase[i]);
				}
			}

			while (row)
			{
				// Get row data
				row.getField(0, meleeCritChance);

				auto * meleeCritChances = project.meleeCritChance.getById(id);
				if (meleeCritChances)
				{
					meleeCritChances->clear_chanceperlevel();
					meleeCritChances->set_chanceperlevel(meleeCritChance);
				}
				else
				{
					WLOG("Unable to meleeCritChance by id: " << id);
				}

				// Next row
				id++;
				row = row.next(select);
			}
		}

		return true;
	}

	static bool importSpellCritChance(proto::Project &project, MySQL::Connection &conn)
	{
		wowpp::MySQL::Select select(conn, "SELECT `field0` FROM `tbcdb`.`dbc_gtchancetospellcrit`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);

			UInt32 id = 0;
			float spellCritChance = 0;
			std::array<float, 11> spellCritChanceBase =
			{
				0.0f,
				0.033355f,
				0.03602f,
				0.0f,
				0.012375f,
				0.2f,
				0.02201f,
				0.009075f,
				0.017f,
				0.2f,
				0.018515f
			};

			for (int i = 0; i < 11; ++i)
			{
				for (int j = 0; j < 100; ++j)
				{
					project.spellCritChance.getById(i * 100 + j)->set_basechanceperlevel(spellCritChanceBase[i]);
				}
			}

			while (row)
			{
				// Get row data
				row.getField(0, spellCritChance);

				auto * spellCritChances = project.spellCritChance.getById(id);
				if (spellCritChances)
				{
					spellCritChances->clear_chanceperlevel();
					spellCritChances->set_chanceperlevel(spellCritChance);
				}
				else
				{
					WLOG("Unable to spellCritChance by id: " << id);
				}

				// Next row
				id++;
				row = row.next(select);
			}
		}

		return true;
	}

	static bool importDodgeChance(proto::Project &project)
	{
		std::array<float, 11> dodgeChanceBase =
		{
			0.0075f,   // Warrior
			0.00652f,  // Paladin
			-0.0545f,   // Hunter
			-0.0059f,   // Rogue
			0.03183f,  // Priest
			0.0114f,   // DK
			0.0167f,   // Shaman
			0.034575f, // Mage
			0.02011f,  // Warlock
			0.0f,      // ??
			-0.0187f    // Druid
		};

		std::array<float, 11> critToDodge =
		{
			1.1f,      // Warrior
			1.0f,      // Paladin
			1.6f,      // Hunter
			2.0f,      // Rogue
			1.0f,      // Priest
			1.0f,      // DK?
			1.0f,      // Shaman
			1.0f,      // Mage
			1.0f,      // Warlock
			0.0f,      // ??
			1.7f       // Druid
		};

		for (int i = 0; i < 11; ++i)
		{
			auto * dodgeChances = project.dodgeChance.getById(i);
			dodgeChances->set_basedodge(dodgeChanceBase[i]);
			dodgeChances->set_crittododge(critToDodge[i]);
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

	static bool importQuestRelations(proto::Project &project, MySQL::Connection &conn)
	{
		// Remove all quest associations
		for (auto &creature : *project.units.getTemplates().mutable_entry())
		{
			creature.clear_quests();
			creature.clear_end_quests();
		}
		for (auto &object : *project.objects.getTemplates().mutable_entry())
		{
			object.clear_quests();
			object.clear_end_quests();
		}

		// Get creature quest relation
		{
			wowpp::MySQL::Select select(conn, "SELECT `id`,`quest` FROM `tbcdb`.`creature_questrelation`;");
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					UInt32 creatureId = 0, questId = 0;
					row.getField(0, creatureId);
					row.getField(1, questId);

					// Find quest
					if (!project.quests.getById(questId))
					{
						row = row.next(select);
						continue;
					}

					// Find creature
					auto *creature = project.units.getById(creatureId);
					if (creature)
					{
						creature->add_quests(questId);
					}

					// Next row
					row = row.next(select);
				}
			}
		}
		// Get creature end quest relation
		{
			wowpp::MySQL::Select select(conn, "SELECT `id`,`quest` FROM `tbcdb`.`creature_involvedrelation`;");
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					UInt32 creatureId = 0, questId = 0;
					row.getField(0, creatureId);
					row.getField(1, questId);

					// Find quest
					if (!project.quests.getById(questId))
					{
						row = row.next(select);
						continue;
					}

					// Find creature
					auto *creature = project.units.getById(creatureId);
					if (creature)
					{
						creature->add_end_quests(questId);
					}

					// Next row
					row = row.next(select);
				}
			}
		}

		// And one more time for objects
		{
			wowpp::MySQL::Select select(conn, "SELECT `id`,`quest` FROM `tbcdb`.`gameobject_questrelation`;");
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					UInt32 objectId = 0, questId = 0;
					row.getField(0, objectId);
					row.getField(1, questId);

					// Find quest
					if (!project.quests.getById(questId))
					{
						row = row.next(select);
						continue;
					}

					// Find object
					auto *object = project.objects.getById(objectId);
					if (object)
					{
						object->add_quests(questId);
					}

					// Next row
					row = row.next(select);
				}
			}
		}
		// And one more time for objects
		{
			wowpp::MySQL::Select select(conn, "SELECT `id`,`quest` FROM `tbcdb`.`gameobject_involvedrelation`;");
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					UInt32 objectId = 0, questId = 0;
					row.getField(0, objectId);
					row.getField(1, questId);

					// Find quest
					if (!project.quests.getById(questId))
					{
						row = row.next(select);
						continue;
					}

					// Find object
					auto *object = project.objects.getById(objectId);
					if (object)
					{
						object->add_end_quests(questId);
					}

					// Next row
					row = row.next(select);
				}
			}
		}

		return true;
	}

	static bool importDispelData(proto::Project &project, MySQL::Connection &conn)
	{
		wowpp::MySQL::Select select(conn, "SELECT `Id`,`Dispel`,`SpellFamilyName`,`SpellFamilyFlags` FROM `dbc_spell`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				// Get row data
				UInt32 id = 0, dispel = 0;
				UInt64 family = 0, familyFlags = 0;
				row.getField(0, id);
				row.getField(1, dispel);
				row.getField(2, family);
				row.getField(3, familyFlags);

				auto * spell = project.spells.getById(id);
				if (!spell)
				{
					WLOG("Unable to find spell by id: " << id);
					row = row.next(select);
					continue;
				}

				spell->set_dispel(dispel);
				spell->set_family(family);
				spell->set_familyflags(familyFlags);

				row = row.next(select);
			}
		}

		return true;
	}

	static bool importTrainerLinks(proto::Project &project, MySQL::Connection &conn)
	{
		wowpp::MySQL::Select select(conn, "SELECT `entry`,`trainer_id`, `trainer_type`,`trainer_class` FROM `tbcdb`.`creature_template` WHERE `trainer_id` != 0;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				// Get row data
				UInt32 entry = 0, trainerid = 0, trainertype = 0, trainerclass = 0;
				row.getField(0, entry);
				row.getField(1, trainerid);
				row.getField(2, trainertype);
				row.getField(3, trainerclass);
				
				auto * trainer = project.trainers.getById(trainerid);
				if (!trainer)
				{
					WLOG("Unable to find trainer by id: " << trainerid);
					row = row.next(select);
					continue;
				}

				trainer->set_type(static_cast<proto::TrainerEntry_TrainerType>(trainertype));
				trainer->set_classid(trainerclass);
				trainer->set_title("");

				auto *unit = project.units.getById(entry);
				if (!unit)
				{
					WLOG("Unable to find unit by id: " << entry);
					row = row.next(select);
					continue;
				}

				unit->set_trainerentry(trainerid);
				row = row.next(select);
			}
		}
		else
		{
			ELOG("Error: " << conn.getErrorMessage());
		}

		return true;
	}

	static bool importItemQuests(proto::Project &project, MySQL::Connection &conn)
	{
		for (auto &item : *project.items.getTemplates().mutable_entry())
		{
			item.clear_questentry();
		}

		wowpp::MySQL::Select select(conn, "SELECT `entry`,`startquest` FROM `tbcdb`.`item_template` WHERE `startquest` != 0;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				// Get row data
				UInt32 entry = 0, questEntry = 0;
				row.getField(0, entry);
				row.getField(1, questEntry);

				auto * item = project.items.getById(entry);
				if (!item)
				{
					WLOG("Unable to find item by id: " << entry);
					row = row.next(select);
					continue;
				}

				const auto *quest = project.quests.getById(questEntry);
				if (!quest)
				{
					WLOG("Unable to find quest by id: " << questEntry);
					row = row.next(select);
					continue;
				}

				item->set_questentry(questEntry);
				row = row.next(select);
			}
		}
		else
		{
			ELOG("Error: " << conn.getErrorMessage());
		}

		return true;
	}

	static bool importItemSets(proto::Project &project, MySQL::Connection &conn)
	{
		project.itemSets.clear();

		wowpp::MySQL::Select select(conn, "SELECT `id`,`name4`,`spell1`,`count1`,`spell2`,`count2`,`spell3`,`count3`,`spell4`,`count4`,`spell5`,`count5`,`spell6`,`count6`,`spell7`,`count7`,`spell8`,`count8` FROM `dbc_itemset` ORDER BY `id`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				// Get row data
				UInt32 entry = 0, index = 0;
				String name;
				row.getField(index++, entry);
				row.getField(index++, name);

				auto *added = project.itemSets.add(entry);
				if (!added)
				{
					ELOG("Failed to add item set");
					return false;
				}

				added->set_name(name);

				for (UInt32 i = 0; i < 8; ++i)
				{
					UInt32 spellId = 0, count = 0;
					row.getField(index++, spellId);
					row.getField(index++, count);

					if (spellId == 0 || count == 0)
					{
						continue;
					}

					if (!project.spells.getById(spellId))
					{
						WLOG("Could not find spell by id " << spellId << ": Referenced by item set " << entry << " in slot " << i);
						continue;
					}

					auto *addedSpell = added->add_spells();
					if (!addedSpell)
					{
						ELOG("Could not add item set spell entry");
						return false;
					}

					addedSpell->set_spell(spellId);
					addedSpell->set_itemcount(count);
				}

				row = row.next(select);
			}
		}
		else
		{
			ELOG("Error: " << conn.getErrorMessage());
		}

		return true;
	}

	static bool importSpawnMovement(proto::Project &project, MySQL::Connection &conn)
	{
		for (auto &map : *project.maps.getTemplates().mutable_entry())
		{
			for (auto &spawn : *map.mutable_unitspawns())
			{
				spawn.clear_waypoints();
				spawn.clear_movement();
			}
		}

		wowpp::MySQL::Select select(conn, "SELECT `entry`,`MovementType` FROM `tbcdb`.`creature_template` WHERE `MovementType` != 0 ORDER BY `entry`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				// Get row data
				UInt32 entry = 0, movement = 0, index = 0;
				row.getField(index++, entry);
				row.getField(index++, movement);

				for (auto &map : *project.maps.getTemplates().mutable_entry())
				{
					for (auto &spawn : *map.mutable_unitspawns())
					{
						if (spawn.unitentry() == entry)
						{
							spawn.set_movement(movement);
						}
					}
				}

				row = row.next(select);
			}
		}
		else
		{
			ELOG("Error: " << conn.getErrorMessage());
		}

		wowpp::MySQL::Select select2(conn, "SELECT `entry`,`point`,`position_x`,`position_y`,`position_z`,`waittime` FROM `tbcdb`.`creature_movement_template` ORDER BY `entry`, `point`;");
		if (select2.success())
		{
			wowpp::MySQL::Row row(select2);
			while (row)
			{
				// Get row data
				UInt32 entry = 0, point = 0, index = 0, delay = 0;
				float x, y, z;
				row.getField(index++, entry);
				row.getField(index++, point);
				row.getField(index++, x);
				row.getField(index++, y);
				row.getField(index++, z);
				row.getField(index++, delay);

				for (auto &map : *project.maps.getTemplates().mutable_entry())
				{
					for (auto &spawn : *map.mutable_unitspawns())
					{
						if (spawn.unitentry() == entry)
						{
							auto *added = spawn.add_waypoints();
							added->set_positionx(x);
							added->set_positiony(y);
							added->set_positionz(z);
							added->set_waittime(delay);
						}
					}
				}

				row = row.next(select2);
			}
		}
		else
		{
			ELOG("Error: " << conn.getErrorMessage());
		}

		return true;
	}

	static bool importAreaTrigger(proto::Project &project, MySQL::Connection &conn)
	{
		{
			wowpp::MySQL::Select select(conn, "SELECT `id`,`map`,`x`,`y`,`z`,`radius`,`box_x`,`box_y`,`box_z`,`box_o` FROM `dbc`.`dbc_areatrigger` ORDER BY `id`;");
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					// Get row data
					UInt32 entry = 0, map = 0, index = 0;
					float x = 0.0f, y = 0.0f, z = 0.0f, radius = 0.0f, boxx = 0.0f, boxy = 0.0f, boxz = 0.0f, boxo = 0.0f;
					row.getField(index++, entry);
					row.getField(index++, map);
					row.getField(index++, x);
					row.getField(index++, y);
					row.getField(index++, z);
					row.getField(index++, radius);
					row.getField(index++, boxx);
					row.getField(index++, boxy);
					row.getField(index++, boxz);
					row.getField(index++, boxo);
					
					auto *trigger = project.areaTriggers.getById(entry);
					if (!trigger)
					{
						trigger = project.areaTriggers.add(entry);
						if (!trigger)
						{
							WLOG("Could not add area trigger " << entry);
							row = row.next(select);
							continue;
						}

						// New trigger is not a teleport trigger
						trigger->set_name("Unnamed");
						trigger->set_targetmap(0);
						trigger->set_target_x(0.0f);
						trigger->set_target_y(0.0f);
						trigger->set_target_z(0.0f);
						trigger->set_target_o(0.0f);
					}

					trigger->clear_questid();
					trigger->clear_tavern();
					trigger->set_map(map);
					trigger->set_x(x);
					trigger->set_y(y);
					trigger->set_z(z);
					if (radius != 0.0f) trigger->set_radius(radius);
					if (boxx != 0.0f) trigger->set_box_x(boxx);
					if (boxx != 0.0f) trigger->set_box_y(boxy);
					if (boxx != 0.0f) trigger->set_box_z(boxz);
					if (boxx != 0.0f) trigger->set_box_o(boxo);
					row = row.next(select);
				}
			}
			else
			{
				ELOG("Error: " << conn.getErrorMessage());
			}
		}

		{
			wowpp::MySQL::Select select(conn, "SELECT `id`,`quest` FROM `tbcdb`.`areatrigger_involvedrelation` ORDER BY `id`;");
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					// Get row data
					UInt32 entry = 0, quest = 0, index = 0;
					row.getField(index++, entry);
					row.getField(index++, quest);

					auto *trigger = project.areaTriggers.getById(entry);
					if (!trigger)
					{
						WLOG("Could not find referenced quest trigger " << entry);
						row = row.next(select);
						continue;
					}

					auto *questEntry = project.quests.getById(quest);
					if (!questEntry)
					{
						WLOG("Could not find quest " << quest << " referenced by area trigger " << entry);
						row = row.next(select);
						continue;
					}

					trigger->set_questid(quest);
					row = row.next(select);
				}
			}
			else
			{
				ELOG("Error: " << conn.getErrorMessage());
			}
		}

		{
			wowpp::MySQL::Select select(conn, "SELECT `id` FROM `tbcdb`.`areatrigger_tavern` ORDER BY `id`;");
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					// Get row data
					UInt32 entry = 0, index = 0;
					row.getField(index++, entry);

					auto *trigger = project.areaTriggers.getById(entry);
					if (!trigger)
					{
						WLOG("Could not find referenced tavern trigger " << entry);
						row = row.next(select);
						continue;
					}

					trigger->set_tavern(true);
					row = row.next(select);
				}
			}
			else
			{
				ELOG("Error: " << conn.getErrorMessage());
			}
		}

		return true;
	}

	static bool importSpellFocus(proto::Project &project, MySQL::Connection &conn)
	{
		{
			wowpp::MySQL::Select select(conn, "SELECT `Id`,`RequiresSpellFocus` FROM `dbc`.`dbc_spell` WHERE `RequiresSpellFocus` != 0 ORDER BY `Id`;");
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					// Get row data
					UInt32 entry = 0, focus = 0, index = 0;
					row.getField(index++, entry);
					row.getField(index++, focus);

					auto *spell = project.spells.getById(entry);
					if (!spell)
					{
						WLOG("Could not find spell " << entry);
						row = row.next(select);
						continue;
					}

					spell->set_focusobject(focus);
					row = row.next(select);
				}
			}
			else
			{
				ELOG("Error: " << conn.getErrorMessage());
			}
		}

		return true;
	}

	static bool importSpellFamilyFlags(proto::Project &project, MySQL::Connection &conn)
	{
		wowpp::MySQL::Select select(conn, "SELECT `Id`, `SpellFamilyFlags` FROM `dbc_spell`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				// Get row data
				UInt32 id = 0;
				UInt64 familyFlags = 0;
				row.getField(0, id);
				row.getField(1, familyFlags);

				// Find spell by id
				auto * spell = project.spells.getById(id);
				if (spell)
				{
					spell->set_familyflags(familyFlags);
				}
				else
				{
					WLOG("Unable to find spell by id: " << id);
				}

				// Next row
				row = row.next(select);
			}
		}

		return true;
	}

	static bool importSpellProcs(proto::Project &project, MySQL::Connection &conn)
	{
		{
			wowpp::MySQL::Select select(conn, "SELECT `entry`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `procEx`, `ppmRate`, `Cooldown` FROM `tbcdb`.`spell_proc_event`;");
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					// Get row data
					UInt64 entry = 0, schoolmask = 0, spellfamilyname = 0, spellfamilymask = 0, procex = 0, ppmrate = 0, cooldown = 0, index = 0;
					row.getField(index++, entry);
					row.getField(index++, schoolmask);
					row.getField(index++, spellfamilyname);
					row.getField(index++, spellfamilymask);
					row.getField(index++, procex);
					row.getField(index++, ppmrate);
					row.getField(index++, cooldown);

					auto *spell = project.spells.getById(entry);
					if (!spell)
					{
						WLOG("Could not find spell " << entry);
						row = row.next(select);
						continue;
					}

					spell->set_procschool(schoolmask);
					spell->set_procfamily(spellfamilyname);
					spell->set_procfamilyflags(spellfamilymask);
					spell->set_procexflags(procex);
					spell->set_procpermin(ppmrate);
					spell->set_proccooldown(cooldown);

					row = row.next(select);
				}
			}
			else
			{
				ELOG("Error: " << conn.getErrorMessage());
			}
		}

		return true;
	}

	static bool importSpellNames(proto::Project &project, MySQL::Connection &conn)
	{
		{
			wowpp::MySQL::Select select(conn, "SELECT `Id`, `SpellName1` FROM `dbc_spell`;");
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					// Get row data
					UInt32 entry = 0, index = 0;
					String name;
					row.getField(index++, entry);
					row.getField(index++, name);

					auto *spell = project.spells.getById(entry);
					if (!spell)
					{
						WLOG("Could not find spell " << entry);
						row = row.next(select);
						continue;
					}

					spell->set_name(name);
					row = row.next(select);
				}
			}
			else
			{
				ELOG("Error: " << conn.getErrorMessage());
			}
		}

		return true;
	}

	static bool importItemSockets(proto::Project &project, MySQL::Connection &conn)
	{
		wowpp::MySQL::Select select(conn, "SELECT `entry`, `socketColor_1`, `socketContent_1`, `socketColor_2`, `socketContent_2`, `socketColor_3`, `socketContent_3` FROM `tbcdb`.`item_template`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				UInt32 entry = 0, index = 0, color = 0, content = 0;

				row.getField(index++, entry);
				auto *item = project.items.getById(entry);
				if (!item)
				{
					WLOG("Could not find item " << entry << ": Skipping socket import");

					row = row.next(select);
					continue;
				}

				// First remove all sockets of this item
				item->clear_sockets();

				// Now insert the new ones
				for (UInt32 i = 0; i < 3; ++i)
				{
					row.getField(index++, color);
					row.getField(index++, content);
					if (color != 0 || content != 0)
					{
						auto *added = item->add_sockets();
						if (added)
						{
							added->set_color(color);
							added->set_content(content);
						}
					}
				}

				row = row.next(select);
			}
		}
		else
		{
			ELOG("Error: " << conn.getErrorMessage());
			return false;
		}

		return true;
	}

	static bool importResistancePercentages(proto::Project &project, MySQL::Connection &conn)
	{
		wowpp::MySQL::Select select(conn, "SELECT `percentages_0`, `percentages_25`, `percentages_50`, `percentages_75`, `percentages_100` FROM `dbc`.`resistance_values`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);

			// First cleanup
			project.resistancePcts.clear();
			for (int i = 0; i <= 10000; ++i)
			{	
				project.resistancePcts.add(i);
			}

			UInt32 id = 0;

			while (row)
			{
				std::array<UInt32, 5> pcts = { 0 };
				UInt32 index = 0;

				auto * values = project.resistancePcts.getById(id);
				if (values)
				{
					for (UInt8 i = 0; i < 5; ++i)
					{
						row.getField(index++, pcts[i]);
						values->add_percentages(pcts[i]);
					}
				}
				else
				{
					WLOG("Unable to resistancePcts by id: " << id);
				}

				id++;
				row = row.next(select);
			}
		}
		else
		{
			return false;
		}

		return true;
	}

	static bool importSkillLines(proto::Project &project, MySQL::Connection &conn)
	{
		for (auto &skill : *project.skills.getTemplates().mutable_entry())
		{
			skill.clear_spells();
		}

		wowpp::MySQL::Select select(conn, "SELECT `skill_line`, `spell`, `race_mask`, `class_mask`, `aquire_method`, `trivial_skill_line_high`, `trivial_skill_line_low` FROM `dbc`.`dbc_skilllineability`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				UInt32 skillId = 0, spellId = 0, raceMask = 0, classMask = 0, aquire = 0, high = 0, low = 0;
				row.getField(0, skillId);
				row.getField(1, spellId);
				row.getField(2, raceMask);
				row.getField(3, classMask);
				row.getField(4, aquire);
				row.getField(5, high);
				row.getField(6, low);

				do
				{
					// Find spell and skill
					auto *spell = project.spells.getById(spellId);
					if (!spell)
					{
						WLOG("Unable to find spell " << spellId);
						break;
					}
					auto *skill = project.skills.getById(skillId);
					if (!skill)
					{
						WLOG("Unable to find skill " << skillId);
						break;
					}

					spell->set_skill(skillId);
					spell->set_trivialskilllow(low);
					spell->set_trivialskillhigh(high);
					spell->set_racemask(raceMask);
					spell->set_classmask(classMask);

					// Add spell to the list of spells to learn when getting this skill
					if (aquire > 0)
					{
						skill->add_spells(spellId);
					}
				} while (false);

				row = row.next(select);
			}
		}

		return true;
	}

	static bool importSpellReagents(proto::Project &project, MySQL::Connection &conn)
	{
		for (auto &spell : *project.spells.getTemplates().mutable_entry())
		{
			spell.clear_reagents();
		}

		wowpp::MySQL::Select select(conn, "SELECT `id`, `Reagent1`, `ReagentCount1`, `Reagent2`, `ReagentCount2`, `Reagent3`, `ReagentCount3`"
			", `Reagent4`, `ReagentCount4`, `Reagent5`, `ReagentCount5`, `Reagent6`, `ReagentCount6`, `Reagent7`, `ReagentCount7`, `Reagent8`, `ReagentCount8` FROM `dbc_8606`.`dbc_spell`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				UInt32 spellId = 0;
				Int32 reagent = 0, count = 0;
				row.getField(0, spellId);

				do
				{
					// Find spell and skill
					auto *spell = project.spells.getById(spellId);
					if (!spell)
					{
						WLOG("Unable to find spell " << spellId);
						break;
					}

					Int32 fieldIndex = 1;
					for (UInt32 i = 0; i < 8; ++i)
					{
						row.getField(fieldIndex++, reagent);
						row.getField(fieldIndex++, count);
						if (reagent > 0 && count > 0)
						{
							auto *added = spell->add_reagents();
							if (!added)
								continue;

							added->set_item(reagent);
							if (count > 1)
							{
								added->set_count(count);
							}
						}
					}
				} while (false);

				row = row.next(select);
			}
		}

		return true;
	}

#if 0
	static void fixTriggerEvents(proto::Project &project)
	{
		for (auto &trigger : *project.triggers.getTemplates().mutable_entry())
		{
			for (auto &e : trigger.events())
			{
				auto *added = trigger.add_newevents();
				added->set_type(e);
				added->clear_data();
			}
		}
	}
#endif

	static bool importSpellChain(proto::Project &project, MySQL::Connection &conn)
	{
		for (auto &spell : *project.spells.getTemplates().mutable_entry())
		{
			spell.clear_prevspell();
			spell.clear_nextspell();
			spell.clear_rank();
		}

		wowpp::MySQL::Select select(conn, "SELECT `spell_id`, `prev_spell`, `first_spell`, `rank` FROM `tbcdb`.`spell_chain`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				UInt32 spellId = 0, prevSpellId = 0, baseSpellId = 0, rank = 0;
				row.getField(0, spellId);
				row.getField(1, prevSpellId);
				row.getField(2, baseSpellId);
				row.getField(3, rank);

				do
				{
					// Find spell
					auto *spell = project.spells.getById(spellId);
					if (!spell)
					{
						WLOG("Unable to find spell " << spellId);
						break;
					}

					auto *prevSpell = 
						prevSpellId != 0 ?
						project.spells.getById(prevSpellId) : nullptr;

					spell->set_rank(rank);
					spell->set_baseid(baseSpellId);
					spell->set_prevspell(prevSpellId);
					
					if (prevSpell)
					{
						prevSpell->set_nextspell(spellId);
					}
				} while (false);

				row = row.next(select);
			}
		}

		wowpp::MySQL::Select select2(conn, "SELECT `spell`, `superseded_by_spell` FROM `dbc`.`dbc_skilllineability`;");
		if (select2.success())
		{
			wowpp::MySQL::Row row(select2);
			while (row)
			{
				UInt32 spellId = 0, nextSpellId = 0;
				row.getField(0, spellId);
				row.getField(1, nextSpellId);

				do
				{
					// Find spell and skill
					auto *spell = project.spells.getById(spellId);
					if (!spell)
					{
						WLOG("Unable to find spell " << spellId);
						break;
					}
					auto *nextSpell = project.spells.getById(nextSpellId);

					// Enter next spell id
					spell->set_nextspell(nextSpell ? nextSpellId : 0);

					// Enter prev spell id of next spell (if any)
					if (nextSpell) nextSpell->set_prevspell(spellId);

					// Determine spell rank
					if (spell->baseid() == spell->id())
					{
						spell->set_rank(1);
					}
					else
					{
						const auto *prevSpell = project.spells.getById(spell->prevspell());
						if (prevSpell) spell->set_rank(prevSpell->rank() + 1);
					}
				} while (false);

				row = row.next(select2);
			}
		}

		return true;
	}

	static bool importKillCredits(proto::Project &project, MySQL::Connection &conn)
	{
		for (auto &unit : *project.units.getTemplates().mutable_entry())
		{
			unit.clear_killcredit();
		}

		wowpp::MySQL::Select select(conn, "SELECT `entry`, `KillCredit1` FROM `tbcdb`.`creature_template` WHERE KillCredit1 != 0;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				UInt32 entryId = 0, killCredit = 0;
				row.getField(0, entryId);
				row.getField(1, killCredit);

				do
				{
					// Find unit
					auto *unit = project.units.getById(entryId);
					if (!unit)
					{
						WLOG("Unable to find unit " << entryId);
						break;
					}

					unit->set_killcredit(killCredit);
				} while (false);

				row = row.next(select);
			}
		}

		return true;
	}

	static bool importLocales(proto::Project &project, MySQL::Connection &conn)
	{
		for (auto &unit : *project.units.getTemplates().mutable_entry())
		{
			unit.clear_name_loc();
			unit.clear_subname_loc();

			// Fill with default name / subname
			for (int i = 0; i < 12; ++i)
			{
				unit.add_name_loc();
				unit.add_subname_loc();
			}
		}

		for (auto &item : *project.items.getTemplates().mutable_entry())
		{
			item.clear_name_loc();
			item.clear_description_loc();

			// Fill with default name / subname
			for (int i = 0; i < 12; ++i)
			{
				item.add_name_loc();
				item.add_description_loc();
			}
		}

		for (auto &obj : *project.objects.getTemplates().mutable_entry())
		{
			obj.clear_name_loc();
			obj.clear_caption_loc();

			// Fill with default name / subname
			for (int i = 0; i < 12; ++i)
			{
				obj.add_name_loc();
				obj.add_caption_loc();
			}
		}

		for (auto &quest : *project.quests.getTemplates().mutable_entry())
		{
			quest.clear_name_loc();
			quest.clear_detailstext_loc();
			quest.clear_objectivestext_loc();
			quest.clear_offerrewardtext_loc();
			quest.clear_requestitemstext_loc();
			quest.clear_endtext_loc();

			// Fill with default texts
			for (int i = 0; i < 12; ++i)
			{
				quest.add_name_loc();
				quest.add_detailstext_loc();
				quest.add_objectivestext_loc();
				quest.add_offerrewardtext_loc();
				quest.add_requestitemstext_loc();
				quest.add_endtext_loc();
			}
		}

		if (!conn.execute("SET NAMES 'UTF8';"))
		{
			ELOG("Database error: " << conn.getErrorMessage());
			return false;
		}

		{
			wowpp::MySQL::Select select(conn, "SELECT `entry`, `name_loc3`, `subname_loc3` FROM `tbcdb`.`locales_creature` WHERE name_loc3 IS NOT NULL AND TRIM(name_loc3) <> '';");
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					UInt32 entryId = 0;
					String name_loc3, subname_loc3;
					row.getField(0, entryId);
					row.getField(1, name_loc3);
					row.getField(2, subname_loc3);

					do
					{
						// Find unit
						auto *unit = project.units.getById(entryId);
						if (!unit)
						{
							WLOG("Unable to find unit " << entryId);
							break;
						}

						unit->set_name_loc(1, name_loc3);
						if (!subname_loc3.empty()) unit->set_subname_loc(1, subname_loc3);
					} while (false);

					row = row.next(select);
				}
			}
			else
			{
				ELOG("Database error: " << conn.getErrorMessage());
				return false;
			}
		}

		{
			wowpp::MySQL::Select select(conn, "SELECT `entry`, `name_loc3`, `description_loc3` FROM `tbcdb`.`locales_item` WHERE name_loc3 IS NOT NULL AND TRIM(name_loc3) <> '';");
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					UInt32 entryId = 0;
					String name_loc3, description_loc3;
					row.getField(0, entryId);
					row.getField(1, name_loc3);
					row.getField(2, description_loc3);

					do
					{
						// Find item
						auto *item = project.items.getById(entryId);
						if (!item)
						{
							WLOG("Unable to find item " << entryId);
							break;
						}

						item->set_name_loc(1, name_loc3);
						if (!description_loc3.empty()) item->set_description_loc(1, description_loc3);
					} while (false);

					row = row.next(select);
				}
			}
			else
			{
				ELOG("Database error: " << conn.getErrorMessage());
				return false;
			}
		}

		{
			wowpp::MySQL::Select select(conn, "SELECT `entry`, `name_loc3`, `castbarcaption_loc3` FROM `tbcdb`.`locales_gameobject` WHERE name_loc3 IS NOT NULL AND TRIM(name_loc3) <> '';");
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					UInt32 entryId = 0;
					String name_loc3, caption_loc3;
					row.getField(0, entryId);
					row.getField(1, name_loc3);
					row.getField(2, caption_loc3);

					do
					{
						// Find object
						auto *obj = project.objects.getById(entryId);
						if (!obj)
						{
							WLOG("Unable to find object " << entryId);
							break;
						}

						obj->set_name_loc(1, name_loc3);
						if (!caption_loc3.empty()) obj->set_caption_loc(1, caption_loc3);
					} while (false);

					row = row.next(select);
				}
			}
			else
			{
				ELOG("Database error: " << conn.getErrorMessage());
				return false;
			}
		}

		{
			wowpp::MySQL::Select select(conn, "SELECT `entry`, `Title_loc3`, `Details_loc3`, `Objectives_loc3`, `OfferRewardText_loc3`, `RequestItemsText_loc3`, `EndText_loc3` FROM `tbcdb`.`locales_quest` WHERE Title_loc3 IS NOT NULL AND TRIM(Title_loc3) <> '';");
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					UInt32 entryId = 0;
					String name_loc3, details_loc3, objectives_loc3, offerreward_loc3, requestitems_loc3, end_loc3;
					row.getField(0, entryId);
					row.getField(1, name_loc3);
					row.getField(2, details_loc3);
					row.getField(3, objectives_loc3);
					row.getField(4, offerreward_loc3);
					row.getField(5, requestitems_loc3);
					row.getField(6, end_loc3);

					do
					{
						// Find quest
						auto *quest = project.quests.getById(entryId);
						if (!quest)
						{
							WLOG("Unable to find quest " << entryId);
							break;
						}

						if (!name_loc3.empty()) quest->set_name_loc(1, name_loc3);
						if (!details_loc3.empty()) quest->set_detailstext_loc(1, details_loc3);
						if (!objectives_loc3.empty()) quest->set_objectivestext_loc(1, objectives_loc3);
						if (!offerreward_loc3.empty()) quest->set_offerrewardtext_loc(1, offerreward_loc3);
						if (!requestitems_loc3.empty()) quest->set_requestitemstext_loc(1, requestitems_loc3);
						if (!end_loc3.empty()) quest->set_endtext_loc(1, end_loc3);
					} while (false);

					row = row.next(select);
				}
			}
			else
			{
				ELOG("Database error: " << conn.getErrorMessage());
				return false;
			}
		}

		return true;
	}

	static bool hasPositiveTarget(const proto::SpellEffect &effect)
	{
		if (effect.targetb() == game::targets::UnitAreaEnemySrc) {
			return false;
		}

		switch (effect.targeta())
		{
			case game::targets::UnitTargetEnemy:
			case game::targets::UnitAreaEnemyDst:
				//case game::targets::UnitTargetAny:
			case game::targets::UnitConeEnemy:
			case game::targets::DestDynObjEnemy:
				return false;
			default:
				return true;
		}
	}

	static bool isPositiveAura(proto::SpellEffect &effect)
	{
		switch (effect.aura())
		{
			//always positive
			case game::aura_type::PeriodicHeal:
			case game::aura_type::ModThreat:
			case game::aura_type::DamageShield:
			case game::aura_type::ModStealth:
			case game::aura_type::ModStealthDetect:
			case game::aura_type::ModInvisibility:
			case game::aura_type::ModInvisibilityDetection:
			case game::aura_type::ObsModHealth:
			case game::aura_type::ObsModMana:
			case game::aura_type::ReflectSpells:
			case game::aura_type::ProcTriggerDamage:
			case game::aura_type::TrackCreatures:
			case game::aura_type::TrackResources:
			case game::aura_type::ModBlockSkill:
			case game::aura_type::ModDamageDoneCreature:
			case game::aura_type::PeriodicHealthFunnel:
			case game::aura_type::FeignDeath:
			case game::aura_type::SchoolAbsorb:
			case game::aura_type::ExtraAttacks:
			case game::aura_type::ModSpellCritChanceSchool:
			case game::aura_type::ModPowerCostSchool:
			case game::aura_type::ReflectSpellsSchool:
			case game::aura_type::FarSight:
			case game::aura_type::Mounted:
			case game::aura_type::SplitDamagePct:
			case game::aura_type::WaterBreathing:
			case game::aura_type::ModBaseResistance:
			case game::aura_type::ModRegen:
			case game::aura_type::ModPowerRegen:
			case game::aura_type::InterruptRegen:
			case game::aura_type::SpellMagnet:
			case game::aura_type::ManaShield:
			case game::aura_type::ModSkillTalent:
			case game::aura_type::ModMeleeAttackPowerVersus:
			case game::aura_type::ModTotalThreat:
			case game::aura_type::WaterWalk:
			case game::aura_type::FeatherFall:
			case game::aura_type::Hover:
			case game::aura_type::AddTargetTrigger:
			case game::aura_type::AddCasterHitTrigger:
			case game::aura_type::ModRangedDamageTaken:
			case game::aura_type::ModRegenDurationCombat:
			case game::aura_type::Untrackable:
			case game::aura_type::ModOffhandDamagePct:
			case game::aura_type::ModMeleeDamageTaken:
			case game::aura_type::ModMeleeDamageTakenPct:
			case game::aura_type::ModPossessPet:
			case game::aura_type::ModMountedSpeedAlways:
			case game::aura_type::ModRangedAttackPowerVersus:
			case game::aura_type::ModManaRegenInterrupt:
			case game::aura_type::ModRangedAmmoHaste:
			case game::aura_type::RetainComboPoints:
			case game::aura_type::SpiritOfRedemption:
			case game::aura_type::ModResistanceOfStatPercent:
			case game::aura_type::AuraType_222:
			case game::aura_type::PrayerOfMending:
			case game::aura_type::DetectStealth:
			case game::aura_type::ModAOEDamageAvoidance:
			case game::aura_type::ModIncreaseHealth_3:
			case game::aura_type::AuraType_233:
			case game::aura_type::ModSpellDamageOfAttackPower:
			case game::aura_type::ModSpellHealingOfAttackPower:
			case game::aura_type::ModScale_2:
			case game::aura_type::ModExpertise:
			case game::aura_type::ModSpellDamageFromHealing:
			case game::aura_type::ComprehendLanguage:
			case game::aura_type::ModIncreaseHealth_2:
			case game::aura_type::AuraType_261:
				return true;
			//always negative
			case game::aura_type::BindSight:
			case game::aura_type::ModPossess:
			case game::aura_type::ModConfuse:
			case game::aura_type::ModCharm:
			case game::aura_type::ModFear:
			case game::aura_type::ModTaunt:
			case game::aura_type::ModStun:
			case game::aura_type::ModRoot:
			case game::aura_type::ModSilence:
			case game::aura_type::PeriodicLeech:
			case game::aura_type::ModPacifySilence:
			case game::aura_type::PeriodicManaLeech:
			case game::aura_type::ModDisarm:
			case game::aura_type::ModStalked:
			case game::aura_type::ChannelDeathItem:
			case game::aura_type::ModDetectRange:
			case game::aura_type::Ghost:
			case game::aura_type::AurasVisible:
			case game::aura_type::Empathy:
			case game::aura_type::ModTargetResistance:
			case game::aura_type::RangedAttackPowerAttackerBonus:
			case game::aura_type::PowerBurnMana:
			case game::aura_type::ModCritDamageBonusMelee:
			case game::aura_type::AOECharm:
			case game::aura_type::UseNormalMovementSpeed:
			case game::aura_type::ArenaPreparation:
			case game::aura_type::ModDetaunt:
			case game::aura_type::AuraType_223:
			case game::aura_type::ModForceMoveForward:
			case game::aura_type::AuraType_243:
				return false;
			//depends on basepoints (more is better)
			case game::aura_type::ModRangedHaste:
			case game::aura_type::ModBaseResistancePct:
			case game::aura_type::ModResistanceExclusive:
			case game::aura_type::SafeFall:
			case game::aura_type::ResistPushback:
			case game::aura_type::ModShieldBlockValuePct:
			case game::aura_type::ModDetectedRange:
			case game::aura_type::SplitDamageFlat:
			case game::aura_type::ModStealthLevel:
			case game::aura_type::ModWaterBreathing:
			case game::aura_type::ModReputationGain:
			case game::aura_type::PetDamageMulti:
			case game::aura_type::ModShieldBlockValue:
			case game::aura_type::ModAOEAvoidance:
			case game::aura_type::ModHealthRegenInCombat:
			case game::aura_type::ModAttackPowerPct:
			case game::aura_type::ModRangedAttackPowerPct:
			case game::aura_type::ModDamageDoneVersus:
			case game::aura_type::ModCritPercentVersus:
			case game::aura_type::ModSpeedNotStack:
			case game::aura_type::ModMountedSpeedNotStack:
			case game::aura_type::ModSpellDamageOfStatPercent:
			case game::aura_type::ModSpellHealingOfStatPercent:
			case game::aura_type::ModDebuffResistance:
			case game::aura_type::ModAttackerSpellCritChance:
			case game::aura_type::ModFlatSpellDamageVersus:
			case game::aura_type::ModRating:
			case game::aura_type::ModFactionReputationGain:
			case game::aura_type::HasteMelee:
			case game::aura_type::MeleeSlow:
			case game::aura_type::ModIncreaseSpellPctToHit:
			case game::aura_type::ModXpPct:
			case game::aura_type::ModFlightSpeed:
			case game::aura_type::ModFlightSpeedMounted:
			case game::aura_type::ModFlightSpeedStacking:
			case game::aura_type::ModFlightSpeedMountedStacking:
			case game::aura_type::ModFlightSpeedMountedNotStacking:
			case game::aura_type::ModRangedAttackPowerOfStatPercent:
			case game::aura_type::ModRageFromDamageDealt:
			case game::aura_type::AuraType_214:
			case game::aura_type::HasteSpells:
			case game::aura_type::HasteRanged:
			case game::aura_type::ModManaRegenFromStat:
			case game::aura_type::ModDispelResist:
				return (effect.basepoints() >= 0);
			//depends on basepoints (less is better)
			case game::aura_type::ModCriticalThreat:
			case game::aura_type::ModAttackerMeleeHitChance:
			case game::aura_type::ModAttackerRangedHitChance:
			case game::aura_type::ModAttackerSpellHitChance:
			case game::aura_type::ModAttackerMeleeCritChance:
			case game::aura_type::ModAttackerRangedCritChance:
			case game::aura_type::ModCooldown:
			case game::aura_type::ModAttackerSpellAndWeaponCritChance:
			case game::aura_type::ModAttackerMeleeCritDamage:
			case game::aura_type::ModAttackerRangedCritDamage:
			case game::aura_type::ModAttackerSpellCritDamage:
			case game::aura_type::MechanicDurationMod:
			case game::aura_type::MechanicDurationModNotStack:
			case game::aura_type::ModDurationOfMagicEffects:
			case game::aura_type::ModCombatResultChance:
			case game::aura_type::ModEnemyDodge:
				return (effect.basepoints() <= 0);
				//depends
			case game::aura_type::PeriodicDamage:
			case game::aura_type::Dummy:
			case game::aura_type::ModAttackSpeed:
			case game::aura_type::ModDamageTaken:
			case game::aura_type::ModResistance:
			case game::aura_type::PeriodicTriggerSpell:
			case game::aura_type::PeriodicEnergize:
			case game::aura_type::ModPacify:
			case game::aura_type::ModStat:
			case game::aura_type::ModSkill:
			case game::aura_type::ModIncreaseSpeed:
			case game::aura_type::ModIncreaseMountedSpeed:
			case game::aura_type::ModDecreaseSpeed:
			case game::aura_type::ModIncreaseHealth:
			case game::aura_type::ModIncreaseEnergy:
			case game::aura_type::ModShapeShift:
			case game::aura_type::EffectImmunity:
			case game::aura_type::StateImmunity:
			case game::aura_type::SchoolImmunity:
			case game::aura_type::DamageImmunity:
			case game::aura_type::DispelImmunity:
			case game::aura_type::ProcTriggerSpell:
			case game::aura_type::ModParryPercent:
			case game::aura_type::ModDodgePercent:
			case game::aura_type::ModBlockPercent:
			case game::aura_type::ModCritPercent:
			case game::aura_type::ModHitChance:
			case game::aura_type::ModSpellHitChance:
			case game::aura_type::Transform:
			case game::aura_type::ModSpellCritChance:
			case game::aura_type::ModIncreaseSwimSpeed:
			case game::aura_type::ModScale:
			case game::aura_type::ModCastingSpeed:
			case game::aura_type::ModPowerCostSchoolPct:
			case game::aura_type::ModLanguage:
			case game::aura_type::MechanicImmunity:
			case game::aura_type::ModDamagePercentDone:
			case game::aura_type::ModPercentStat:
			case game::aura_type::ModDamagePercentTaken:
			case game::aura_type::ModHealthRegenPercent:
			case game::aura_type::PeriodicDamagePercent:
			case game::aura_type::PreventsFleeing:
			case game::aura_type::ModUnattackable:
			case game::aura_type::ModAttackPower:
			case game::aura_type::ModResistancePct:
			case game::aura_type::AddFlatModifier:
			case game::aura_type::AddPctModifier:
			case game::aura_type::ModPowerRegenPercent:
			case game::aura_type::OverrideClassScripts:
			case game::aura_type::ModHealing:
			case game::aura_type::ModMechanicResistance:
			case game::aura_type::ModHealingPct:
			case game::aura_type::ModRangedAttackPower:
			case game::aura_type::ModSpeedAlways:
			case game::aura_type::ModIncreaseEnergyPercent:
			case game::aura_type::ModIncreaseHealthPercent:
			case game::aura_type::ModHealingDone:
			case game::aura_type::ModHealingDonePct:
			case game::aura_type::ModTotalStatPercentage:
			case game::aura_type::ModHaste:
			case game::aura_type::ForceReaction:
			case game::aura_type::MechanicImmunityMask:
			case game::aura_type::TrackStealthed:
			case game::aura_type::NoPvPCredit:
			case game::aura_type::MeleeAttackPowerAttackerBonus:
			case game::aura_type::DetectAmore:
			case game::aura_type::Fly:
			case game::aura_type::PeriodicDummy:
			case game::aura_type::PeriodicTriggerSpellWithValue:
			case game::aura_type::ProcTriggerSpellWithValue:
			case game::aura_type::AuraType_247:
			default:
				return hasPositiveTarget(effect);
		}

		return true;
	}

	static bool isPositiveSpell(proto::SpellEntry &spell)
	{
		// Passive spells are always considered positive
		if (spell.attributes(0) & game::spell_attributes::Passive)
			return true;

		// Negative attribute
		if (spell.attributes(0) & game::spell_attributes::Negative)
			return false;

		for (auto &effect : *spell.mutable_effects())
		{
			if (isPositiveAura(effect))
				return true;
		}

		return false;
	}

	static bool updateSpells(proto::Project &project)
	{
		for (auto &spell : *project.spells.getTemplates().mutable_entry())
		{
			// Determine if spell would be positive
			spell.set_positive(isPositiveSpell(spell));
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

	simple::scoped_connection genericLogConnection;
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
	
	if (!importLocales(protoProject, connection))
	{
		WLOG("Couldn't import locales!");
		return 1;
	}

	// Save project
	if (!protoProject.save(configuration.dataPath))
	{
		return 1;
	}

	// Shutdown protobuf and free all memory (optional)
	google::protobuf::ShutdownProtobufLibrary();

	return 0;
}
