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

#pragma once

#include "database.h"
#include "mysql_wrapper/mysql_connection.h"

namespace wowpp
{
	namespace proto
	{
		class Project;
	}

	/// MySQL implementation of the realm server database system.
	class MySQLDatabase final : public IDatabase
	{
	public:

		/// Initializes a new instance of the MySQLDatabase class. Does not try to connect
		/// with the server, since the connection attempt could fail. Use load method to connect.
		/// @param connectionInfo Describes how to connect (server address, port etc.).
		explicit MySQLDatabase(proto::Project& project, const MySQL::DatabaseInfo &connectionInfo);

		/// Tries to establish a connection to the MySQL server.
		bool load();

		/// @copydoc wowpp::IDatabase::renameCharacter
		game::ResponseCode renameCharacter(DatabaseId id, const String &newName) override;
		/// @copydoc wowpp::IDatabase::getCharacterCount
		UInt32 getCharacterCount(UInt32 accountId) override;
		/// @copydoc wowpp::IDatabase::createCharacter
		game::ResponseCode createCharacter(UInt32 accountId, const std::vector<const proto::SpellEntry*> &spells, const std::vector<pp::world_realm::ItemData> &items, game::CharEntry &character) override;
		/// @copydoc wowpp::IDatabase::createCharacterById
		bool getCharacterById(DatabaseId id, game::CharEntry &out_character) override;
		/// @copydoc wowpp::IDatabase::createCharacterByName
		bool getCharacterByName(const String &name, game::CharEntry &out_character) override;
		/// @copydoc wowpp::IDatabase::getCharacters
		bool getCharacters(UInt32 accountId, game::CharEntries &out_characters) override;
		/// @copydoc wowpp::IDatabase::deleteCharacter
		game::ResponseCode deleteCharacter(UInt32 accountId, UInt64 characterGuid) override;
		/// @copydoc wowpp::IDatabase::getGameCharacter
		bool getGameCharacter(DatabaseId characterId, GameCharacter &out_character, std::vector<pp::world_realm::ItemData> &out_items) override;
		/// @copydoc wowpp::IDatabase::saveGamecharacter
		bool saveGameCharacter(const GameCharacter &character, const std::vector<pp::world_realm::ItemData> &items, const std::vector<UInt32> &spells) override;
		/// @copydoc wowpp::IDatabase::getCharacterSocialList
		bool getCharacterSocialList(DatabaseId characterId, PlayerSocial &out_social) override;
		/// @copydoc wowpp::IDatabase::addCharacterSocialContact
		bool addCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags, const String &note) override;
		/// @copydoc wowpp::IDatabase::updateCharacterSocialContact
		bool updateCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags) override;
		/// @copydoc wowpp::IDatabase::updateCharacterSocialContact
		bool updateCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags, const String &note) override;
		/// @copydoc wowpp::IDatabase::removeCharacterSocialContact
		bool removeCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid) override;
		/// @copydoc wowpp::IDatabase::removeCharacterSocialContact
		bool getCharacterActionButtons(DatabaseId characterId, ActionButtons &out_buttons) override;
		/// @copydoc wowpp::IDatabase::setCharacterActionButtons
		bool setCharacterActionButtons(DatabaseId characterId, const ActionButtons &buttons) override;
		/// @copydoc wowpp::IDatabase::setCinematicState
		bool setCinematicState(DatabaseId characterId, bool state) override;
		/// @copydoc wowpp::IDatabase::setQuestData
		bool setQuestData(DatabaseId characterId, UInt32 questId, const QuestStatusData &data) override;

	private:

		/// Prints the last database error to the log.
		void printDatabaseError();

	private:

		proto::Project &m_project;
		MySQL::DatabaseInfo m_connectionInfo;
		MySQL::Connection m_connection;
	};
}
