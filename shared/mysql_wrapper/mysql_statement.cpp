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
#include "mysql_statement.h"
#include "mysql_exception.h"
#include "mysql_connection.h"

namespace wowpp
{
	namespace MySQL
	{
		namespace
		{
			void throwError(MYSQL_STMT &statement)
			{
				const auto rc = mysql_stmt_errno(&statement);
				throw StatementException(
				    boost::lexical_cast<std::string>(rc) + ", " +
				    mysql_stmt_error(&statement)
				);
			}

			void checkResultCode(MYSQL_STMT &statement, int rc)
			{
				if (rc != 0)
				{
					throwError(statement);
				}
			}
		}


		Statement::Statement()
			: m_handle(nullptr)
		{
		}

		Statement::Statement(Statement &&other)
			: m_handle(nullptr)
		{
			swap(other);
		}

		Statement::Statement(MYSQL &mysql)
		{
			allocateHandle(mysql);
		}

		Statement::Statement(MYSQL &mysql, const char *query, size_t length)
		{
			allocateHandle(mysql);
			prepare(query, length);
		}

		Statement::Statement(MYSQL &mysql, const std::string &query)
		{
			allocateHandle(mysql);
			prepare(query.data(), query.length());
		}

		Statement::Statement(Connection &mysql, const std::string &query)
		{
			{
				auto *const mysqlHandle = mysql.getHandle();
				assert(mysqlHandle);
				allocateHandle(*mysqlHandle);
			}
			prepare(query.data(), query.length());
		}

		Statement::~Statement()
		{
			if (m_handle)
			{
				mysql_stmt_close(m_handle);
			}
		}

		Statement &Statement::operator = (Statement &&other)
		{
			swap(other);
			return *this;
		}

		void Statement::swap(Statement &other)
		{
			using std::swap;
			swap(m_handle, other.m_handle);
			swap(m_parameters, other.m_parameters);
			swap(m_convertedParameters, other.m_convertedParameters);
			swap(m_boundResult, other.m_boundResult);
		}

		void Statement::prepare(const char *query, size_t length)
		{
			{
				const int rc = mysql_stmt_prepare(m_handle,
				                                  query,
				                                  static_cast<unsigned long>(length));
				checkResultCode(*m_handle, rc);
				m_parameters.resize(getParameterCount());
			}

			if (mysql_stmt_field_count(m_handle) > 0)
			{
				const MYSQL_BIND null = {};
				m_boundResult.resize(mysql_stmt_field_count(m_handle), null);
				const int rc = mysql_stmt_bind_result(m_handle, m_boundResult.data());
				checkResultCode(*m_handle, rc);
			}
		}

		size_t Statement::getParameterCount() const
		{
			return static_cast<size_t>(
			           mysql_stmt_param_count(m_handle));
		}

		namespace
		{
			struct BindToMySQLConverter : boost::static_visitor<MYSQL_BIND>
			{
				BindToMySQLConverter()
					: boost::static_visitor<MYSQL_BIND>()
				{
				}

				MYSQL_BIND operator ()(Null) const
				{
					MYSQL_BIND result = {};
					result.buffer_type = MYSQL_TYPE_NULL;
					return result;
				}

				MYSQL_BIND operator ()(const Int64 &value) const
				{
					MYSQL_BIND result = {};
					result.buffer_type = MYSQL_TYPE_LONGLONG;
					result.buffer = const_cast<void *>(static_cast<const void *>(&value));
					result.buffer_length = 8;
					return result;
				}

				MYSQL_BIND operator ()(const double &value) const
				{
					MYSQL_BIND result = {};
					result.buffer_type = MYSQL_TYPE_DOUBLE;
					result.buffer = const_cast<void *>(static_cast<const void *>(&value));
					result.buffer_length = 8;
					return result;
				}

				MYSQL_BIND operator ()(const std::string &value) const
				{
					MYSQL_BIND result = {};
					result.buffer_type = MYSQL_TYPE_STRING;
					result.buffer = const_cast<void *>(static_cast<const void *>(value.data()));
					result.buffer_length = static_cast<unsigned long>(value.size());
					return result;
				}

