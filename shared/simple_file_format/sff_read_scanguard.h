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

#include "sff_read_token.h"
#include "sff_read_scanner.h"

namespace sff
{
	namespace read
	{
		template <class T>
		struct ScanGuard
		{
			typedef Scanner<T> MyScanner;
			typedef Token<T> MyToken;


			MyScanner &scanner;


			explicit ScanGuard(MyScanner &scanner)
				: scanner(scanner)
				, m_index(scanner.getTokenIndexReference())
				, m_offset(0)
				, m_approved(false)
			{
			}

			~ScanGuard()
			{
				if (!m_approved)
				{
					m_index -= m_offset;
				}
			}

			void back()
			{
				--m_index;
				--m_offset;
			}

			MyToken next()
			{
				const MyToken temp = scanner.getToken(m_index);
				++m_index;
				++m_offset;
				return temp;
			}

			void approve()
			{
				m_approved = true;
			}

		private:

			std::size_t &m_index;
			std::size_t m_offset;
			bool m_approved;
		};
	}
}
