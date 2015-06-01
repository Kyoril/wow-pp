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
//
// World of Warcraft, and all World of Warcraft or Warcraft art, images,
// and lore are copyrighted by Blizzard Entertainment, Inc.
// 

#pragma once

#include "source.h"
#include <algorithm>
#include <vector>
#include <cassert>
#include <cstring>

namespace io
{
	class MemorySource : public ISource
	{
	public:

		MemorySource()
			: m_begin(nullptr)
			, m_end(nullptr)
			, m_pos(nullptr)
		{
		}

		MemorySource(const char *begin, const char *end)
			: m_begin(begin)
			, m_end(end)
			, m_pos(begin)
		{
			assert(begin <= end);
		}

		explicit MemorySource(const std::string &buffer)
			: m_begin(&buffer[0])
		{
			m_pos = m_begin;
			m_end = m_begin + buffer.size();
		}

		explicit MemorySource(const std::vector<char> &buffer)
			: m_begin(&buffer[0])
		{
			m_pos = m_begin;
			m_end = m_begin + buffer.size();
		}

		void rewind()
		{
			m_pos = m_begin;
		}

		std::size_t getSize() const
		{
			return static_cast<std::size_t>(m_end - m_begin);
		}

		std::size_t getRest() const
		{
			return static_cast<std::size_t>(m_end - m_pos);
		}

		std::size_t getRead() const
		{
			return static_cast<std::size_t>(m_pos - m_begin);
		}

		const char *getBegin() const
		{
			return m_begin;
		}

		const char *getEnd() const
		{
			return m_end;
		}

		const char *getPosition() const
		{
			return m_pos;
		}

		virtual bool end() const override
		{
			return (m_pos == m_end);
		}

		virtual std::size_t read(char *dest, std::size_t size) override
		{
			const std::size_t rest = getRest();
			const std::size_t copyable = std::min<std::size_t>(rest, size);
			std::memcpy(dest, m_pos, copyable);
			m_pos += copyable;
			return copyable;
		}

		virtual std::size_t skip(std::size_t size) override
		{
			const std::size_t rest = getRest();
			const std::size_t skippable = std::min<std::size_t>(rest, size);
			m_pos += skippable;
			return skippable;
		}

		virtual std::size_t size() const override
		{
			return static_cast<std::size_t>(m_end - m_begin);
		}

		virtual std::size_t position() const override
		{
			return static_cast<size_t>(m_pos - m_begin);
		}

	private:

		const char *m_begin, *m_end, *m_pos;
	};
}
