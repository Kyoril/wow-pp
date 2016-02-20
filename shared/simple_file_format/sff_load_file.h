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
#include <istream>
#include "sff_read_tree.h"

namespace sff
{
	namespace detail
	{
		template <class I, class L>
		bool skipLiteral(I &pos, I end, L literal, L literal_end)
		{
			while (literal != literal_end)
			{
				if ((pos == end) ||
				        (*pos != *literal))
				{
					return false;
				}

				++pos;
				++literal;
			}

			return true;
		}

		template <class I>
		bool skipUTF8BOM(I &pos, I end)
		{
			static const std::array<char, 3> UTF8BOM =
			{{
					'\xEF', '\xBB', '\xBF',
				}
			};

			return skipLiteral(pos, end, UTF8BOM.begin(), UTF8BOM.end());
		}
	}

	template <class C, class I>
	void loadTableFromMemory(
	    sff::read::tree::Table<I> &fileTable,
	    const C &content,
	    FileEncoding encoding = UTF8_guess)
	{
		auto begin = content.begin();
		auto end = content.end();

		switch (encoding)
		{
		case UTF8_BOM:
		case UTF8_guess:
			{
				auto pos = begin;

				if (detail::skipUTF8BOM(pos, end))
				{
					begin = pos;
				}
				else if (encoding == UTF8_BOM)
				{
					throw InvalidEncodingException(UTF8_BOM);
				}
				break;
			}

		case UTF8:
			break;
		}

		sff::read::Parser<I> parser(
		    (begin), end);

		fileTable.parseFile(parser);
	}

	inline void loadFileIntoMemory(
	    std::istream &source,
	    std::string &content)
	{
		content.assign(std::istreambuf_iterator<char>(source),
		               std::istreambuf_iterator<char>());
	}

	template <class I>
	void loadTableFromFile(
	    sff::read::tree::Table<I> &fileTable,
	    std::string &content,
	    std::istream &source,
	    FileEncoding encoding = UTF8_guess)
	{
		loadFileIntoMemory(source, content);
		loadTableFromMemory(fileTable, content, encoding);
	}
}
