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

namespace sff
{
	template <class T>
	class CharTraits
	{
	};

	template <>
	class CharTraits<char>
	{
	public:

		static const char LeftParenthesis = '(';
		static const char RightParenthesis = ')';
		static const char LeftBrace = '{';
		static const char RightBrace = '}';
		static const char LeftBracket = '[';
		static const char RightBracket = ']';
		static const char Assign = '=';
		static const char Comma = ',';
		static const char Dot = '.';
		static const char Plus = '+';
		static const char Minus = '-';
		static const char Underscore = '_';
		static const char Quotes = '"';
		static const char Slash = '/';
		static const char BackSlash = '\\';
		static const char EndOfLine = '\n';
		static const char Star = '*';

		static inline bool isAlpha(char c)
		{
			return
			    (c >= 'a' && c <= 'z') ||
			    (c >= 'A' && c <= 'Z');
		}

		static inline bool isDigit(char c)
		{
			return
			    (c >= '0') && (c <= '9');
		}

		static inline bool isIdentifierBegin(char c)
		{
			return isAlpha(c) || (c == Underscore);
		}

		static inline bool isIdentifierMiddle(char c)
		{
			return isAlpha(c) || (c == Underscore) || isDigit(c);
		}

		static inline bool isWhitespace(char c)
		{
			auto avoidAlwaysTrueWarning = static_cast<int>(c);
			return (avoidAlwaysTrueWarning >= '\0') && (c <= ' ');
		}
	};
}
