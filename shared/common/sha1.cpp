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
#include "sha1.h"

namespace wowpp
{
	Boost_SHA1HashSink::Boost_SHA1HashSink()
		: m_sha1(std::make_shared<boost::uuids::detail::sha1>())
	{
	}

	std::streamsize Boost_SHA1HashSink::write(const char *data, std::streamsize length)
	{
		assert(data);
		assert(length >= 0);

		if (!m_sha1)
		{
			m_sha1 = std::make_shared<boost::uuids::detail::sha1>();
		}

		m_sha1->process_bytes(data, static_cast<size_t>(length));
		return length;
	}

	SHA1Hash Boost_SHA1HashSink::finalizeHash()
	{
		unsigned int digest[5];
		assert(sizeof(digest) == 20);

		m_sha1->get_digest(digest);

		SHA1Hash hash;
		assert(sizeof(digest) == sizeof(hash));
		auto dest = hash.begin();
		for (const unsigned int digit : digest)
		{
			assert(sizeof(digit) == 4);
			assert(dest != hash.end());

			dest[0] = static_cast<unsigned char>(digit >> 24);
			dest[1] = static_cast<unsigned char>(digit >> 16);
			dest[2] = static_cast<unsigned char>(digit >>  8);
			dest[3] = static_cast<unsigned char>(digit);

			std::advance(dest, 4);
		}

		m_sha1.reset();
		return hash;
	}

	SHA1Hash sha1(std::istream &source)
	{
		Boost_SHA1HashSink sink;
		boost::iostreams::copy(source, sink);
		return sink.finalizeHash();
	}

	void sha1PrintHex(std::ostream &sink, const SHA1Hash &value)
	{
		boost::io::ios_all_saver saver(sink);

		for (unsigned char e : value)
		{
			sink
			        << std::hex
			        << std::setfill('0')
			        << std::setw(2)
			        << (static_cast<unsigned>(e) & 0xff);
		}
	}

	namespace
	{
		int hexDigitValue(char c)
		{
			if (c >= '0' && c <= '9')
			{
				return (c - '0');
			}

			std::locale loc;
			c = static_cast<char>(std::tolower(c, loc));

			if (c >= 'a' && c <= 'f')
			{
				return (c - 'a') + 10;
			}

			return -1;
		}
	}

	SHA1Hash sha1ParseHex(std::istream &source)
	{
		boost::io::ios_all_saver saver(source);

		SHA1Hash result;

		for (unsigned char &e : result)
		{
			char high, low;
			source >> high >> low;

			const int left = hexDigitValue(high);
			const int right = hexDigitValue(low);

			if (left < 0 ||
			        right < 0)
			{
				source.setstate(std::ios::failbit);
				break;
			}

			e = static_cast<unsigned char>((left * 16) + right);
		}

		return result;
	}
}
