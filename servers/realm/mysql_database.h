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
		boost::optional<UInt32> getCharacterCount(UInt32 accountId) override;
		/// @copydoc wowpp::IDatabase::createCharacter
		game::ResponseCode createCharacter(UInt32 accountId, const std::vector<const proto::SpellEntry*> &spells, const std::vector<ItemData> &items, game::CharEntry &character) override;
		/// @copydoc wowpp::IDatabase::createCharacterById
		game::CharEntry getCharacterById(DatabaseId id) override;
		/// @copydoc wowpp::IDatabase::createCharacterByName
		game::CharEntry getCharacterByName(const String &name) override;
		/// @copydoc wowpp::IDatabase::getDeletedCharacters
		game::CharEntries getDeletedCharacters(UInt32 accountId) override;
		/// @copydoc wowpp::IDatabase::getCharacters
		game::CharEntries getCharacters(UInt32 accountId) override;
		/// @copydoc wowpp::IDatabase::deleteCharacter
		void deleteCharacter(DeleteCharacterArgs arguments) override;
		/// @copydoc wowpp::IDatabase::getGameCharacter
		bool getGameCharacter(DatabaseId characterId, GameCharacter &out_character) override;
		/// @copydoc wowpp::IDatabase::saveGamecharacter
		bool saveGameCharacter(const GameCharacter &character, const std::vector<ItemData> &items) override;
		/// @copydoc wowpp::IDatabase::getCharacterSocialList
		boost::optional<PlayerSocialEntries> getCharacterSocialList(DatabaseId characterId) override;
		/// @copydoc wowpp::IDatabase::addCharacterSocialContact
		void addCharacterSocialContact(AddSocialContactArg arguments) override;
		/// @copydoc wowpp::IDatabase::updateCharacterSocialContact
		void updateCharacterSocialContact(UpdateSocialContactArg arguments) override;
		/// @copydoc wowpp::IDatabase::updateCharacterSocialContact
		void updateCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags, const String &note) override;
		/// @copydoc wowpp::IDatabase::removeCharacterSocialContact
		void removeCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid) override;
		/// @copydoc wowpp::IDatabase::removeCharacterSocialContact
		bool getCharacterActionButtons(DatabaseId characterId, ActionButtons &out_buttons) override;
		/// @copydoc wowpp::IDatabase::setCharacterActionButtons
		void setCharacterActionButtons(DatabaseId characterId, const ActionButtons &buttons) override;
		/// @copydoc wowpp::IDatabase::setCinematicState
		void setCinematicState(DatabaseId characterId, bool state) override;
		/// @copydoc wowpp::IDatabase::setQuestData
		void setQuestData(DatabaseId characterId, UInt32 questId, const QuestStatusData &data) override;
		/// @copydoc wowpp::IDatabase::teleportCharacter
		void teleportCharacter(DatabaseId characterId, UInt32 mapId, float x, float y, float z, float o, bool changeHome = false) override;
		/// @copydoc wowpp::IDatabase::learnSpell
		void learnSpell(DatabaseId characterId, UInt32 spellId) override;
		/// @copydoc wowpp::IDatabase::createGroup
		void createGroup(UInt64 groupId, UInt64 leader) override;
		/// @copydoc wowpp::IDatabase::disbandGroup
		void disbandGroup(UInt64 groupId) override;
		/// @copydoc wowpp::IDatabase::addGroupMember
		void addGroupMember(UInt64 groupId, UInt64 member) override;
		/// @copydoc wowpp::IDatabase::setGroupLeader
		void setGroupLeader(UInt64 groupId, UInt64 leaderGuid) override;
		/// @copydoc wowpp::IDatabase::removeGroupMember
		void removeGroupMember(UInt64 groupId, UInt64 member) override;
		/// @copydoc wowpp::IDatabase::listGroups
		boost::optional<std::vector<UInt64>> listGroups() override;
		/// @copydoc wowpp::IDatabase::loadGroup
		boost::optional<GroupData> loadGroup(UInt64 groupId) override;

	private:

		/// Prints the last database error to the log.
		void printDatabaseError();

	private:

		proto::Project &m_project;
		MySQL::DatabaseInfo m_connectionInfo;
		MySQL::Connection m_connection;
	};
}
