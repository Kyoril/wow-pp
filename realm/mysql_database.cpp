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

#include "mysql_database.h"
#include "mysql_wrapper/mysql_row.h"
#include "mysql_wrapper/mysql_select.h"
#include "mysql_wrapper/mysql_statement.h"
#include "common/constants.h"
#include <boost/format.hpp>
#include "log/default_log_levels.h"

namespace wowpp
{
	MySQLDatabase::MySQLDatabase(const MySQL::DatabaseInfo &connectionInfo)
		: m_connectionInfo(connectionInfo)
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

	wowpp::UInt32 MySQLDatabase::getCharacterCount(UInt32 accountId)
	{
		UInt32 retCount = 0;

		wowpp::MySQL::Select select(m_connection,
			(boost::format("SELECT COUNT(id) FROM `character` WHERE `account`=%1%")
			% accountId).str());

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

	game::ResponseCode MySQLDatabase::createCharacter(UInt32 accountId, game::CharEntry &character)
	{
		// See if the character name is already in use
		const String safeName = m_connection.escapeString(character.name);

		// Check if character name is already in use
		{
			wowpp::MySQL::Select select(m_connection,
				(boost::format("SELECT `id` FROM `character` WHERE `name`='%1%' LIMIT 1")
				% safeName).str());
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				if (row)
				{
					// There was an error
					printDatabaseError();
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
				(boost::format("SELECT `race` FROM `character` WHERE `account`=%1%")
				% accountId).str());
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

		if (m_connection.execute((boost::format(
			//                           0        1      2      3        4        5       6       7    8        9          10            11            12			13
			"INSERT INTO `character` (`account`,`name`,`race`,`class`,`gender`,`bytes`,`bytes2`,`map`,`zone`,`position_x`,`position_y`,`position_z`,`orientation`,`cinematic`) VALUES (%1%, '%2%', %3%, %4%, %5%, %6%, %7%, %8%, %9%, %10%, %11%, %12%, %13%, %14%)")
			% accountId
			% safeName
			% static_cast<UInt32>(character.race)
			% static_cast<UInt32>(character.class_)
			% static_cast<UInt32>(character.gender)
			% bytes
			% bytes2
			% character.mapId
			% character.zoneId
			% character.x
			% character.y
			% character.z
			% character.o
			% 1).str()))
		{
			// Retrieve id of the newly created character
			wowpp::MySQL::Select select(m_connection,
				(boost::format("SELECT `id` FROM `character` WHERE `name`='%1%' LIMIT 1")
				% safeName).str());
			if (select.success())
			{
				wowpp::MySQL::Row row(select);
				if (row)
				{
					row.getField(0, character.id);
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
			(boost::format("SELECT `id`, `name`, `race`, `class`, `gender`,`bytes`,`bytes2`,`level`,`map`,"
							//		 9       10            11            12           13		  14
								 "`zone`,`position_x`,`position_y`,`position_z`,`orientation`,`cinematic` FROM `character` WHERE `account`=%1% ORDER BY `id`")
			% accountId).str());
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
				row.getField(10, entry.x);
				row.getField(11, entry.y);
				row.getField(12, entry.z);
				row.getField(13, entry.o);
				
				Int32 cinematic = 0;
				row.getField(14, cinematic);
				entry.cinematic = (cinematic != 0);

				// Reinterpret bytes
				entry.skin = static_cast<UInt8>(bytes);
				entry.face = static_cast<UInt8>(bytes >> 8);
				entry.hairStyle = static_cast<UInt8>(bytes >> 16);
				entry.hairColor = static_cast<UInt8>(bytes >> 24);
				entry.facialHair = static_cast<UInt8>(bytes2 & 0xff);

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

		return true;
	}

	game::ResponseCode MySQLDatabase::deleteCharacter(UInt32 accountId, UInt32 characterGuid)
	{
		if (m_connection.execute((boost::format(
			"DELETE FROM `character` WHERE `id`=%1% AND `account`=%2%")
			% characterGuid 
			% accountId).str()))
		{
			return game::response_code::CharDeleteSuccess;
		}
		else
		{
			// There was an error
			printDatabaseError();
			return game::response_code::CharDeleteFailed;
		}
	}

	bool MySQLDatabase::getGameCharacter(DatabaseId characterId, GameCharacter &out_character)
	{
		wowpp::MySQL::Select select(m_connection, (boost::format(
			//       0       1       2        3        4       5        6       7     8       9     
			"SELECT `name`, `race`, `class`, `gender`,`bytes`,`bytes2`,`level`,`xp`, `gold`, `map`,"
			//       10     11           12           13           14
				   "`zone`,`position_x`,`position_y`,`position_z`,`orientation` FROM `character` WHERE `id`=%1% LIMIT 1")
			% characterId).str());
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

				//TODO Character flags

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
				out_character.relocate(x, y, z, o);
				out_character.setMapId(mapId);

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

	void MySQLDatabase::printDatabaseError()
	{
		ELOG("Realm database error: " << m_connection.getErrorMessage());
	}

}
