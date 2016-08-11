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

	private:

		/// Prints the last database error to the log.
		void printDatabaseError();

	private:

		proto::Project &m_project;
		MySQL::DatabaseInfo m_connectionInfo;
		MySQL::Connection m_connection;
	};
}
