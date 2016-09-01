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

						// Enrage
					case 5229:
						it->mutable_effects(1)->set_basedice(0);
						it->mutable_effects(1)->set_diesides(0);
						it->mutable_effects(1)->set_targeta(1);
						it->mutable_effects(1)->set_aura(142);
						it->mutable_effects(1)->set_miscvaluea(1);
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

	static bool checkSpellAttributes(proto::Project &project)
	{
		auto *spellEntries = project.spells.getTemplates().mutable_entry();
		if (spellEntries)
		{
			auto it = spellEntries->begin();
			while (it != spellEntries->end())
			{
				if (it->attributes(5) & game::spell_attributes_ex_e::Unknown_17)
				{
					DLOG("id is " << it->id());
				}
				it++;
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

	if (!addSpellLinks(protoProject))
	{
		ELOG("Failed to add spell links");
		return 1;
	}

#if 0
	if (!importSpellFocus(protoProject, connection))
	{
		ELOG("Failed to load spell focus object");
		return 1;
	}

	if (!importTrainerLinks(protoProject, connection))
	{
		ELOG("Failed to import trainer links");
		return 1;
	}

	if (!importSpellMultipliers(protoProject, connection))
	{
		ELOG("Failed to import spell damage multipliers");
		return 1;
	}

	if (!importSpellFamilyFlags(protoProject, connection))
	{
		ELOG("Failed to import spell family flags multipliers");
		return 1;
	}

	if (!importSpellFocus(protoProject, connection))
	{
		ELOG("Failed to import spell focus targets");
		return 1;
	}

	if (!importSpellProcs(protoProject, connection))
	{
		ELOG("Failed to import spell proc events");
		return 1;
	}

	if (!addSpellBaseId(protoProject))
	{
		WLOG("Could not set base id for spells");
		return 1;
	}

	if (!importItemSockets(protoProject, connection))
	{
		WLOG("Could not import item sockets");
		return 1;
	}

	if (!checkSpellAttributes(protoProject))
	{
		WLOG("Could not check spell attributes");
		return 1;
	}

#endif
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
