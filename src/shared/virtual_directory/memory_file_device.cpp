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

#include "pch.h"
#include "memory_file_device.h"
#include "memory_directory.h"

namespace wowpp
{
	namespace virtual_dir
	{
		struct MemoryFileDevice::Impl
		{
			Impl(
			    MemoryFile &file,
			    bool isWriteable)
				: m_file(file)
				, m_position(0)
				, m_isWriteable(isWriteable)
			{
				if (m_isWriteable)
				{
					if (m_file.currentReaders > 0 ||
					        m_file.currentWriters > 0)
					{
						throw std::runtime_error(
						    "Cannot open memory file for writing when already "
						    "in use");
					}

					++m_file.currentWriters;
				}
				else
				{
					if (m_file.currentWriters > 0)
					{
						throw std::runtime_error(
						    "Cannot open memory file for reading while it is "
						    "opened for writing");
					}
				}

				++m_file.currentReaders;
			}

			~Impl()
			{
				assert(m_file.currentReaders > 0);
				--m_file.currentReaders;
				if (m_isWriteable)
				{
					assert(m_file.currentWriters > 0);
					--m_file.currentWriters;
				}
			}

			std::streamsize read(
			    char *s,
			    std::streamsize n)
			{
				assert(s);
				assert(n >= 0);

				const auto readCount = std::min<std::streamsize>(
				                           n,
				                           static_cast<std::streamsize>(m_file.content.size()) - m_position);
				if (readCount > 0)
				{
					const auto begin = m_file.content.begin() + m_position;
					std::copy(
					    begin,
					    begin + readCount,
					    s);
					m_position += readCount;
					return readCount;
				}

				return -1;
			}

			std::streamsize write(
			    const char *s,
			    std::streamsize n)
			{
				assert(s);
				assert(n >= 0);

				if (!m_isWriteable)
				{
					throw std::logic_error("Cannot write to a memory file "
					                       "opened read-only");
				}

				const auto newFileSize = std::max<boost::uintmax_t>(
				                             m_file.content.size(),
				                             static_cast<boost::uintmax_t>(m_position + n));

				if (newFileSize > m_file.content.max_size())
				{
					throw std::runtime_error(
					    "Cannot write: Maximum memory file size exceeded");
				}

				m_file.content.resize(static_cast<size_t>(newFileSize));
				std::copy(
				    s,
				    s + n,
				    m_file.content.begin() + static_cast<ptrdiff_t>(m_position));

				m_position += n;
				return n;
			}

			boost::iostreams::stream_offset seek(
			    boost::iostreams::stream_offset off,
			    std::ios_base::seekdir way)
			{
				switch (way)
				{
				case std::ios_base::beg:
					{
						m_position = 0;
						break;
					}

				case std::ios_base::cur:
					{
						break;
					}

				case std::ios_base::end:
					{
						m_position = static_cast<boost::iostreams::stream_offset>(
						                 m_file.content.size());
						break;
					}
				}

				//TODO prevent overflow
				m_position += off;

				if (m_position < 0)
				{
					m_position = 0;
				}
				else if (static_cast<boost::uintmax_t>(m_position) >
				         m_file.content.size())
				{
					m_position = static_cast<boost::iostreams::stream_offset>(m_file.content.size());
				}

				return m_position;
			}

		private:

			MemoryFile &m_file;
			boost::iostreams::stream_offset m_position;
			const bool m_isWriteable;
		};


		MemoryFileDevice::MemoryFileDevice(
		    MemoryFile &file,
		    bool isWriteable)
			: m_impl(std::make_shared<Impl>(file, isWriteable))
		{
		}

		std::streamsize MemoryFileDevice::read(
		    char *s,
		    std::streamsize n)
		{
			return m_impl->read(s, n);
		}

		std::streamsize MemoryFileDevice::write(
		    const char *s,
		    std::streamsize n)
		{
			return m_impl->write(s, n);
		}

		boost::iostreams::stream_offset MemoryFileDevice::seek(
		    boost::iostreams::stream_offset off,
		    std::ios_base::seekdir way)
		{
			return m_impl->seek(off, way);
		}
	}
}
