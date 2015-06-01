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
	/// MySQL implementation of the login server database system.
	class MySQLDatabase final : public IDatabase
	{
	public:

		explicit MySQLDatabase(const MySQL::DatabaseInfo &connectionInfo);

		/// Tries to establish a connection to the MySQL server.
		bool load();

		/// @copydoc wow::IDatabase::getPlayerPassword
		bool getPlayerPassword(const String &userName, UInt32 &out_id, String &out_passwordHash) override;
		/// @copydoc wow::IDatabase::getSVFields
		bool getSVFields(const UInt32 &userId, BigNumber &out_S, BigNumber &out_V) override;
		/// @copydoc wow::IDatabase::setSVFields
		bool setSVFields(const UInt32 &userId, const BigNumber &S, const BigNumber &V) override;
		/// @copydoc wow::IDatabase::realmLogIn
		pp::realm_login::LoginResult realmLogIn(UInt32 &out_id, const String &name, const String &password) override;
		/// @copydoc wow::IDatabase::setAllRealmsOffline
		bool setAllRealmsOffline() override;
		/// @copydoc wow::IDatabase::setRealmOnline
		bool setRealmOnline(UInt32 id, const String &visibleName, const String &host, NetPort port) override;
		/// @copydoc wow::IDatabase::setRealmOffline
		bool setRealmOffline(UInt32 id) override;
		/// @copydoc wow::IDatabase::setRealmCurrentPlayerCount
		bool setRealmCurrentPlayerCount(UInt32 id, size_t players) override;

	private:

		void printDatabaseError();

	private:

		MySQL::DatabaseInfo m_connectionInfo;
		MySQL::Connection m_connection;
	};
}
