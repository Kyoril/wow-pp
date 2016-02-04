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
//
// World of Warcraft, and all World of Warcraft or Warcraft art, images,
// and lore are copyrighted by Blizzard Entertainment, Inc.
// 

#pragma once

#include "sink.h"
#include <cassert>
#include <cstring>

namespace io
{
	class StringSink : public ISink
	{
	public:

		typedef std::string String;


		explicit StringSink(String &buffer)
			: m_buffer(buffer)
		{
		}

		String &buffer()
		{
			return m_buffer;
		}

		const String &buffer() const
		{
			return m_buffer;
		}

		virtual std::size_t write(const char *src, std::size_t size) override
		{
			m_buffer.append(src, size);
			return size;
		}

		virtual std::size_t overwrite(std::size_t position, const char *src, std::size_t size) override
		{
			assert((position + size) <= m_buffer.size());

			char *const dest = &m_buffer[0] + position;
			std::memcpy(dest, src, size);
			return size;
		}

		virtual std::size_t position() override
		{
			return m_buffer.size();
		}

		virtual void flush() override
		{
		}

	private:

		String &m_buffer;
	};
}
