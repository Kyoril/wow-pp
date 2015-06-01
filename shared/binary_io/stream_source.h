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

namespace io
{
	class StreamSource : public ISource
	{
	public:

		explicit StreamSource(std::istream &stream)
			: m_stream(stream)
		{
			const std::streampos old = m_stream.tellg();
			m_stream.seekg(static_cast<std::streamoff>(0), std::ios::end);
			m_streamSize = static_cast<size_t>(m_stream.tellg());
			m_stream.seekg(old, std::ios::beg);
		}

		virtual bool end() const override
		{
			return m_stream.eof();
		}

		virtual std::size_t read(char *dest, std::size_t size) override
		{
			m_stream.read(dest, static_cast<std::streamsize>(size));
			return static_cast<std::size_t>(m_stream.gcount());
		}

		virtual std::size_t skip(std::size_t size) override
		{
			const std::streampos old = m_stream.tellg();
			m_stream.seekg(
			    old + static_cast<std::streamoff>(size),
			    std::ios::beg);
			const std::streampos new_ = m_stream.tellg();
			return static_cast<std::size_t>(new_ - old);
		}

		virtual std::size_t size() const override
		{
			return m_streamSize;
		}

		virtual std::size_t position() const override
		{
			return static_cast<std::size_t>(m_stream.tellg());
		}

	private:

		std::istream &m_stream;
		std::size_t m_streamSize;
	};
}
