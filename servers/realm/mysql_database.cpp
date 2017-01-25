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
#include "mysql_database.h"
#include "mysql_wrapper/mysql_row.h"
#include "mysql_wrapper/mysql_select.h"
#include "mysql_wrapper/mysql_statement.h"
#include "common/constants.h"
#include "game_protocol/game_protocol.h"
#include "player_social.h"
#include "player.h"
#include "player_manager.h"
#include "log/default_log_levels.h"
#include "proto_data/project.h"

namespace wowpp
{
	MySQLDatabase::MySQLDatabase(proto::Project &project, const MySQL::DatabaseInfo &connectionInfo)
		: m_project(project)
		, m_connectionInfo(connectionInfo)
	{
	}

	bool MySQLDatabase::load()
	{
		if (!m_connection.connect(m_connectionInfo))
		{
			ELOG("Could not connect to the realm database");
			ELOG(m_connection.getErrorMessage());
			return false;
		}

		ILOG("Connected to MySQL at " <<
			m_connectionInfo.host << ":" <<
			m_connectionInfo.port);

		return true;
	}

	game::ResponseCode MySQLDatabase::renameCharacter(DatabaseId id, const String & newName)
	{
		const String safeName = m_connection.escapeString(newName);

		// Check if name is already in use
		wowpp::MySQL::Select select(m_connection,
			fmt::format("SELECT `id` FROM `character` WHERE `name`='{0}' LIMIT 1", safeName));
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			if (row)
			{
				return game::response_code::CharCreateNameInUse;
			}
		}
		else
		{
			printDatabaseError();
			return game::response_code::CharNameFailure;
		}

		const UInt32 lowerGuid = guidLowerPart(id);

		if (m_connection.execute(fmt::format(
			"UPDATE `character` SET `name`='{0}', `at_login`=`at_login` & ~{1} WHERE `id`={2}"
			, safeName
			, game::atlogin_flags::Rename
			, lowerGuid)))
		{
			return game::response_code::Success;
		}
		else
		{
			printDatabaseError();
		}

