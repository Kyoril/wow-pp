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

#include <algorithm>
#include <vector>
#include "sff_read_token.h"
#include "sff_chartraits.h"

namespace sff
{
	namespace read
	{
		template <class T>
		class Scanner
		{
		public:

			typedef Token<T> MyToken;
			typedef typename MyToken::Char Char;
			typedef CharTraits<Char> MyCharTraits;

		public:

			explicit Scanner(T begin, T end)
				: m_begin(begin)
				, m_end(end)
				, m_pos(begin)
				, m_index(0)
			{
			}

			const MyToken &getToken(std::size_t index)
			{
				while (index >= m_tokens.size())
				{
					if (!parseToken())
					{
						addToken(MyToken(token_type::Unknown, m_pos, std::find(m_pos, m_end, +MyCharTraits::EndOfLine)));
						return m_tokens.back();
					}
				}

				return m_tokens[index];
			}

			std::size_t &getTokenIndexReference()
			{
				return m_index;
			}

			std::size_t getTokenIndex() const
			{
				return m_index;
			}

			const MyToken &getCurrentToken()
			{
				return getToken(getTokenIndex());
			}

			std::size_t getTokenLine(const MyToken &token) const
			{
				return getCharacterLine(token.begin, m_begin);
			}

			static std::size_t getCharacterLine(T position, T begin)
			{
				return std::count(begin, position, +MyCharTraits::EndOfLine);
			}

		private:

			typedef std::vector<MyToken> Tokens;


			T m_begin, m_end, m_pos;
			Tokens m_tokens;
			std::size_t m_index;


			void addToken(const MyToken &token)
			{
				m_tokens.push_back(token);
			}

			bool parseToken()
			{
				skipWhitespace();

				if (m_pos == m_end)
				{
					return false;
				}

				const Char first = *m_pos;

				switch (first)
				{
				case MyCharTraits::LeftParenthesis:
					{
						addToken(MyToken(token_type::LeftParenthesis, m_pos, m_pos + 1));
						++m_pos;
						break;
					}

				case MyCharTraits::RightParenthesis:
					{
						addToken(MyToken(token_type::RightParenthesis, m_pos, m_pos + 1));
						++m_pos;
						break;
					}

				case MyCharTraits::LeftBrace:
					{
						addToken(MyToken(token_type::LeftBrace, m_pos, m_pos + 1));
						++m_pos;
						break;
					}

				case MyCharTraits::RightBrace:
					{
						addToken(MyToken(token_type::RightBrace, m_pos, m_pos + 1));
						++m_pos;
						break;
					}

				case MyCharTraits::LeftBracket:
					{
						addToken(MyToken(token_type::LeftBracket, m_pos, m_pos + 1));
						++m_pos;
						break;
					}

				case MyCharTraits::RightBracket:
					{
						addToken(MyToken(token_type::RightBracket, m_pos, m_pos + 1));
						++m_pos;
						break;
					}

				case MyCharTraits::Assign:
					{
						addToken(MyToken(token_type::Assign, m_pos, m_pos + 1));
						++m_pos;
						break;
					}

				case MyCharTraits::Comma:
					{
						addToken(MyToken(token_type::Comma, m_pos, m_pos + 1));
						++m_pos;
						break;
					}

				case MyCharTraits::Plus:
					{
						addToken(MyToken(token_type::Plus, m_pos, m_pos + 1));
						++m_pos;
						break;
					}

				case MyCharTraits::Minus:
					{
						addToken(MyToken(token_type::Minus, m_pos, m_pos + 1));
						++m_pos;
						break;
					}

				case MyCharTraits::Slash:
					{
						++m_pos;
						if (m_pos == m_end)
						{
							return false;
						}

						//single line comment "//"
						if (*m_pos == MyCharTraits::Slash)
						{
							for (;;)
							{
								++m_pos;

								if (m_pos == m_end)
								{
									return false;
								}

								if (*m_pos == MyCharTraits::EndOfLine)
								{
									++m_pos;
									return true;
								}
							}
						}

						//multi line comment "/* */"
						else if (*m_pos == MyCharTraits::Star)
						{
							for (;;)
							{
								++m_pos;

								if (m_pos == m_end)
								{
									return false;
								}

								if (*m_pos == MyCharTraits::Star)
								{
									++m_pos;

									if (m_pos == m_end)
									{
										return false;
									}

									if (*m_pos == MyCharTraits::Slash)
									{
										++m_pos;
										return true;
									}
								}
							}
						}
						else
						{
							return false;
						}
					}

				default:
					{
						if (MyCharTraits::isIdentifierBegin(first))
						{
							const T identifierBegin = m_pos;

							do
							{
								++m_pos;
							}
							while ((m_pos != m_end) &&
							        MyCharTraits::isIdentifierMiddle(*m_pos));

							const T identifierEnd = m_pos;

							addToken(MyToken(token_type::Identifier, identifierBegin, identifierEnd));
							return true;
						}

						else if (MyCharTraits::isDigit(first))
						{
							const T numberBegin = m_pos;
							bool hasDot = false;

							do
							{
								++m_pos;

								if ((m_pos != m_end) &&
								        (MyCharTraits::Dot == *m_pos))
								{
									if (hasDot)
									{
										break;
									}

									hasDot = true;
									++m_pos;
								}
							}
							while ((m_pos != m_end) &&
							        MyCharTraits::isDigit(*m_pos));

							const T numberEnd = m_pos;

							addToken(MyToken(token_type::Decimal, numberBegin, numberEnd));
							return true;
						}

						else if (first == MyCharTraits::Quotes)
						{
							++m_pos;
							const T stringBegin = m_pos;
							T stringEnd = m_end;

							while (m_pos != m_end)
							{
								const Char c = *m_pos;

								if (c == MyCharTraits::BackSlash)
								{
									++m_pos;
									if (m_pos == m_end)
									{
										return false;
									}
								}

								else if (c == MyCharTraits::Quotes)
								{
									stringEnd = m_pos;
									++m_pos;
									break;
								}

								++m_pos;
							}

							addToken(MyToken(token_type::String, stringBegin, stringEnd));
							return true;
						}

						return false;
					}
				}

				return true;
			}

			void skipWhitespace()
			{
				while (
				    (m_pos != m_end) &&
				    MyCharTraits::isWhitespace(*m_pos))
				{
					++m_pos;
				}
			}
		};
	}
}
