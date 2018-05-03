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

#include "common/typedefs.h"

namespace wowpp
{
	namespace MySQL
	{
		struct DatabaseInfo
		{
			String host;
			NetPort port;
			String user;
			String password;
			String database;


			DatabaseInfo();
			DatabaseInfo(
			    const String &host,
			    NetPort port,
			    const String &user,
			    const String &password,
			    const String &database);
		};


		struct Connection
			: boost::spirit::classic::safe_bool<Connection>
		{
		private:

			Connection(const Connection &Other) = delete;
			Connection &operator=(const Connection &Other) = delete;

		public:

			Connection();
			Connection(Connection  &&other);
			explicit Connection(const DatabaseInfo &info);
			~Connection();
			Connection &operator = (Connection &&other);
			void swap(Connection &other);
			bool connect(const DatabaseInfo &info);
			bool execute(const String &query);
			void tryExecute(const String &query);
			const char *getErrorMessage();
			int getErrorCode();
			MYSQL_RES *storeResult();
			bool keepAlive();
			String escapeString(const String &str);
			MYSQL *getHandle();
			bool operator_bool() const;

		private:

			std::unique_ptr<MYSQL> m_mySQL;
			bool m_isConnected;
		};


		struct Transaction
		{
		private:

			Transaction(const Transaction &Other) = delete;
			Transaction &operator=(const Transaction &Other) = delete;

		public:

			explicit Transaction(Connection &connection);
			~Transaction();
			void commit();

		private:

			Connection &m_connection;
			bool m_isCommit;
		};
	}
}
