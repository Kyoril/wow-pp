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

#include <exception>
#include "sff_read_token.h"
#include "sff_datatypes.h"

namespace sff
{
	struct Exception : std::exception
	{
		virtual const char *what() const throw() = 0;
	};


	struct InvalidEncodingException : Exception
	{
		explicit InvalidEncodingException(FileEncoding expected)
			: m_message("Expected file encoding " + getFileEncodingName(expected))
		{
		}

		virtual ~InvalidEncodingException() throw()
		{
		}

		virtual const char *what() const throw()
		{
			return m_message.c_str();
		}

	private:

		std::string m_message;
	};

	namespace read
	{
		struct EndOfRangeException : Exception
		{
			virtual const char *what() const throw()
			{
				return "Unexpected end of the input range";
			}
		};


		template <class T>
		struct ParseException : Exception
		{
			typedef Token<T> MyToken;


			MyToken position;


			ParseException(MyToken position)
				: position(position)
			{
			}
		};


		template <class T>
		struct SyntacticException : ParseException<T>
		{
			typedef ParseException<T> Super;


			explicit SyntacticException(typename Super::MyToken token)
				: Super(token)
			{
			}

			virtual const char *what() const throw()
			{
				return "There is an error in the syntax of this file";
			}
		};


		template <class T>
		struct InvalidEscapeSequenceException : SyntacticException<T>
		{
			typedef SyntacticException<T> Super;


			InvalidEscapeSequenceException(typename Super::MyToken token)
				: Super(token)
			{
			}

			virtual const char *what() const throw()
			{
				return "Found an invalid escape sequence";
			}
		};


		template <class T>
		struct UnexpectedTokenException : SyntacticException<T>
		{
			typedef SyntacticException<T> Super;


			const char *message;


			UnexpectedTokenException(typename Super::MyToken token, const char *message)
				: Super(token)
				, message(message)
			{
			}

			virtual const char *what() const throw()
			{
				return message;
			}
		};


		template <class T>
		struct ObjectExpectedException : SyntacticException<T>
		{
			typedef SyntacticException<T> Super;


			ObjectExpectedException(typename Super::MyToken token)
				: Super(token)
			{
			}

			virtual const char *what() const throw()
			{
				return "Object expected";
			}
		};


		template <class T>
		struct SemanticException : ParseException<T>
		{
			typedef ParseException<T> Super;


			SemanticException(typename Super::MyToken token)
				: Super(token)
			{
			}

			virtual const char *what() const throw()
			{
				return "Something different was expected here";
			}
		};


		template <class T>
		struct DataTypeException : SemanticException<T>
		{
			typedef SemanticException<T> Super;


			DataTypeException(typename Super::MyToken token)
				: Super(token)
			{
			}

			virtual const char *what() const throw()
			{
				return "Other type expected";
			}
		};


		template <class T>
		struct NegativeUnsignedException : DataTypeException<T>
		{
			typedef DataTypeException<T> Super;


			NegativeUnsignedException(typename Super::MyToken token)
				: Super(token)
			{
			}

			virtual const char *what() const throw()
			{
				return "Unsigned integers cannot be negative";
			}
		};


		template <class T, DataType Expected>
		struct TypeExpectedException : DataTypeException<T>
		{
			typedef DataTypeException<T> Super;


			TypeExpectedException(typename Super::MyToken token)
				: Super(token)
			{
			}

			virtual const char *what() const throw()
			{
				static const std::array<const char *, CountOfTypes> Messages =
				{{
						"Integer expected",
						"String expected",
						"Array expected",
						"Table expected",
					}
				};

				return Messages[Expected];
			}
		};
	}
}
