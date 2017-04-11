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
	namespace SQL
	{
		class Statement;


		struct DatabaseEditor
		{
			virtual ~DatabaseEditor();
			virtual void create(const std::string &name) = 0;
			virtual void drop(const std::string &name) = 0;
			virtual void use(const std::string &name) = 0;
		};

		/// The Connection interface can be implemented for different
		/// SQL connectors for providing flexibility.
		class Connection
		{
		public:

			virtual ~Connection();

			/// execute tries to execute a textually given SQL query.
			/// @param query The begin of the SQL string.
			/// @param size The length of the SQL string from the begin.
			/// @throws if the query fails. The exception is derived from std::runtime_error.
			virtual void execute(const char *query, std::size_t size) = 0;

			/// createStatement compiles an SQL query into a reusable
			/// statement.
			/// @param query The begin of the SQL string.
			/// @param size The length of the SQL string from the begin.
			/// @return the compiled statement which can be used from now. Never nullptr.
			/// @throws if the compilation fails. The exception is derived from std::runtime_error.
			virtual std::unique_ptr<Statement> createStatement(
			    const char *query,
			    std::size_t size) = 0;

			/// beginTransaction starts a transaction. A transaction
			/// combines an arbitrary number of queries into an atomic unit.
			/// Either all of the queries in the transaction will be executed or
			/// none. Other observers will never see a state of the database in
			/// between. This Connection is the only one which does.
			virtual void beginTransaction() = 0;

			/// commit ends a transaction and guarantees that all of the
			/// queries in the transaction have been executed successfully on
			/// the database.
			/// @throws if the transaction could not be committed. Reasons for
			/// this can be specific to the DBMS you are actually using. A
			/// possible reason for the failure of a commit is a constraint
			/// violation.
			virtual void commit() = 0;

			/// rollback stops a transaction and reverts the changes of
			/// the queries executed so far in this transaction. In other words,
			/// this transaction will have had no effect on the state of
			/// the database for observers.
			virtual void rollback() = 0;

			virtual bool isTableExisting(const std::string &tableName);
			virtual const std::string &getAutoIncrementSyntax() const = 0;
			virtual DatabaseEditor *getDatabaseEditor() = 0;

			/// execute is a utility which forwards the query to
			/// @link Connection::execute(const char *, std::size_t)}.
			/// @param query the query to be executed.
			void execute(const std::string &query);

			std::unique_ptr<Statement> createStatement(const std::string &query);
		};


		/// The Null struct represents an SQL NULL.
		struct Null {};


		/// Primitive contains a simple SQL value type. Possible values
		/// are NULL, integers up to 64 bits, floats up to 64 bit or strings
		/// of 8-bit bytes.
		typedef boost::variant<Null, Int64, double, std::string> Primitive;


		class ResultReader
		{
		public:

			virtual ~ResultReader();
			virtual bool isRow() const = 0;
			virtual void nextRow() = 0;
			virtual Primitive getPrimitive(std::size_t column) const = 0;

			virtual Int64 getInt(std::size_t column) const;
			virtual double getFloat(std::size_t column) const;
			virtual std::string getString(std::size_t column) const;
		};


		class Statement
		{
		public:

			virtual ~Statement();

			virtual std::size_t getParameterCount() const = 0;
			virtual void clearParameters() = 0;
			virtual void execute() = 0;
			virtual ResultReader &select() = 0;
			virtual void freeResult() = 0;
			virtual void setParameter(std::size_t column, const Primitive &parameter) = 0;

			virtual void setNull(std::size_t column);
			virtual void setInt(std::size_t column, Int64 value);
			virtual void setFloat(std::size_t column, double value);
			virtual void setString(std::size_t column,
			                       const char *begin,
			                       std::size_t size);

			void setString(std::size_t column, const std::string &value);
		};


		void unpackParameter(const Primitive &parameter,
		                     std::size_t column,
		                     Statement &statement);

		template <class Int>
		Int castInt(Int64 fullValue)
		{
			const Int castValue = static_cast<Int>(fullValue);

			//this typedef is outside of the lambda because of strange compiler behaviour
			//(GCC 4.7 wants the typename, VS11 rejects it)
			typedef typename std::make_signed<Int>::type SignedInt;
			const Int64 checkedValue = [ = ]() -> Int64
			{
				if (std::is_unsigned<Int>::value && (fullValue < 0))
				{
					return static_cast<SignedInt>(castValue);
				}
				return static_cast<Int64>(castValue);
			}();
			if (checkedValue != fullValue)
			{
				throw std::runtime_error("SQL result integer cannot be represented in the requested type");
			}
			return castValue;
		}

		class Row final
		{
		public:

			Row();
			explicit Row(ResultReader &readerWithRow);
			Int64 getInt(std::size_t column) const;
			double getFloat(std::size_t column) const;
			std::string getString(std::size_t column) const;

			template <class Int>
			Int getInt(std::size_t column) const
			{
				const Int64 fullValue = getInt(column);
				return castInt<Int>(fullValue);
			}

			template <class Int>
			void getIntInto(std::size_t column, Int &out_value) const
			{
				out_value = getInt<Int>(column);
			}

		private:

			ResultReader *m_reader;
		};


		class Result final
		{
		public:

			explicit Result(Statement &statement);
			~Result();
			Row *getCurrentRow();
			void nextRow();

		private:

			Statement &m_statement;
			ResultReader &m_reader;
			Row m_currentRow;
		};


		/// The Transaction struct begins a transaction on construction
		/// and guarantees to either commit or rollback the transaction before
		/// it is destroyed.
		class Transaction final
		{
		private:

			Transaction(const Transaction &Other) = delete;
			Transaction &operator=(const Transaction &Other) = delete;

		public:

			/// Transaction Begins a transaction.
			/// @param connection The connection to begin the transaction on.
			explicit Transaction(Connection &connection);

			/// Rolls back the transaction if not already committed.
			~Transaction();

			/// commit Commits the transaction. Undefined behaviour if
			/// called more than once on the same Transaction object.
			void commit();

		private:

			Connection &m_connection;
			bool m_isCommit;
		};


		/// The Owner template extends a Statement or connection wrapper with
		/// automatic resource clean-up.
		/// @param Statement The statement wrapper to extend. Has to provide a
		/// default constructor, a Handle constructor, a swap method and a method
		/// getHandle which returns a pointer which can be managed with Handle.
		/// @param Handle A unique_ptr-like movable type which manages the
		/// lifetime of the internal statement resource.
		template <class Base, class Handle>
		class Owner
			: public Base
		{
		private:

			Owner<Base, Handle>(const Owner<Base, Handle> &Other) = delete;
			Owner<Base, Handle> &operator=(const Owner<Base, Handle> &Other) = delete;

		public:

			Owner()
			{
			}

			Owner(Handle handle)
				: Base(*handle)
			{
				handle.release();
			}

			Owner(Owner &&other)
			{
				this->swap(other);
				Owner().swap(other);
			}

			Owner &operator = (Owner &&other)
			{
				if (this != &other)
				{
					Owner(std::move(other)).swap(*this);
				}
				return *this;
			}

			virtual ~Owner()
			{
				(Handle(this->getHandle()));
			}
		};
	}
}
