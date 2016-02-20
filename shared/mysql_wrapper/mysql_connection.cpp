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

#include "mysql_connection.h"
#include "mysql_exception.h"
#include <exception>
#include <cassert>

namespace wowpp
{
	namespace MySQL
	{
		DatabaseInfo::DatabaseInfo()
			: port(0)
		{
		}

		DatabaseInfo::DatabaseInfo(
		    const String &host,
		    NetPort port,
		    const String &user,
		    const String &password,
		    const String &database)
			: host(host)
			, port(port)
			, user(user)
			, password(password)
			, database(database)
		{
		}


		Connection::Connection()
			: m_mySQL(new MYSQL)
			, m_isConnected(false)
		{
			if (!::mysql_init(m_mySQL.get()))
			{
				throw std::bad_alloc();
			}
		}

		Connection::Connection(const DatabaseInfo &info)
			: m_mySQL(new MYSQL)
			, m_isConnected(false)
		{
			if (!::mysql_init(m_mySQL.get()))
			{
				throw std::bad_alloc();
			}
			if (!connect(info))
			{
				throw Exception(mysql_error(getHandle()));
			}
		}

		Connection::Connection(Connection  &&other)
			: m_isConnected(false)
		{
			swap(other);
		}

		Connection::~Connection()
		{
			if (getHandle())
			{
				::mysql_close(getHandle());
			}
		}

		Connection &Connection::operator = (Connection &&other)
		{
			swap(other);
			return *this;
		}

		void Connection::swap(Connection &other)
		{
			using std::swap;
			swap(m_mySQL, other.m_mySQL);
			swap(m_isConnected, other.m_isConnected);
		}

		bool Connection::connect(const DatabaseInfo &info)
		{
			assert(!m_isConnected);

			{
				my_bool reconnect = true;
				if (::mysql_options(getHandle(), MYSQL_OPT_RECONNECT, &reconnect) != 0)
				{
					return false;
				}
			}

			MYSQL *const result = ::mysql_real_connect(getHandle(),
			                      info.host.c_str(),
			                      info.user.c_str(),
			                      info.password.c_str(),
			                      info.database.c_str(),
			                      static_cast<UInt16>(info.port),
			                      0,
			                      0);

			m_isConnected = (result != nullptr);
			return m_isConnected;
		}

		bool Connection::execute(const String &query)
		{
			assert(m_isConnected);

			const int result = ::mysql_real_query(getHandle(),
			                                      query.data(),
			                                      static_cast<unsigned long>(query.size()));
			return (result == 0);
		}

		void Connection::tryExecute(const String &query)
		{
			if (!execute(query))
			{
				throw QueryFailException(getErrorMessage());
			}
		}

		const char *Connection::getErrorMessage()
		{
			return ::mysql_error(getHandle());
		}

		int Connection::getErrorCode()
		{
			return ::mysql_errno(getHandle());
		}

		MYSQL_RES *Connection::storeResult()
		{
			assert(m_isConnected);
			return ::mysql_store_result(getHandle());
		}

		bool Connection::keepAlive()
		{
			return (::mysql_ping(getHandle()) == 0);
		}

		String Connection::escapeString(const String &str)
		{
			const std::size_t worstCaseLength = (str.size() * 2 + 1);
			std::vector<char> temp(worstCaseLength);
			char *const begin = temp.data();
			const std::size_t length = ::mysql_real_escape_string(getHandle(), begin, str.data(), static_cast<unsigned long>(str.size()));
			const char *const end = (begin + length);
			return String(static_cast<const char *>(begin), end);
		}

		MYSQL *Connection::getHandle()
		{
			return m_mySQL.get();
		}

		bool Connection::operator_bool() const
		{
			return m_isConnected;
		}


		Transaction::Transaction(Connection &connection)
			: m_connection(connection)
			, m_isCommit(false)
		{
			m_connection.tryExecute("START TRANSACTION");
		}

		Transaction::~Transaction()
		{
			if (!m_isCommit)
			{
				m_connection.tryExecute("ROLLBACK");
			}
		}

		void Transaction::commit()
		{
			assert(!m_isCommit);
			m_connection.tryExecute("COMMIT");
			m_isCommit = true;
		}
	}
}
