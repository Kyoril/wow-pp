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
#include "hmac.h"
#include "macros.h"
#include "utilities.h"
#include <openssl/hmac.h>
#include <openssl/sha.h>

namespace wowpp
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	struct OpenSSL_HMACHashSink::Context : HMAC_CTX
	{
	};
	
#else
	struct OpenSSL_HMACHashSink::Context
	{
		HMAC_CTX* Ctx;
		
		Context()
			: Ctx(nullptr)
		{		
		}
		Context(const std::array<unsigned char, 16>& key)
			: Ctx(nullptr)
		{
			Ctx = HMAC_CTX_new();
			HMAC_Init_ex(Ctx, key.data(), key.size(), EVP_sha1(), nullptr);
		}
		~Context()
		{
			if (Ctx)
			{
				HMAC_CTX_free(Ctx);
				Ctx = nullptr;
			}
		}
	};
#endif

	OpenSSL_HMACHashSink::OpenSSL_HMACHashSink()
	{
		createContext();
	}

	std::streamsize OpenSSL_HMACHashSink::write(const char *data, std::streamsize length)
	{
		ASSERT(data);
		ASSERT(length >= 0);
		ASSERT(static_cast<boost::uintmax_t>(length) <= std::numeric_limits<size_t>::max());

		if (!m_context)
		{
			createContext();
		}

#ifdef __APPLE__
		HMAC_Update(reinterpret_cast<HMAC_CTX*>(getContext()), reinterpret_cast<const unsigned char *>(data), static_cast<size_t>(length));
#else
		if (!HMAC_Update(reinterpret_cast<HMAC_CTX*>(getContext()), reinterpret_cast<const unsigned char *>(data), static_cast<size_t>(length)))
		{
			throw std::runtime_error("HMAC_Update failed");
		}
#endif

		return length;
	}

	HMACHash OpenSSL_HMACHashSink::finalizeHash()
	{
		HMACHash digest;
		ASSERT(digest.size() == 20);
		unsigned int len = 0;

#ifdef __APPLE__
		HMAC_Final(reinterpret_cast<HMAC_CTX*>(getContext()), digest.data(), &len);
#else
		if (!HMAC_Final(reinterpret_cast<HMAC_CTX*>(getContext()), digest.data(), &len))
		{
			throw std::runtime_error("HMAC_Final failed");
		}
#endif
		ASSERT(len == 20);

		m_context.reset();
		return digest;
	}

	hmac_ctx_st* OpenSSL_HMACHashSink::getContext()
	{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
		return m_context.get();
#else
		return reinterpret_cast<hmac_ctx_st*>(m_context->Ctx);
#endif

	}
	
	void OpenSSL_HMACHashSink::createContext()
	{
		// Setup key
		size_t i = 0;
		m_key[i++] = 0x38;
		m_key[i++] = 0xA7;
		m_key[i++] = 0x83;
		m_key[i++] = 0x15;
		m_key[i++] = 0xF8;
		m_key[i++] = 0x92;
		m_key[i++] = 0x25;
		m_key[i++] = 0x30;
		m_key[i++] = 0x71;
		m_key[i++] = 0x98;
		m_key[i++] = 0x67;
		m_key[i++] = 0xB1;
		m_key[i++] = 0x8C;
		m_key[i++] = 0x04;
		m_key[i++] = 0xE2;
		m_key[i++] = 0xAA;
		
 #if OPENSSL_VERSION_NUMBER < 0x10100000L
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
#else
		m_context = std::make_shared<Context>(std::ref(m_key));
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
	
	HMACHash hmacParseHex(std::istream &source)
	{
		boost::io::ios_all_saver saver(source);

		HMACHash result;

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