				MYSQL_BIND operator ()(ConstStringPtr value) const
				{
					return operator ()(*value.ptr);
				}
			};

			MYSQL_BIND convertBind(const Bind &bind)
			{
				const BindToMySQLConverter converter;
				return boost::apply_visitor(converter, bind);
			}
		}

		void Statement::setParameter(std::size_t index, const Bind &argument)
		{
			if (index >= m_parameters.size())
			{
				throw StatementException("Invalid argument index");
			}

			m_parameters[index] = argument;
		}

		void Statement::setString(std::size_t index, const std::string &value)
		{
			setParameter(index, value);
		}

		void Statement::setInt(std::size_t index, Int64 value)
		{
			setParameter(index, value);
		}

		void Statement::setDouble(std::size_t index, double value)
		{
			setParameter(index, value);
		}

		void Statement::execute()
		{
			assert(m_parameters.size() == getParameterCount());

			if (!m_parameters.empty())
			{
				m_convertedParameters.resize(m_parameters.size());
				for (std::size_t i = 0; i < m_convertedParameters.size(); ++i)
				{
					auto &maybeParameter = m_parameters[i];
					if (!maybeParameter)
					{
						throw StatementException(
							fmt::format("All parameters must be set before execution ({0} is not set)", i));
					}

					m_convertedParameters[i] = convertBind(*maybeParameter);
				}

				const int rc = mysql_stmt_bind_param(m_handle,
				                                     m_convertedParameters.data());
				checkResultCode(*m_handle, rc);
			}

			{
				const int rc = mysql_stmt_execute(m_handle);
				checkResultCode(*m_handle, rc);
			}

			for (auto &parameter : m_parameters)
			{
				parameter.reset();
			}
		}

		StatementResult Statement::executeSelect()
		{
			execute();
			//TODO make sure that there are results
			return StatementResult(*m_handle);
		}

		void Statement::allocateHandle(MYSQL &mysql)
		{
			m_handle = mysql_stmt_init(&mysql);
			if (!m_handle)
			{
				throw StatementException("Could not allocate MySQL statement");
			}
		}


		StatementResult::StatementResult()
			: m_statement(nullptr)
		{
		}

		StatementResult::StatementResult(StatementResult &&other)
			: m_statement(other.m_statement)
		{
			other.m_statement = nullptr;
		}

		StatementResult::StatementResult(MYSQL_STMT &statementWithResults)
			: m_statement(&statementWithResults)
		{
		}

		StatementResult::~StatementResult()
		{
			if (m_statement)
			{
				//ignore result for now
				mysql_stmt_free_result(m_statement);
			}
		}

		StatementResult &StatementResult::operator = (StatementResult &&other)
		{
			if (this != &other)
			{
				m_statement = other.m_statement;
				other.m_statement = nullptr;
			}
			return *this;
		}

		void StatementResult::swap(StatementResult &other)
		{
			std::swap(m_statement, other.m_statement);
		}

		void StatementResult::storeResult()
		{
			const int rc = mysql_stmt_store_result(m_statement);
			checkResultCode(*m_statement, rc);
		}

		size_t StatementResult::getAffectedRowCount() const
		{
			return static_cast<size_t>(
			           mysql_stmt_affected_rows(m_statement));
		}

		bool StatementResult::fetchResultRow()
		{
			const int rc = mysql_stmt_fetch(m_statement);
			switch (rc)
			{
			case 0: //there is a row
			case MYSQL_DATA_TRUNCATED: //TODO handle truncation
				return true;

			case MYSQL_NO_DATA: //no more rows
				return false;

			default:
				throwError(*m_statement);
				//throwError always throws, but the compiler does not know that
				return false;
			}
		}