		return game::response_code::CharNameFailure;
	}

	wowpp::UInt32 MySQLDatabase::getCharacterCount(UInt32 accountId)
	{
		UInt32 retCount = 0;

		wowpp::MySQL::Select select(m_connection,
			fmt::format("SELECT COUNT(id) FROM `character` WHERE `account`={0}"
			, accountId));

		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			if (row)
			{
				row.getField(0, retCount);
			}
			else
			{
				// No row found: Account does not exist
				return retCount;
			}
		}
		else
		{
			// There was an error
			printDatabaseError();
			return retCount;
		}

		return retCount;
	}

	game::ResponseCode MySQLDatabase::createCharacter(
		UInt32 accountId, 
		const std::vector<const proto::SpellEntry*> &spells, 
		const std::vector<ItemData> &items, 
		game::CharEntry &character)
	{
		// See if the character name is already in use
		const String safeName = m_connection.escapeString(character.name);

		// Check if character name is already in use
		{
			wowpp::MySQL::Select select(m_connection,
				fmt::format("SELECT `id` FROM `character` WHERE `name`='{0}' LIMIT 1"
				, safeName));
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				if (row)
				{
					// There was an error
					return game::response_code::CharCreateNameInUse;
				}
			}
			else
			{
				// There was an error
				printDatabaseError();
				return game::response_code::CharCreateError;
			}
		}

		// Check if faction is allowed
		{
			// Get selected faction
			const bool isAlliance = ((game::race::Alliance & (1 << (character.race - 1))) == (1 << (character.race - 1)));

			// Get all characters and check their race
			wowpp::MySQL::Select select(m_connection,
				fmt::format("SELECT `race` FROM `character` WHERE `account`={0}"
				, accountId));
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					// Check race
					UInt32 tmpRace;
					row.getField(0, tmpRace);
					
					if (isAlliance)
					{
						// Horde character not allowed
						if ((game::race::Horde & (1 << (tmpRace - 1))) == (1 << (tmpRace - 1)))
						{
							return game::response_code::CharCreatePvPTeamsViolation;
						}
					}
					else
					{
						// Alliance character not allowed
						if ((game::race::Alliance & (1 << (tmpRace - 1))) == (1 << (tmpRace - 1)))
						{
							return game::response_code::CharCreatePvPTeamsViolation;
						}
					}

					row = row.next(select);
				}
			}
			else
			{
				// There was an error
				printDatabaseError();
				return game::response_code::CharCreateError;
			}
		}

		// Character name is not in use: try to create the character
		UInt32 bytes = 0, bytes2 = 0;
		bytes &= ~static_cast<UInt32>(static_cast<UInt32>(0xff) << (0 * 8));
		bytes |= static_cast<UInt32>(static_cast<UInt32>(character.skin) << (0 * 8));
		bytes &= ~static_cast<UInt32>(static_cast<UInt32>(0xff) << (1 * 8));
		bytes |= static_cast<UInt32>(static_cast<UInt32>(character.face) << (1 * 8));
		bytes &= ~static_cast<UInt32>(static_cast<UInt32>(0xff) << (2 * 8));
		bytes |= static_cast<UInt32>(static_cast<UInt32>(character.hairStyle) << (2 * 8));
		bytes &= ~static_cast<UInt32>(static_cast<UInt32>(0xff) << (3 * 8));
		bytes |= static_cast<UInt32>(static_cast<UInt32>(character.hairColor) << (3 * 8));
		bytes2 &= ~static_cast<UInt32>(static_cast<UInt32>(0xff) << (0 * 8));
		bytes2 |= static_cast<UInt32>(static_cast<UInt32>(character.facialHair) << (0 * 8));

		if (m_connection.execute(fmt::format(
			//                        0         1      2      3       4        5       6        7     8      9            10           11           12			  13		  
			"INSERT INTO `character` (`account`,`name`,`race`,`class`,`gender`,`bytes`,`bytes2`,`map`,`zone`,`position_x`,`position_y`,`position_z`,`orientation`,`cinematic`,"
			//						  14		 15		  16	   17		18		  19	  20		  21		22		  23		24		  25		26
									 "`home_map`,`home_x`,`home_y`,`home_z`,`home_o`,`level`, `at_login`, `health`, `power1`, `power2`, `power3`, `power4`, `power5`) "
			"VALUES ({0}, '{1}', {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}, {10}, {11}, {12}, {13}, {14}, {15}, {16}, {17}, {18}, {19}, {20}, {21}, {22}, {23}, {24}, {25}, {26})"
			, accountId										// 0
			, safeName										// 1
			, static_cast<UInt32>(character.race)			// 2
			, static_cast<UInt32>(character.class_)			// 3
			, static_cast<UInt32>(character.gender)			// 4
			, bytes											// 5
			, bytes2										// 6
			, character.mapId	// Location					// 7
			, character.zoneId								// 8
			, character.location.x							// 9
			, character.location.y							// 10
			, character.location.z							// 11
			, character.o									// 12 
			, static_cast<UInt32>(character.cinematic)		// 13
			, character.mapId	// Home point				// 14
			, character.location.x							// 15
			, character.location.y							// 16
			, character.location.z							// 17
			, character.o									// 18
			, static_cast<UInt32>(character.level)			// 19
			, static_cast<UInt32>(character.atLogin)		// 20
			, 1000											// 21
			, 1000											// 22
			, 0												// 23
			, 0												// 24
			, 100											// 25
			, 0												// 26
			)))
		{
			// Retrieve id of the newly created character
			wowpp::MySQL::Select select(m_connection,
				fmt::format("SELECT `id` FROM `character` WHERE `name`='{0}' LIMIT 1"
				, safeName));
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				if (row)
				{
					row.getField(0, character.id);

					if (!spells.empty())
					{
						std::ostringstream fmtStrm;
						fmtStrm << "INSERT IGNORE INTO `character_spells` (`guid`,`spell`) VALUES ";

						// Add spells
						bool isFirstEntry = true;
						for (const auto *spell : spells)
						{
							if (isFirstEntry)
							{
								isFirstEntry = false;
							}
							else
							{
								fmtStrm << ",";
							}

							fmtStrm << "(" << character.id << "," << spell->id() << ")";
						}

						// Now, learn all initial spells
						if (!m_connection.execute(fmtStrm.str()))
						{
							// Could not learn initial spells
							printDatabaseError();
							return game::response_code::CharCreateError;
						}
					}
					
					// TODO: Initialize action bars
					if (!items.empty())
					{
						std::ostringstream fmtStrm;
						fmtStrm << "INSERT INTO `character_items` (`owner`, `entry`, `slot`, `count`, `durability`) VALUES ";

						// Add items
						bool isFirstEntry = true;
						for (const auto &item : items)
						{
							if (item.stackCount == 0)
								continue;;

							if (isFirstEntry)
							{
								isFirstEntry = false;
							}
							else
							{
								fmtStrm << ",";
							}

							fmtStrm << "(" << character.id << "," << item.entry << "," << item.slot << "," << UInt16(item.stackCount) << "," << item.durability << ")";
						}

						// Now, learn all initial spells
						if (!m_connection.execute(fmtStrm.str()))
						{
							// Could not learn initial spells
							printDatabaseError();
							return game::response_code::CharCreateError;
						}
					}

					return game::response_code::CharCreateSuccess;
				}
				else
				{
					// Character does not exist - something went wrong?
					return game::response_code::CharCreateError;
				}
			}
			else
			{
				// Error occurred
				printDatabaseError();
				return game::response_code::CharCreateError;
			}
		}
		else
		{
			// Error occurred
			printDatabaseError();
			return game::response_code::CharCreateError;
		}
	}

	bool MySQLDatabase::getCharacters(UInt32 accountId, game::CharEntries &out_characters)
	{
		wowpp::MySQL::Select select(m_connection,
							//      0     1       2       3        4        5       6        7       8    
			fmt::format("SELECT `id`, `name`, `race`, `class`, `gender`,`bytes`,`bytes2`,`level`,`map`,"
							//		 9       10            11            12           13		  14		15		  16
								 "`zone`,`position_x`,`position_y`,`position_z`,`orientation`,`cinematic`, `at_login`,`flags` FROM `character` WHERE `account`={0} ORDER BY `id`"
			, accountId));
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				UInt32 bytes = 0, bytes2 = 0;

				game::CharEntry entry;

				// Basic stuff
				row.getField(0, entry.id);
				row.getField(1, entry.name);

				// Display
				UInt32 tmp = 0;
				row.getField(2, tmp);
				entry.race = static_cast<game::Race>(tmp);
				row.getField(3, tmp);
				entry.class_ = static_cast<game::CharClass>(tmp);
				row.getField(4, tmp);
				entry.gender = static_cast<game::Gender>(tmp);
				row.getField(5, bytes);
				row.getField(6, bytes2);
				row.getField(7, tmp);
				entry.level = static_cast<UInt8>(tmp);

				// Placement
				row.getField(8, entry.mapId);
				row.getField(9, entry.zoneId);
				row.getField(10, entry.location.x);
				row.getField(11, entry.location.y);
				row.getField(12, entry.location.z);
				row.getField(13, entry.o);
				
				Int32 cinematic = 0;
				row.getField(14, cinematic);
				entry.cinematic = (cinematic != 0);

				row.getField(15, tmp);
				entry.atLogin = static_cast<game::AtLoginFlags>(tmp);

				// Reinterpret bytes
				entry.skin = static_cast<UInt8>(bytes);
				entry.face = static_cast<UInt8>(bytes >> 8);
				entry.hairStyle = static_cast<UInt8>(bytes >> 16);
				entry.hairColor = static_cast<UInt8>(bytes >> 24);
				entry.facialHair = static_cast<UInt8>(bytes2 & 0xff);

				row.getField(16, tmp);
				entry.flags = static_cast<game::CharacterFlags>(tmp);

				// Add to vector
				out_characters.push_back(entry);

				// Next row
				row = row.next(select);
			}
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}

		for (auto &entry : out_characters)
		{
			std::ostringstream strm;
			strm << "SELECT `entry`, `slot` FROM `character_items` WHERE (`slot` BETWEEN 65280 AND 65299) AND (`owner` = " << entry.id << ") LIMIT 19;";

			wowpp::MySQL::Select select(m_connection, strm.str());
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					UInt8 slot = 0;
					row.getField<UInt8, UInt16>(1, slot);

					UInt32 itemEntry = 0;
					row.getField(0, itemEntry);

					const auto *item = m_project.items.getById(itemEntry);
					if (item)
					{
						entry.equipment[slot & 0xFF] = item;
					}
					
					row = row.next(select);
				}
			}
		}

		return true;
	}

	game::ResponseCode MySQLDatabase::deleteCharacter(UInt32 accountId, UInt64 characterGuid)
	{
		const UInt32 lowerPart = guidLowerPart(characterGuid);

		// Start transaction
		MySQL::Transaction transation(m_connection);
		{
			if (!m_connection.execute(fmt::format(
				"DELETE FROM `character` WHERE `id`={0} AND `account`={1}"
				, lowerPart
				, accountId)))
			{
				// There was an error
				printDatabaseError();
				return game::response_code::CharDeleteFailed;
			}

			if (!m_connection.execute(fmt::format(
				"DELETE FROM `character_social` WHERE `guid_1`={0} OR `guid_2`={0}"
				, characterGuid)))
			{
				// There was an error
				printDatabaseError();
				return game::response_code::CharDeleteFailed;
			}
		}
		transation.commit();

		return game::response_code::CharDeleteSuccess;
	}

	bool MySQLDatabase::getGameCharacter(DatabaseId characterId, GameCharacter &out_character)
	{
		wowpp::MySQL::Select select(m_connection, fmt::format(
			//       0       1       2        3        4       5        6       7     8       9     
			"SELECT `name`, `race`, `class`, `gender`,`bytes`,`bytes2`,`level`,`xp`, `gold`, `map`,"
			//       10     11           12           13           14
				   "`zone`,`position_x`,`position_y`,`position_z`,`orientation`,"
            //       15        16       17       18       19	   20					21			  22		 23				
				   "`home_map`,`home_x`,`home_y`,`home_z`,`home_o`,`explored_zones`, `last_save`, `last_group`, `actionbars`,"
			//		 24	      25             26			  27       28       29        30       31      32
				   "`flags`, `played_time`, `level_time`, `health`,`power1`,`power2`,`power3`,`power4`,`power5` "
			"FROM `character` WHERE `id`={0} LIMIT 1"
			, characterId));
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			if (row)
			{
				// Character name
				out_character.setName(row.getField(0));

				// Race
				UInt32 raceId;
				row.getField(1, raceId);
				
				// Class
				UInt32 classId;
				row.getField(2, classId);
				
				// Gender
				UInt32 genderId;
				row.getField(3, genderId);
				
				// Update values
				out_character.setByteValue(unit_fields::Bytes2, 1, 0x08 | 0x20);	//UNK3 | UNK5
				out_character.setRace(raceId);
				out_character.setClass(classId);
				out_character.setGender(static_cast<game::Gender>(genderId & 0x01));

				// Level
				UInt32 level;
				row.getField(6, level);
				out_character.setLevel(level);

				// XP
				UInt32 xp;
				row.getField(7, xp);
				out_character.setUInt32Value(character_fields::Xp, xp);

				// Gold
				UInt32 gold;
				row.getField(8, gold);
				out_character.setUInt32Value(character_fields::Coinage, gold);

				//TODO: Explored zones
				out_character.setUInt32Value(1123, 0x00000002);
				out_character.setUInt32Value(1125, 0x3f4ccccd);
				out_character.setUInt32Value(1126, 0x3f4ccccd);
				out_character.setUInt32Value(1130, 0x3f4ccccd);

				// Bytes
				UInt32 bytes, bytes2;
				row.getField(4, bytes);
				row.getField(5, bytes2);
				out_character.setUInt32Value(character_fields::CharacterBytes, bytes);
				out_character.setUInt32Value(character_fields::CharacterBytes2, bytes2);

				out_character.setByteValue(character_fields::CharacterBytes2, 3, 2);

				//TODO Drunk state
				out_character.setByteValue(character_fields::CharacterBytes3, 0, genderId & 0x01);
				out_character.setByteValue(character_fields::CharacterBytes3, 3, 0x00);

				// Character flags
				UInt32 flags = 0;
				row.getField(24, flags);
				out_character.setUInt32Value(character_fields::CharacterFlags, flags);

				//TODO: Init primary professions
				out_character.setUInt32Value(character_fields::CharacterPoints_2, 10);		// Why 10?

				// Location
				UInt32 mapId;
				float x, y, z, o;
				row.getField(9, mapId);
				row.getField(11, x);
				row.getField(12, y);
				row.getField(13, z);
				row.getField(14, o);
				out_character.relocate(math::Vector3(x, y, z), o);
				out_character.setMapId(mapId);

				// Home point
				row.getField(15, mapId);
				row.getField(16, x);
				row.getField(17, y);
				row.getField(18, z);
				row.getField(19, o);
				out_character.setHome(mapId, math::Vector3(x, y, z), o);

				// Load action bar states
				UInt32 actionBars = 0;
				row.getField(23, actionBars);
				out_character.setByteValue(character_fields::FieldBytes, 2, actionBars);

				String zoneBuffer;
				row.getField(20, zoneBuffer);
				if (!zoneBuffer.empty())
				{
					std::stringstream strm(zoneBuffer);
					for (size_t i = 0; i < 64; ++i)
					{
						UInt32 zone = 0;
						strm >> zone;
						out_character.setUInt32Value(character_fields::ExploredZones_1 + i, zone);
					}
				}

				size_t lastSave = 0;
				row.getField(21, lastSave);
				if (lastSave == 0)
				{
					lastSave = time(nullptr);
				}

				size_t now = time(nullptr);
				const bool removeConjuredItems = (now >= lastSave) && ((now - lastSave) > 60 * 15);

				// Load character group
				UInt64 lastGroup = 0;
				row.getField(22, lastGroup);
				out_character.setGroupId(lastGroup);

				// Load play time values
				UInt32 totalTime = 0, levelTime = 0;
				row.getField(25, totalTime);
				row.getField(26, levelTime);
				out_character.setPlayTime(player_time_index::TotalPlayTime, totalTime);
				out_character.setPlayTime(player_time_index::LevelPlayTime, levelTime);

				UInt32 value = 0;
				row.getField(27, value);
				out_character.setUInt32Value(unit_fields::Health, value);
				for (int powerIndex = 0; powerIndex < 5; ++powerIndex)
				{
					row.getField(28 + powerIndex, value);
					out_character.setUInt32Value(unit_fields::Power1 + powerIndex, value);
				}

				// Load character spells
				wowpp::MySQL::Select spellSelect(m_connection, fmt::format(
					//       0     
					"SELECT `spell` FROM `character_spells` WHERE `guid`={0}"
					, characterId));
				if (spellSelect.success())
				{
					wowpp::MySQL::Row spellRow(spellSelect);
					while (spellRow)
					{
						UInt32 spellId = 0;
						spellRow.getField(0, spellId);

						// Try to find that spell
						const auto *spell = m_project.spells.getById(spellId);
						if (!spell)
						{
							// Could not find spell
							WLOG("Unknown spell found: " << spellId << " - spell will be ignored!");
						}
						else
						{
							// Our character knows that spell now
							out_character.addSpell(*spell);
						}

						// Next row
						spellRow = spellRow.next(spellSelect);
					}
				}

				// Load items
				wowpp::MySQL::Select itemSelect(m_connection, fmt::format(
					//         0		1		2			3		4
					"SELECT `entry`, `slot`, `creator`, `count`, `durability` FROM `character_items` WHERE `owner`={0}"
					, characterId));
				if (itemSelect.success())
				{
					wowpp::MySQL::Row itemRow(itemSelect);
					while (itemRow)
					{
						// Read item data
						ItemData data;
						itemRow.getField(0, data.entry);
						itemRow.getField(1, data.slot);
						itemRow.getField(2, data.creator);
						itemRow.getField<UInt8, UInt16>(3, data.stackCount);
						itemRow.getField(4, data.durability);
						const auto *itemEntry = m_project.items.getById(data.entry);
						if (itemEntry)
						{
							// More than 15 minutes passed since last save?
							const bool isConjured = (itemEntry->flags() & 0x02) != 0;
							if (!isConjured || !removeConjuredItems)
							{
								out_character.getInventory().addRealmData(data);
							}
						}
						else
						{
							WLOG("Unknown item in character database: " << data.entry);
						}

						// Next row
						itemRow = itemRow.next(itemSelect);
					}
				}

				// Load quest data
				wowpp::MySQL::Select questSelect(m_connection, fmt::format(
					//         0		1			2		
					"SELECT `quest`, `status`, `explored`, "
					//		3			4			5				6
					"`unitcount1`, `unitcount2`, `unitcount3`, `unitcount4`, "
					//		7				8				9			10
					"`objectcount1`, `objectcount2`, `objectcount3`, `objectcount4`, "
					//		11			12			13				14		  15
					"`itemcount1`, `itemcount2`, `itemcount3`, `itemcount4`, `timer` "
					"FROM `character_quests` WHERE `guid`={0}"
					, characterId));
				if (questSelect.success())
				{
					wowpp::MySQL::Row questRow(questSelect);
					while (questRow)
					{
						UInt32 questId = 0, index = 0;
						QuestStatusData data;
						questRow.getField(index++, questId);
						
						UInt32 status = 0;
						questRow.getField(index++, status);
						data.status = static_cast<game::QuestStatus>(status);

						questRow.getField(index++, data.explored);
						questRow.getField(index++, data.creatures[0]);
						questRow.getField(index++, data.creatures[1]);
						questRow.getField(index++, data.creatures[2]);
						questRow.getField(index++, data.creatures[3]);
						questRow.getField(index++, data.objects[0]);
						questRow.getField(index++, data.objects[1]);
						questRow.getField(index++, data.objects[2]);
						questRow.getField(index++, data.objects[3]);
						questRow.getField(index++, data.items[0]);
						questRow.getField(index++, data.items[1]);
						questRow.getField(index++, data.items[2]);
						questRow.getField(index++, data.items[3]);
						questRow.getField(index++, data.expiration);
						
						// Let the quest fail if it timed out
						if ((data.status == game::quest_status::Incomplete || data.status == game::quest_status::Complete) &&
							data.expiration > 0 &&
							data.expiration <= GameTime(time(nullptr)))
						{
							data.status = game::quest_status::Failed;
						}
						else if (data.status == game::quest_status::Failed)
						{
							data.expiration = 1;
						}

						out_character.setQuestData(questId, data);

						// Next row
						questRow = questRow.next(questSelect);
					}
				}

				// Load skills
				wowpp::MySQL::Select skillSelect(m_connection, fmt::format(
					//       0     
					"SELECT `skill`,`current`,`max` FROM `character_skills` WHERE `guid`={0}"
					, characterId));
				if (skillSelect.success())
				{
					wowpp::MySQL::Row skillRow(skillSelect);
					while (skillRow)
					{
						UInt32 skillId = 0;
						skillRow.getField(0, skillId);
						UInt16 current = 1, max = 1;
						skillRow.getField(1, current);
						skillRow.getField(2, max);

						// Try to find that spell
						const auto *skill = m_project.skills.getById(skillId);
						if (!skill)
						{
							// Could not find skill
							WLOG("Unknown skill found: " << skillId << " - skill will be ignored!");
						}
						else
						{
							// Our character knows that skill now
							out_character.addSkill(*skill);
							out_character.setSkillValue(skillId, current, max);
						}


						// Next row
						skillRow = skillRow.next(skillSelect);
					}
				}


				return true;
			}
			else
			{
				// Character did not exist?
				return false;
			}
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}
	}

	bool MySQLDatabase::saveGameCharacter(const GameCharacter &character, const std::vector<ItemData> &items)
	{
		GameTime start = getCurrentTime();
		MySQL::Transaction transaction(m_connection);

		float o = character.getOrientation();
		math::Vector3 location(character.getLocation());

		UInt32 homeMap;
		math::Vector3 homePos;
		float homeO;
		character.getHome(homeMap, homePos, homeO);

		std::ostringstream strm;
		for (UInt32 i = 0; i < 64; ++i)
		{
			strm << character.getUInt32Value(character_fields::ExploredZones_1 + i) << " ";
		}

		const UInt32 lowerGuid = guidLowerPart(character.getGuid());

		if (!m_connection.execute(fmt::format(
			"UPDATE `character` SET `map`={1}, `zone`={2}, `position_x`={3}, `position_y`={4}, `position_z`={5}, `orientation`={6}, `level`={7}, `xp`={8}, `gold`={9}, "
			"`home_map`={10}, `home_x`={11}, `home_y`={12}, `home_z`={13}, `home_o`={14}, `explored_zones`='{15}', `last_save`={16}, `last_group`={17}, `actionbars`={18}, `flags`={19},"
			"`played_time`={20}, `level_time`={21}, `health`={22},`power1`={23},`power2`={24},`power3`={25},`power4`={26},`power5`={27} WHERE `id`={0};"
			, lowerGuid															// 0
			, character.getMapId()												// 1
			, character.getZone()												// 2
			, location.x, location.y, location.z, o								// 3, 4, 5, 6
			, character.getLevel()												// 7
			, character.getUInt32Value(character_fields::Xp)					// 8
			, character.getUInt32Value(character_fields::Coinage)				// 9
			, homeMap															// 10
			, homePos.x, homePos.y, homePos.z, homeO							// 11, 12, 13, 14
			, strm.str()														// 15
			, time(nullptr)														// 16
			, character.getGroupId()											// 17
			, UInt32(character.getByteValue(character_fields::FieldBytes, 2))	// 18
			, character.getUInt32Value(character_fields::CharacterFlags)		// 19
			, character.getPlayTime(player_time_index::TotalPlayTime)			// 20
			, character.getPlayTime(player_time_index::LevelPlayTime)			// 21
			, character.getUInt32Value(unit_fields::Health)						// 22
			, character.getUInt32Value(unit_fields::Power1)						// 23
			, character.getUInt32Value(unit_fields::Power2)						// 24
			, character.getUInt32Value(unit_fields::Power3)						// 25
			, character.getUInt32Value(unit_fields::Power4)						// 26
			, character.getUInt32Value(unit_fields::Power5)						// 27
			)))
		{
			// There was an error
			printDatabaseError();
			return false;
		}

		// Delete character items
		if (!m_connection.execute(fmt::format(
			"DELETE FROM `character_items` WHERE `owner`={0};"
			, lowerGuid					// 0
			)))
		{
			// There was an error
			printDatabaseError();
			return false;
		}

		// Save character items
		if (!items.empty())
		{
			std::ostringstream strm;
			strm << "INSERT INTO `character_items` (`owner`, `entry`, `slot`, `creator`, `count`, `durability`) VALUES ";
			bool isFirstItem = true;
			for (auto &item : items)
			{
				if (!isFirstItem) strm << ",";
				else
				{
					isFirstItem = false;
				}

				strm << "(" << lowerGuid << "," << item.entry << "," << item.slot << ",";
				if (item.creator == 0)
				{
					strm << "NULL";
				}
				else
				{
					strm << item.creator;
				}
				strm << "," << UInt16(item.stackCount) << "," << item.durability << ")";
			}
			strm << ";";

			if (!m_connection.execute(strm.str()))
			{
				// There was an error
				printDatabaseError();
				return false;
			}
		}
		
		// Save character spells
		if (!m_connection.execute(fmt::format(
			"DELETE FROM `character_spells` WHERE `guid`={0};"
			, lowerGuid					// 0
		)))
		{
			// There was an error
			printDatabaseError();
			return false;
		}

		// Save character spells
		if (!character.getSpells().empty())
		{
			std::ostringstream strm;
			strm << "INSERT INTO `character_spells` (`guid`, `spell`) VALUES ";
			bool isFirstItem = true;
			for (auto &spell : character.getSpells())
			{
				if (!isFirstItem) strm << ",";
				else
				{
					isFirstItem = false;
				}

				strm << "(" << lowerGuid << "," << spell->id() << ")";
			}
			strm << ";";

			if (!m_connection.execute(strm.str()))
			{
				// There was an error
				printDatabaseError();
				return false;
			}
		}
		
		// Delete character skills
		if (!m_connection.execute(fmt::format(
			"DELETE FROM `character_skills` WHERE `guid`={0};"
			, lowerGuid					// 0
		)))
		{
			// There was an error
			printDatabaseError();
			return false;
		}

		// Gather character skills (TODO: Cache this somehow?)
		std::vector<UInt32> skillIds;
		for (UInt32 i = 0; i < 127; i++)
		{
			const UInt32 skillIndex = character_fields::SkillInfo1_1 + (i * 3);
			UInt32 skillId = character.getUInt32Value(skillIndex);
			if (skillId != 0)
			{
				skillIds.push_back(skillId);
			}
		}

		// If there are any skills at all...
		if (skillIds.size() > 0)
		{
			std::ostringstream strm;
			strm << "INSERT INTO `character_skills` (`guid`, `skill`, `current`, `max`) VALUES ";
			bool isFirstItem = true;
			for (const auto &skillId : skillIds)
			{
				UInt16 current = 1, max = 1;
				if (!character.getSkillValue(skillId, current, max))
				{
					WLOG("Could not get skill values of skill " << skillId << " for saving");
					continue;
				}

				if (!isFirstItem) strm << ",";
				else
				{
					isFirstItem = false;
				}
				
				strm << "(" << lowerGuid << "," << skillId << "," << current << "," << max << ")";
			}
			strm << ";";

			if (!m_connection.execute(strm.str()))
			{
				// There was an error
				printDatabaseError();
				return false;
			}
		}


		transaction.commit();
		GameTime end = getCurrentTime();
		DLOG("Saved character data in " << (end - start) << " ms");
		return true;
	}

	void MySQLDatabase::printDatabaseError()
	{
		ELOG("Realm database error: " << m_connection.getErrorMessage());
		assert(false);
	}

	bool MySQLDatabase::getCharacterById(DatabaseId id, game::CharEntry &out_character)
	{
		UInt32 lowerPart = guidLowerPart(id);
		wowpp::MySQL::Select select(m_connection,
			//      0     1       2       3        4        5       6        7       8    
			fmt::format("SELECT `id`, `name`, `race`, `class`, `gender`,`bytes`,`bytes2`,`level`,`map`,"
			//		 9       10            11            12           13		  14
			"`zone`,`position_x`,`position_y`,`position_z`,`orientation`,`cinematic` FROM `character` WHERE `id`={0} LIMIT 1"
			, lowerPart));
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			if (row)
			{
				UInt32 bytes = 0, bytes2 = 0;

				// Basic stuff
				row.getField(0, out_character.id);
				row.getField(1, out_character.name);

				// Display
				UInt32 tmp = 0;
				row.getField(2, tmp);
				out_character.race = static_cast<game::Race>(tmp);
				row.getField(3, tmp);
				out_character.class_ = static_cast<game::CharClass>(tmp);
				row.getField(4, tmp);
				out_character.gender = static_cast<game::Gender>(tmp);
				row.getField(5, bytes);
				row.getField(6, bytes2);
				row.getField(7, tmp);
				out_character.level = static_cast<UInt8>(tmp);

				// Placement
				row.getField(8, out_character.mapId);
				row.getField(9, out_character.zoneId);
				row.getField(10, out_character.location.x);
				row.getField(11, out_character.location.y);
				row.getField(12, out_character.location.z);
				row.getField(13, out_character.o);

				Int32 cinematic = 0;
				row.getField(14, cinematic);
				out_character.cinematic = (cinematic != 0);

				// Reinterpret bytes
				out_character.skin = static_cast<UInt8>(bytes);
				out_character.face = static_cast<UInt8>(bytes >> 8);
				out_character.hairStyle = static_cast<UInt8>(bytes >> 16);
				out_character.hairColor = static_cast<UInt8>(bytes >> 24);
				out_character.facialHair = static_cast<UInt8>(bytes2 & 0xff);
			}
			else
			{
				return false;
			}
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}

		return true;
	}

	bool MySQLDatabase::getCharacterByName(const String &name, game::CharEntry &out_character)
	{
		wowpp::MySQL::Select select(m_connection,
			//      0     1       2       3        4        5       6        7       8    
			fmt::format("SELECT `id`, `name`, `race`, `class`, `gender`,`bytes`,`bytes2`,`level`,`map`,"
			//		 9       10            11            12           13		  14
			"`zone`,`position_x`,`position_y`,`position_z`,`orientation`,`cinematic` FROM `character` WHERE `name`='{0}' LIMIT 1"
			, m_connection.escapeString(name)));
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			if (row)
			{
				UInt32 bytes = 0, bytes2 = 0;

				// Basic stuff
				row.getField(0, out_character.id);
				row.getField(1, out_character.name);

				// Display
				UInt32 tmp = 0;
				row.getField(2, tmp);
				out_character.race = static_cast<game::Race>(tmp);
				row.getField(3, tmp);
				out_character.class_ = static_cast<game::CharClass>(tmp);
				row.getField(4, tmp);
				out_character.gender = static_cast<game::Gender>(tmp);
				row.getField(5, bytes);
				row.getField(6, bytes2);
				row.getField(7, tmp);
				out_character.level = static_cast<UInt8>(tmp);

				// Placement
				row.getField(8, out_character.mapId);
				row.getField(9, out_character.zoneId);
				row.getField(10, out_character.location.x);
				row.getField(11, out_character.location.y);
				row.getField(12, out_character.location.z);
				row.getField(13, out_character.o);

				Int32 cinematic = 0;
				row.getField(14, cinematic);
				out_character.cinematic = (cinematic != 0);

				// Reinterpret bytes
				out_character.skin = static_cast<UInt8>(bytes);
				out_character.face = static_cast<UInt8>(bytes >> 8);
				out_character.hairStyle = static_cast<UInt8>(bytes >> 16);
				out_character.hairColor = static_cast<UInt8>(bytes >> 24);
				out_character.facialHair = static_cast<UInt8>(bytes2 & 0xff);
			}
			else
			{
				return false;
			}
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}

		return true;
	}

	bool MySQLDatabase::getCharacterSocialList(DatabaseId characterId, PlayerSocial &out_social)
	{
		wowpp::MySQL::Select select(m_connection,
			//                         0		1       2          
			fmt::format("SELECT `guid_2`, `flags`, `note` FROM `character_social` WHERE `guid_1`='{0}' LIMIT 75"
				, characterId));
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				UInt64 socialGuid = 0;
				UInt32 flags = 0;
				String note;

				row.getField(0, socialGuid);
				row.getField(1, flags);
				row.getField(2, note);

				const bool isFriend = flags & game::social_flag::Friend;
				out_social.addToSocialList(socialGuid, !isFriend);
				
				if (isFriend)
				{
					out_social.setFriendNote(socialGuid, std::move(note));
				}

				// Next row
				row = row.next(select);
			}
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}

		return true;
	}

	bool MySQLDatabase::addCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags, const String &note)
	{
		if (m_connection.execute(fmt::format(
			"INSERT INTO `character_social` (`guid_1`, `guid_2`, `flags`, `note`) VALUES ({0}, {1}, {2}, '{3}')"
			, characterId
			, socialGuid
			, flags
			, m_connection.escapeString(note))))
		{
			return true;
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}
	}

	bool MySQLDatabase::updateCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags)
	{
		if (m_connection.execute(fmt::format(
			"UPDATE `character_social` SET `flags`={0} WHERE `guid_1`={1} AND `guid_2`={2}"
			, flags				// 0
			, characterId		// 1
			, socialGuid)))		// 2
		{
			return true;
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}
	}

	bool MySQLDatabase::updateCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags, const String &note)
	{
		if (m_connection.execute(fmt::format(
			"UPDATE `character_social` SET `flags`={0}, `note`='{1}' WHERE `guid_1`={2} AND `guid_2`={3}"
			, flags								// 0
			, m_connection.escapeString(note)	// 1
			, characterId						// 2
			, socialGuid)))						// 3
		{
			return true;
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}
	}

	bool MySQLDatabase::removeCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid)
	{
		if (m_connection.execute(fmt::format(
			"DELETE FROM `character_social` WHERE `guid_1`={0} AND `guid_2`={1}"
			, characterId
			, socialGuid)))
		{
			return true;
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}
	}

	bool MySQLDatabase::getCharacterActionButtons(DatabaseId characterId, ActionButtons &out_buttons)
	{
		const UInt32 lowerPart = guidLowerPart(characterId);

		wowpp::MySQL::Select select(m_connection,
			fmt::format("SELECT `button`, `action`, `type` FROM `character_actions` WHERE `guid`={0}"
			, lowerPart));
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				UInt8 slot = 0;
				row.getField<UInt8, UInt16>(0, slot);

				ActionButton button;
				row.getField(1, button.action);
				row.getField<UInt8, UInt16>(2, button.type);

				// Add to vector
				out_buttons[slot] = std::move(button);

				// Next row
				row = row.next(select);
			}
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}

		return true;
	}

	bool MySQLDatabase::setCharacterActionButtons(DatabaseId characterId, const ActionButtons &buttons)
	{
		const UInt32 lowerPart = guidLowerPart(characterId);

		// Start transaction
		MySQL::Transaction transation(m_connection);
		{
			if (!m_connection.execute(fmt::format(
				"DELETE FROM `character_actions` WHERE `guid`={0}"
				, lowerPart)))
			{
				// There was an error
				printDatabaseError();
				return false;
			}

			if (!buttons.empty())
			{
				std::ostringstream fmtStrm;
				fmtStrm << "INSERT INTO `character_actions` (`guid`, `button`, `action`, `type`) VALUES ";

				// Add actions
				bool isFirstEntry = true;
				for (const auto &button : buttons)
				{
					if (button.second.action == 0)
						continue;;

					if (isFirstEntry)
					{
						isFirstEntry = false;
					}
					else
					{
						fmtStrm << ",";
					}

					fmtStrm << "(" << lowerPart << "," << static_cast<UInt32>(button.first) << "," << button.second.action << "," << static_cast<UInt32>(button.second.type) << ")";
				}

				// Now, set all actions
				if (!m_connection.execute(fmtStrm.str()))
				{
					// Could not learn initial spells
					printDatabaseError();
					return false;
				}
			}
			
		}
		transation.commit();
		return true;
	}

	bool MySQLDatabase::setCinematicState(DatabaseId characterId, bool state)
	{
		const UInt32 lowerPart = guidLowerPart(characterId);

		if (m_connection.execute(fmt::format(
			"UPDATE `character` SET `cinematic` = {0} WHERE `id`={1}"
			, (state ? 1 : 0)
			, lowerPart)))
		{
			return true;
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}
	}

	bool MySQLDatabase::setQuestData(DatabaseId characterId, UInt32 questId, const QuestStatusData & data)
	{
		const UInt32 lowerPart = guidLowerPart(characterId);

		if (m_connection.execute(fmt::format(
			"INSERT INTO `character_quests` (`guid`, `quest`, `status`, `explored`, `timer`, `unitcount1`, `unitcount2`, `unitcount3`, `unitcount4`, `objectcount1`, `objectcount2`, `objectcount3`, `objectcount4`, `itemcount1`, `itemcount2`, `itemcount3`, `itemcount4`) VALUES "
			"({0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}, {10}, {11}, {12}, {13}, {14}, {15}, {16}) "
			"ON DUPLICATE KEY UPDATE `status`={2}, `explored`={3}, `timer`={4}, `unitcount1`={5}, `unitcount2`={6}, `unitcount3`={7}, `unitcount4`={8}, `objectcount1`={9}, `objectcount2`={10}, `objectcount3`={11}, `objectcount4`={12}, `itemcount1`={13}, `itemcount2`={14}, `itemcount3`={15}, `itemcount4`={16}"
			, lowerPart
			, questId
			, static_cast<UInt32>(data.status)
			, (data.explored ? 1 : 0)
			, data.expiration
			, data.creatures[0]
			, data.creatures[1]
			, data.creatures[2]
			, data.creatures[3]
			, data.objects[0]
			, data.objects[1]
			, data.objects[2]
			, data.objects[3]
			, data.items[0]
			, data.items[1]
			, data.items[2]
			, data.items[3]
			)))
		{
			return true;
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}

		return false;
	}

	bool MySQLDatabase::teleportCharacter(DatabaseId characterId, UInt32 mapId, float x, float y, float z, float o, bool changeHome)
	{
		const UInt32 lowerPart = guidLowerPart(characterId);

		if (m_connection.execute(fmt::format(
			"UPDATE `character` SET `map`={0}, `position_x`={1}, `position_y`={2}, `position_z`={3}, `orientation`={4} WHERE `id`={5}"
			, mapId
			, x
			, y
			, z
			, o
			, lowerPart
			)))
		{
			return true;
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}

		return false;
	}

	bool MySQLDatabase::learnSpell(DatabaseId characterId, UInt32 spellId)
	{
		const UInt32 lowerPart = guidLowerPart(characterId);

		if (m_connection.execute(fmt::format(
			"INSERT IGNORE INTO `character_spells` VALUES ({0}, {1});"
			, lowerPart
			, spellId
			)))
		{
			return true;
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}

		return false;
	}
	bool MySQLDatabase::createGroup(UInt64 groupId, UInt64 leader)
	{
		const UInt32 lowerPart = guidLowerPart(leader);

		if (m_connection.execute(fmt::format(
			"INSERT INTO `group` (`id`, `leader`) VALUES ({0}, {1});"
			, groupId
			, lowerPart
			)))
		{
			return addGroupMember(groupId, leader);
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}
	}
	bool MySQLDatabase::disbandGroup(UInt64 groupId)
	{
		if (m_connection.execute(fmt::format(
			"DELETE FROM `group` WHERE `id` = {0};"
			, groupId
			)))
		{
			return true;
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}
	}
	bool MySQLDatabase::addGroupMember(UInt64 groupId, UInt64 member)
	{
		const UInt32 lowerPart = guidLowerPart(member);

		if (m_connection.execute(fmt::format(
			"INSERT IGNORE INTO `group_members` (`group`, `guid`) VALUES ({0}, {1});"
			, groupId
			, lowerPart
			)))
		{
			return true;
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}
	}
	bool MySQLDatabase::setGroupLeader(UInt64 groupId, UInt64 leaderGuid)
	{
		const UInt32 lowerPart = guidLowerPart(leaderGuid);
		if (m_connection.execute(fmt::format(
			"UPDATE `group` SET `leader` = {0} WHERE `id` = {1};"
			, lowerPart
			, groupId
			)))
		{
			return true;
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}
	}
	bool MySQLDatabase::removeGroupMember(UInt64 groupId, UInt64 member)
	{
		const UInt32 lowerPart = guidLowerPart(member);
		if (m_connection.execute(fmt::format(
			"DELETE FROM `group_members` WHERE `group` = {0} `guid` = {1};"
			, groupId
			, lowerPart
			)))
		{
			return true;
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}
	}
	bool MySQLDatabase::listGroups(std::vector<UInt64>& out_groupIds)
	{
		wowpp::MySQL::Select select(m_connection, "SELECT `id` FROM `group`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				UInt64 groupId = 0;
				row.getField(0, groupId);
				out_groupIds.push_back(groupId);

				// Next row
				row = row.next(select);
			}
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}

		return true;
	}
	bool MySQLDatabase::loadGroup(UInt64 groupId, UInt64 & out_leader, std::vector<UInt64>& out_member)
	{
		// Load group data
		{
			wowpp::MySQL::Select select(m_connection, fmt::format(
				"SELECT `leader` FROM `group` WHERE `id` = {0} LIMIT 1;"
				, groupId
				));
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				if (row)
				{
					// Load leader guid
					row.getField(0, out_leader);
				}
				else
				{
					// Could not find requested group
					WLOG("Could not find requested group " << groupId);
					return false;
				}
			}
			else
			{
				// There was an error
				printDatabaseError();
				return false;
			}
		}
		
		// Load members
		{
			wowpp::MySQL::Select select(m_connection, fmt::format(
				"SELECT `guid` FROM `group_members` WHERE `group` = {0} AND `guid` != {1} LIMIT 40;"
				, groupId
				, out_leader
				));
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				while (row)
				{
					// Load leader guid
					UInt64 memberGuid = 0;
					row.getField(0, memberGuid);
					out_member.push_back(memberGuid);

					row = row.next(select);
				}
			}
			else
			{
				// There was an error
				printDatabaseError();
				return false;
			}
		}

		return true;
	}
}
