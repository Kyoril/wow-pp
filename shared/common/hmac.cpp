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

#include "hmac.h"
#include <boost/iostreams/copy.hpp>
#include <boost/io/ios_state.hpp>
#include <locale>
#include <iomanip>
#include <cassert>
#include <openssl/hmac.h>
#include <openssl/sha.h>

namespace wowpp
{
	struct OpenSSL_HMACHashSink::Context : HMAC_CTX
	{

	};
	
	OpenSSL_HMACHashSink::OpenSSL_HMACHashSink()
	{
		createContext();
	}

	std::streamsize OpenSSL_HMACHashSink::write(const char *data, std::streamsize length)
	{
		assert(data);
		assert(length >= 0);
		assert(static_cast<boost::uintmax_t>(length) <= std::numeric_limits<size_t>::max());

		if (!m_context)
		{
			createContext();
		}

#ifdef __APPLE__
        HMAC_Update(m_context.get(), reinterpret_cast<const unsigned char*>(data), static_cast<size_t>(length));
#else
        if (!HMAC_Update(m_context.get(), reinterpret_cast<const unsigned char*>(data), static_cast<size_t>(length)))
        {
            throw std::runtime_error("HMAC_Update failed");
        }
#endif

		return length;
	}

	HMACHash OpenSSL_HMACHashSink::finalizeHash()
	{
		HMACHash digest;
		assert(digest.size() == 20);
		unsigned int len = 0;
        
#ifdef __APPLE__
        HMAC_Final(m_context.get(), digest.data(), &len);
#else
        if (!HMAC_Final(m_context.get(), digest.data(), &len))
        {
            throw std::runtime_error("HMAC_Final failed");
        }
#endif
		assert(len == 20);

		m_context.reset();
		return digest;
	}

	void OpenSSL_HMACHashSink::createContext()
	{
		// Setup key
		size_t i = 0;
		m_key[i++] = 0x38; m_key[i++] = 0xA7; m_key[i++] = 0x83; m_key[i++] = 0x15;
		m_key[i++] = 0xF8; m_key[i++] = 0x92; m_key[i++] = 0x25; m_key[i++] = 0x30;
		m_key[i++] = 0x71; m_key[i++] = 0x98; m_key[i++] = 0x67; m_key[i++] = 0xB1;
		m_key[i++] = 0x8C; m_key[i++] = 0x04; m_key[i++] = 0xE2; m_key[i++] = 0xAA;

		m_context = std::make_shared<Context>();
		HMAC_CTX_init(m_context.get());
#ifdef __APPLE__
        HMAC_Init_ex(m_context.get(), m_key.data(), m_key.size(), EVP_sha1(), nullptr);
#else
        if (!HMAC_Init_ex(m_context.get(), m_key.data(), m_key.size(), EVP_sha1(), nullptr))
        {
            throw std::runtime_error("HMAC_Init_ex failed");
        }
#endif
	}


	HMACHash hmac(std::istream &source)
	{
		OpenSSL_HMACHashSink sink;
		boost::iostreams::copy(source, sink);
		return sink.finalizeHash();
	}

	void hmacPrintHex(std::ostream &sink, const HMACHash &value)
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

	HMACHash hmacParseHex(std::istream &source)
	{
		boost::io::ios_all_saver saver(source);

		HMACHash result;

		for (unsigned char & e : result)
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