		std::string StatementResult::getString(std::size_t index,
		                                       std::size_t maxLengthInBytes)
		{
			unsigned long realLength = 0;

			//We do not know the length of the string beforehand, so we
			//have to ask MySQL for it. If we are lucky, the string is short
			//and we are mostly done.
			//If the string is longer than our small buffer on the stack,
			//we have to allocate enough memory and retrieve the whole string
			//from MySQL.

			{
				//Attention: Micro optimization ahead!
				//We do not want to waste a dynamic allocation for a size
				//misprediction, so we use a local buffer for the first try.
				//std::string will allocate anyway, so there is no reason to
				//have a large buffer on the stack.
				//We keep the buffer in the order of one or two cache lines
				//to minimize unnecessary copying and cache thrashing.
				static const size_t StaticBufferSize = 128;

				my_bool isNull = 0;
				my_bool isTruncated = 0;
				std::array<char, StaticBufferSize> staticResult;
				MYSQL_BIND bind = {};
				bind.buffer_type = MYSQL_TYPE_STRING;
				bind.buffer = staticResult.data();
				bind.buffer_length = staticResult.size();
				bind.is_null = &isNull;
				bind.length = &realLength;
				bind.error = &isTruncated;
				const int rc = mysql_stmt_fetch_column(
				                   m_statement, &bind, static_cast<unsigned>(index), 0);
				checkResultCode(*m_statement, rc);
				if (!isTruncated)
				{
					if (realLength > staticResult.size())
					{
						throw StatementException(
						    "MySQL C API returned an invalid string length");
					}
					return std::string(staticResult.data(),
					                   static_cast<std::size_t>(realLength));
				}

				//TODO: handle isNull
			}

			if (realLength > maxLengthInBytes)
			{
				throw StatementException(
					fmt::format("Maximum string length exceeded ({0} > {1})", realLength, maxLengthInBytes));
			}

			std::string result;
			//Here comes the only allocation in this function besides error
			//handling.
			result.resize(static_cast<std::size_t>(realLength));

			my_bool isNull = 0;
			my_bool isTruncated = 0;
			MYSQL_BIND bind = {};
			bind.buffer_type = MYSQL_TYPE_STRING;
			assert(!result.empty());
			bind.buffer = &result[0];
			bind.buffer_length = result.size();
			bind.is_null = &isNull;
			bind.length = &realLength;
			bind.error = &isTruncated;
			const int rc = mysql_stmt_fetch_column(
			                   m_statement, &bind, static_cast<unsigned>(index), 0);
			checkResultCode(*m_statement, rc);

			if (isTruncated)
			{
				throw StatementException(
				    "MySQL C API reported an unexpected truncation");
			}

			if (realLength != result.size())
			{
				throw StatementException(
				    "MySQL C API returned an unexpected string length");
			}

			//TODO: handle isNull

			//Move the string out by returning it by value (C++11 feature).
			return result;
		}

		Int64 StatementResult::getInt(std::size_t index)
		{
			Int64 result = 0;
			my_bool isNull = 0;
			MYSQL_BIND bind = {};
			bind.buffer_type = MYSQL_TYPE_LONGLONG;
			bind.buffer = &result;
			bind.is_null = &isNull;
			const int rc = mysql_stmt_fetch_column(
			                   m_statement, &bind, static_cast<unsigned>(index), 0);
			checkResultCode(*m_statement, rc);
			if (isNull)
			{
				throw StatementException("Unexpected NULL integer result");
			}
			return result;
		}

		double StatementResult::getDouble(std::size_t index)
		{
			double result = 0;
			my_bool isNull = 0;
			MYSQL_BIND bind = {};
			bind.buffer_type = MYSQL_TYPE_DOUBLE;
			bind.buffer = &result;
			bind.is_null = &isNull;
			const int rc = mysql_stmt_fetch_column(
			                   m_statement, &bind, static_cast<unsigned>(index), 0);
			checkResultCode(*m_statement, rc);
			if (isNull)
			{
				throw StatementException("Unexpected NULL double result");
			}
			return result;
		}

		bool StatementResult::getBoolean(std::size_t index)
		{
			my_bool result = 0;
			my_bool isNull = 0;
			MYSQL_BIND bind = {};
			bind.buffer_type = MYSQL_TYPE_BIT;
			bind.buffer = &result;
			bind.is_null = &isNull;
			const int rc = mysql_stmt_fetch_column(
			                   m_statement, &bind, static_cast<unsigned>(index), 0);
			checkResultCode(*m_statement, rc);
			if (isNull)
			{
				throw StatementException("Unexpected NULL bool result");
			}
			return result != 0;
		}
	}
}
