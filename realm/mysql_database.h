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

#pragma once

#include "database.h"
#include "mysql_wrapper/mysql_connection.h"

namespace wowpp
{
	class Project;

	/// MySQL implementation of the realm server database system.
	class MySQLDatabase final : public IDatabase
	{
	public:

		/// Initializes a new instance of the MySQLDatabase class. Does not try to connect
		/// with the server, since the connection attempt could fail. Use load method to connect.
		/// @param connectionInfo Describes how to connect (server address, port etc.).
		explicit MySQLDatabase(Project& project, const MySQL::DatabaseInfo &connectionInfo);

		/// Tries to establish a connection to the MySQL server.
		bool load();

		/// @copydoc wowpp::IDatabase::getCharacterCount
		UInt32 getCharacterCount(UInt32 accountId) override;
		/// @copydoc wowpp::IDatabase::createCharacter
		game::ResponseCode createCharacter(UInt32 accountId, const std::vector<const SpellEntry*> &spells, game::CharEntry &character) override;
		/// @copydoc wowpp::IDatabase::createCharacterById
		bool getCharacterById(DatabaseId id, game::CharEntry &out_character) override;
		/// @copydoc wowpp::IDatabase::createCharacterByName
		bool getCharacterByName(const String &name, game::CharEntry &out_character) override;
		/// @copydoc wowpp::IDatabase::getCharacters
		bool getCharacters(UInt32 accountId, game::CharEntries &out_characters) override;
		/// @copydoc wowpp::IDatabase::deleteCharacter
		game::ResponseCode deleteCharacter(UInt32 accountId, UInt64 characterGuid) override;
		/// @copydoc wowpp::IDatabase::getGameCharacter
		bool getGameCharacter(DatabaseId characterId, GameCharacter &out_character) override;
		/// @copydoc wowpp::IDatabase::getCharacterSocialList
		bool getCharacterSocialList(DatabaseId characterId, PlayerSocial &out_social) override;
		/// @copydoc wowpp::IDatabase::addCharacterSocialContact
		bool addCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags, const String &note) override;
		/// @copydoc wowpp::IDatabase::addCharacterSocialContact
		bool updateCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags, const String &note) override;
		/// @copydoc wowpp::IDatabase::removeCharacterSocialContact
		bool removeCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid) override;

	private:

		/// Prints the last database error to the log.
		void printDatabaseError();

	private:

		Project &m_project;
		MySQL::DatabaseInfo m_connectionInfo;
		MySQL::Connection m_connection;
	};
}
