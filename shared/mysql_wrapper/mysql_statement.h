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
#include "include_mysql.h"
#include <boost/variant.hpp>
#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>
#include <vector>

namespace wowpp
{
	namespace MySQL
	{
		struct StatementResult;
		struct Connection;

		struct Null
		{
		};

		struct ConstStringPtr
		{
			const std::string *ptr;
		};

		typedef boost::variant<Null, Int64, double, std::string, ConstStringPtr> Bind;


		struct Statement : boost::noncopyable
		{
			Statement();
			Statement(Statement  &&other);
			explicit Statement(MYSQL &mysql);
			Statement(MYSQL &mysql, const char *query, size_t length);
			Statement(MYSQL &mysql, const std::string &query);
			Statement(Connection &mysql, const std::string &query);
			~Statement();
			Statement &operator = (Statement &&other);
			void swap(Statement &other);
			void prepare(const char *query, size_t length);
			size_t getParameterCount() const;
			void setParameter(std::size_t index, const Bind &argument);
			void setString(std::size_t index, const std::string &value);
			void setInt(std::size_t index, Int64 value);
			void setDouble(std::size_t index, double value);
			void execute();
			StatementResult executeSelect();

		private:

			MYSQL_STMT *m_handle;
			std::vector<boost::optional<Bind>> m_parameters;
			std::vector<MYSQL_BIND> m_convertedParameters;
			std::vector<MYSQL_BIND> m_boundResult;

			void allocateHandle(MYSQL &mysql);
		};


		struct StatementResult : boost::noncopyable
		{
			StatementResult();
			StatementResult(StatementResult &&other);
			explicit StatementResult(MYSQL_STMT &statementWithResults);
			~StatementResult();
			StatementResult &operator = (StatementResult &&other);
			void swap(StatementResult &other);
			void storeResult();
			size_t getAffectedRowCount() const;
			bool fetchResultRow();
			std::string getString(std::size_t index,
			                      std::size_t maxLengthInBytes = 1024 * 1024);
			Int64 getInt(std::size_t index);
			double getDouble(std::size_t index);
			bool getBoolean(std::size_t index);

		private:

			MYSQL_STMT *m_statement;
		};
	}
}
