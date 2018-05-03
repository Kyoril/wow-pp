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

namespace io
{
	class StreamSink : public ISink
	{
	public:

		typedef std::ostream Stream;


		explicit StreamSink(Stream &dest)
			: m_dest(dest)
		{
		}

		virtual std::size_t write(const char *src, std::size_t size) override
		{
			m_dest.write(src, static_cast<std::streamsize>(size));
			return size; //hack
		}

		virtual std::size_t overwrite(std::size_t position, const char *src, std::size_t size) override
		{
			const auto previous = m_dest.tellp();
			m_dest.seekp(static_cast<std::streamoff>(position));
			const auto result = write(src, size);
			m_dest.seekp(previous);
			return result;
		}

		virtual std::size_t position() override
		{
			return static_cast<std::size_t>(m_dest.tellp());
		}

		virtual void flush() override
		{
			m_dest.flush();
		}

	private:

		Stream &m_dest;
	};
}
