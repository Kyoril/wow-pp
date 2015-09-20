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

#include <string>
#include <boost/mpl/if.hpp>
#include <boost/lexical_cast.hpp>
#include <cassert>

namespace sff
{
	namespace read
	{
		struct token_type
		{
			enum Enum
			{
			    Unknown,
			    LeftParenthesis,
			    RightParenthesis,
			    LeftBrace,
			    RightBrace,
			    LeftBracket,
			    RightBracket,
			    Assign,
			    Comma,
			    Plus,
			    Minus,
			    Identifier,
			    Decimal,
			    String
			};
		};

		typedef token_type::Enum TokenType;


		template <class T>
		struct Token
		{
			typedef typename std::iterator_traits<T>::value_type Char;
			typedef typename std::basic_string<Char> String;


			TokenType type;
			T begin, end;


			Token()
				: type(token_type::Unknown)
			{
			}

			explicit Token(TokenType type, T begin, T end)
				: type(type)
				, begin(begin)
				, end(end)
			{
			}

			bool isEndOfFile() const
			{
				return
				    (type == token_type::Unknown) &&
				    (begin == end);
			}

			String str() const
			{
				assert(type != token_type::Unknown);
				return String(begin, end);
			}

			std::size_t size() const
			{
				return std::distance(begin, end);
			}

			template <class TValue>
			TValue toInteger() const
			{
				return toIntegerImpl<TValue>(std::integral_constant<bool, std::is_integral<TValue>::value && (sizeof(TValue) == 1) && !std::is_same<bool, TValue>::value>());
			}

		private:

			template <class C>
			struct enlarge_char_to_int : boost::mpl::if_<std::is_signed<C>, int, unsigned>
			{
			};

			template <class TValue>
			TValue toIntegerImpl(std::integral_constant<bool, true> isChar) const
			{
				return static_cast<TValue>(boost::lexical_cast<typename enlarge_char_to_int<TValue>::type>(str()));
			}

			template <class TValue>
			TValue toIntegerImpl(std::integral_constant<bool, false> isChar) const
			{
				return boost::lexical_cast<TValue>(str());
			}
		};
	}
}
