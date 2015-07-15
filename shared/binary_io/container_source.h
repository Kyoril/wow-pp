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
	template <class C>
	class ContainerSource : public ISource
	{
	public:

		typedef C Container;


		explicit ContainerSource(const Container &container)
			: m_container(container)
			, m_position(0)
		{
		}

		std::size_t getSizeInBytes() const
		{
			return m_container.size() * sizeof(m_container[0]);
		}

		virtual bool end() const
		{
			return (m_position == m_container.size());
		}

		virtual std::size_t read(char *dest, std::size_t size) override
		{
			const std::size_t rest = getSizeInBytes() - m_position;
			const std::size_t copyable = std::min(size, rest);
			const char *const begin = &m_container[0] + m_position;
			std::memcpy(dest, begin, copyable);
			m_position += copyable;
			return copyable;
		}

		virtual std::size_t skip(std::size_t size) override
		{
			const std::size_t rest = getSizeInBytes() - m_position;
			const std::size_t skippable = std::min(size, rest);
			m_position += skippable;
			return skippable;
		}

		virtual void seek(std::size_t pos) override
		{
			const std::size_t max = m_container.size();
			m_position = std::min(pos, max);
		}

		virtual std::size_t size() const override
		{
			return m_container.size();
		}

		virtual std::size_t position() const override
		{
			return m_position;
		}

	private:

		const Container &m_container;
		std::size_t m_position;
	};
}
