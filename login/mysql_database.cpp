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
			ELOG("Could not connect to the login database");
			ELOG(m_connection.getErrorMessage());
			return false;
		}

		// Mark all realms as offline
		setAllRealmsOffline();
		ILOG("Connected to MySQL at " <<
			m_connectionInfo.host << ":" <<
			m_connectionInfo.port);

		return true;
	}

	bool MySQLDatabase::createAccount(UInt64 accountId, const String & accountName, const String & passwordHash)
	{
		const String safeName = m_connection.escapeString(accountName);
		const String safeHash = m_connection.escapeString(passwordHash);

		bool result = false;
		if (accountId != 0)
		{
			result = m_connection.execute(fmt::format(
				"INSERT INTO account (`id`,`username`,`password`) VALUES ({0}, '{1}', '{2}');"
				, accountId		// 0
				, safeName		// 1
				, safeHash));	// 2
		}
		else
		{
			result = m_connection.execute(fmt::format(
				"INSERT INTO account (`username`,`password`) VALUES ('{0}', '{1}');"
				, safeName		// 0
				, safeHash));	// 1
		}

		if (!result)
		{
			printDatabaseError();
		}

		return result;
	}

	bool MySQLDatabase::setPlayerPassword(UInt64 accountId, const String & passwordHash)
	{
		const String safeHash = m_connection.escapeString(passwordHash);

		if (m_connection.execute(fmt::format(
			"UPDATE account SET password='{0}' WHERE id={1}"
			, safeHash		// 0
			, accountId)))	// 1
		{
			return true;
		}
		else
		{
			printDatabaseError();
		}

		return false;
	}

	bool MySQLDatabase::getAccountInfos(UInt64 accountId, String & out_name, String & out_passwordHash)
	{
		wowpp::MySQL::Select select(m_connection, fmt::format(
			"SELECT username,password FROM account WHERE id={0} LIMIT 1", accountId));
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			if (row)
			{
				// Account exists: Get name and password
				row.getField(0, out_name);
				row.getField(1, out_passwordHash);
			}
			else
			{
				// No row found: Account does not exist
				return true;
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

	bool MySQLDatabase::getPlayerPassword(const String &userName, UInt32 &out_id, String &out_passwordHash)
	{
		// Escape string to avoid sql injection
		const wowpp::String safeName = m_connection.escapeString(userName);

		wowpp::MySQL::Select select(m_connection, fmt::format(
			"SELECT id,password FROM account WHERE username='{0}' LIMIT 1" , safeName));
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			if (row)
			{
				// Account exists: Get id and password
				row.getField(0, out_id);
				row.getField(1, out_passwordHash);
			}
			else
			{
				// No row found: Account does not exist
				return true;
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

	bool MySQLDatabase::getSVFields(const UInt32 &userId, BigNumber &out_S, BigNumber &out_V)
	{
		wowpp::MySQL::Select select(m_connection, fmt::format("SELECT v,s FROM account WHERE id={0} LIMIT 1", userId));
		if (select.success())
		{
			wowpp::String S, V;
			wowpp::MySQL::Row row(select);
			if (row)
			{
				// Account exists: Get S and V
				row.getField(0, V);
				row.getField(1, S);

				// BigNumber conversion
				out_S.setHexStr(S);
				out_V.setHexStr(V);
			}
			else
			{
				// No row found: Account does not exist
				return false;
			}
		}
		else
		{
			// There was an error
			printDatabaseError();
			return false;
		}

		return false;
	}

	bool MySQLDatabase::setSVFields(const UInt32 &userId, const BigNumber &S, const BigNumber &V)
	{
		const String safeS = m_connection.escapeString(S.asHexStr());
		const String safeV = m_connection.escapeString(V.asHexStr());

		if (m_connection.execute(fmt::format("UPDATE account SET v='{0}',s='{1}' WHERE id={2}"
			, safeV		// 0
			, safeS		// 1
			, userId)))	// 2
		{
			return true;
		}
		else
		{
			printDatabaseError();
		}

		return false;
	}

	pp::realm_login::LoginResult MySQLDatabase::realmLogIn(UInt32 &out_id, const String &name, const String &password)
	{
		using namespace pp::realm_login;

		const String safeName = m_connection.escapeString(name);

		MySQL::Select select(m_connection, fmt::format(
			"SELECT id,password FROM realm WHERE internalName='{0}' LIMIT 1"
			, safeName));		// 0
		if (select.success())
		{
			UInt32 id = 0xffffffff;
			String correctPassword;

			MySQL::Row row(select);
			if (row)
			{
				row.getField(0, id);
				row.getField(1, correctPassword);

				// Wrong password
				if (correctPassword != password)
				{
					WLOG("Realm " << name << " tried to login with a wrong password");
					return login_result::WrongPassword;
				}

				out_id = id;
				return login_result::Success;
			}
			else
			{
				return login_result::UnknownRealm;
			}
		}
		else
		{
			printDatabaseError();
		}

		return login_result::ServerError;
	}

	bool MySQLDatabase::setAllRealmsOffline()
	{
		if (m_connection.execute("UPDATE realm SET online=0,players=0"))
		{
			return true;
		}
		else
		{
			printDatabaseError();
		}

		return false;
	}

	bool MySQLDatabase::setRealmOnline(UInt32 id, const String &visibleName, const String &host, NetPort port)
	{
		const String saveName = m_connection.escapeString(visibleName);
		const String safeHost = m_connection.escapeString(host);

		if (m_connection.execute(fmt::format(
			"UPDATE realm SET online=1,lastVisibleName='{0}',lastHost='{1}',lastPort={2} WHERE id={3}"
			, saveName		// 0
			, safeHost		// 1
			, port			// 2
			, id)))			// 3
		{
			return true;
		}
		else
		{
			printDatabaseError();
		}

		return false;
	}

	bool MySQLDatabase::setRealmOffline(UInt32 id)
	{
		if (m_connection.execute(fmt::format(
			"UPDATE realm SET online=0,players=0 WHERE id={0}", id)))
		{
			return true;
		}
		else
		{
			printDatabaseError();
		}

		return false;
	}

	bool MySQLDatabase::setRealmCurrentPlayerCount(UInt32 id, size_t players)
	{
		if (m_connection.execute(fmt::format(
			"UPDATE realm SET players={0} WHERE id={1}"
			, players
			, id)))
		{
			return true;
		}
		else
		{
			printDatabaseError();
		}

		return false;
	}

	void MySQLDatabase::printDatabaseError()
	{
		ELOG("Login database error: " << m_connection.getErrorMessage());
	}

	bool MySQLDatabase::getTutorialData(UInt32 id, std::array<UInt32, 8> &out_data)
	{
		wowpp::MySQL::Select select(m_connection, fmt::format(
			"SELECT tutorial_0, tutorial_1, tutorial_2, tutorial_3, tutorial_4, tutorial_5, tutorial_6, tutorial_7 FROM account_tutorials WHERE account={0} LIMIT 1"
			, id));		// 0
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			if (row)
			{
				// Account exists: Get S and V
				for (size_t i = 0; i < out_data.size(); ++i)
				{
					row.getField(i, out_data[i]);
				}
			}
			else
			{
				// No row found: Account does not exist
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

	bool MySQLDatabase::setTutorialData(UInt32 id, const std::array<UInt32, 8> data)
	{
		if (!m_connection.execute(fmt::format(
			"INSERT INTO account_tutorials SET account = {0}, tutorial_0 = {1}, tutorial_1 = {2}, tutorial_2 = {3}, tutorial_3 = {4}, tutorial_4 = {5}, tutorial_5 = {6}, tutorial_6 = {7}, tutorial_7 = {8} ON DUPLICATE KEY UPDATE "
				"tutorial_0 = {1}, tutorial_1 = {2}, tutorial_2 = {3}, tutorial_3 = {4}, tutorial_4 = {5}, tutorial_5 = {6}, tutorial_6 = {7}, tutorial_7 = {8}"
			, id			// 0
			, data[0]		// 1
			, data[1]		// 2
			, data[2]		// 3
			, data[3]		// 4
			, data[4]		// 5
			, data[5]		// 6
			, data[6]		// 7
			, data[7])))	// 8
		{
			printDatabaseError();
			return false;
		}

		return true;
	}
	pp::team_login::LoginResult MySQLDatabase::teamServerLogIn(UInt32 & out_id, const String & name, const String & password)
	{
		using namespace pp::team_login;

		const String safeName = m_connection.escapeString(name);

		MySQL::Select select(m_connection, fmt::format(
			"SELECT id,password FROM team_server WHERE internalName='{0}' LIMIT 1"
			, safeName));		// 0
		if (select.success())
		{
			UInt32 id = 0xffffffff;
			String correctPassword;

			MySQL::Row row(select);
			if (row)
			{
				row.getField(0, id);
				row.getField(1, correctPassword);

				// Wrong password
				if (correctPassword != password)
				{
					WLOG("Team server " << name << " tried to login with a wrong password");
					return login_result::WrongPassword;
				}

				out_id = id;
				return login_result::Success;
			}
			else
			{
				return login_result::UnknownTeamServer;
			}
		}
		else
		{
			printDatabaseError();
		}

		return login_result::ServerError;
	}
}
