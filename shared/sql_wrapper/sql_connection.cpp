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

#include "sql_connection.h"
#include <cassert>

namespace wowpp
{
	namespace SQL
	{
		DatabaseEditor::~DatabaseEditor()
		{
		}


		Connection::~Connection()
		{
		}

		bool Connection::isTableExisting(const std::string &tableName)
		{
			const auto select = createStatement(
			            "SELECT EXISTS(SELECT table_name FROM INFORMATION_SCHEMA.TABLES "
			            "WHERE table_name LIKE ? AND table_schema=DATABASE())");
			select->setString(0, tableName);
			SQL::Result results(*select);
			SQL::Row * const row = results.getCurrentRow();
			if (!row)
			{
				throw("Result row expected");
			}
			const bool doesExist = (row->getInt(0) != 0);
			return doesExist;
		}

		void Connection::execute(const std::string &query)
		{
			return execute(query.data(), query.size());
		}

		std::unique_ptr<Statement> Connection::createStatement(
		        const std::string &query)
		{
			return createStatement(query.data(), query.size());
		}


		ResultReader::~ResultReader()
		{
		}

		namespace
		{
			template <class Number>
			struct PrimitiveToNumberConverter : boost::static_visitor<Number>
			{
				Number operator ()(Null) const
				{
					return 0;
				}

				Number operator ()(Int64 i) const
				{
					return static_cast<Number>(i);
				}

				Number operator ()(double i) const
				{
					return static_cast<Number>(i);
				}

				Number operator ()(const std::string &) const
				{
					return 0; //TODO
				}
			};

			struct PrimitiveToStringConverter : boost::static_visitor<std::string>
			{
				std::string operator ()(Null) const
				{
					return std::string();
				}

				std::string operator ()(Int64) const
				{
					return std::string();
				}

				std::string operator ()(double) const
				{
					return std::string();
				}

				std::string operator ()(std::string &str) const
				{
					return std::move(str);
				}
			};

			template <class Result, class Converter>
			Result convertColumn(const ResultReader &reader, std::size_t column)
			{
				Converter converter;
				auto value = reader.getPrimitive(column);
				return boost::apply_visitor(converter, value);
			}
		}

		Int64 ResultReader::getInt(std::size_t column) const
		{
			return convertColumn<Int64, PrimitiveToNumberConverter<Int64>>(*this, column);
		}

		double ResultReader::getFloat(std::size_t column) const
		{
			return convertColumn<double, PrimitiveToNumberConverter<double>>(*this, column);
		}

		std::string ResultReader::getString(std::size_t column) const
		{
			return convertColumn<std::string, PrimitiveToStringConverter>(*this, column);
		}


		Statement::~Statement()
		{
		}

		void Statement::setNull(std::size_t column)
		{
			setParameter(column, Null());
		}

		void Statement::setInt(std::size_t column, Int64 value)
		{
			setParameter(column, value);
		}

		void Statement::setFloat(std::size_t column, double value)
		{
			setParameter(column, value);
		}

		void Statement::setString(std::size_t column,
		                          const char *begin,
		                          std::size_t size)
		{
			setParameter(column, std::string(begin, size));
		}

		void Statement::setString(std::size_t column, const std::string &value)
		{
			return setString(column, value.data(), value.size());
		}


		namespace
		{
			class ParameterUnpacker : public boost::static_visitor<>
			{
			public:

				explicit ParameterUnpacker(Statement &statement,
				                           std::size_t column)
				    : m_statement(statement)
				    , m_column(column)
				{
				}

				void operator ()(SQL::Null) const
				{
					m_statement.setNull(m_column);
				}

				void operator ()(Int64 value) const
				{
					m_statement.setInt(m_column, value);
				}

				void operator ()(double value) const
				{
					m_statement.setFloat(m_column, value);
				}

				void operator ()(const std::string &value) const
				{
					m_statement.setString(m_column, value);
				}

			private:

				Statement &m_statement;
				const size_t m_column;
			};
		}

		void unpackParameter(const Primitive &parameter,
		                     std::size_t column,
		                     Statement &statement)
		{
			ParameterUnpacker converter(statement, column);
			return boost::apply_visitor(converter, parameter);
		}


		Row::Row()
		    : m_reader(nullptr)
		{
		}

		Row::Row(ResultReader &readerWithRow)
		    : m_reader(&readerWithRow)
		{
			assert(m_reader->isRow());
		}

		Int64 Row::getInt(std::size_t column) const
		{
			return m_reader->getInt(column);
		}

		double Row::getFloat(std::size_t column) const
		{
			return m_reader->getFloat(column);
		}

		std::string Row::getString(std::size_t column) const
		{
			return m_reader->getString(column);
		}


		Result::Result(Statement &statement)
		    : m_statement(statement)
		    , m_reader(statement.select())
		{
		}

		Result::~Result()
		{
			m_statement.freeResult();
		}

		Row *Result::getCurrentRow()
		{
			if (m_reader.isRow())
			{
				m_currentRow = Row(m_reader);
				return &m_currentRow;
			}
			return nullptr;
		}

		void Result::nextRow()
		{
			assert(m_reader.isRow());
			m_reader.nextRow();
		}


		Transaction::Transaction(Connection &connection)
		    : m_connection(connection)
		    , m_isCommit(false)
		{
			m_connection.beginTransaction();
		}

		Transaction::~Transaction()
		{
			if (!m_isCommit)
			{
				m_connection.rollback();
			}
		}

		void Transaction::commit()
		{
			//TODO forbid double commit with ASSERT?
			m_connection.commit();
			m_isCommit = true;
		}
	}
}
