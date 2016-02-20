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

#include <boost/iostreams/concepts.hpp>
#include <memory>
#include <array>

namespace wowpp
{
	/// Represents a SHA1 hash in it's binary form
	typedef std::array<unsigned char, 20> HMACHash;

	/// Generates a HMAC hash.
	struct OpenSSL_HMACHashSink : boost::iostreams::sink
	{
		typedef char char_type;
		typedef boost::iostreams::sink_tag category;

		OpenSSL_HMACHashSink();
		std::streamsize write(const char *data, std::streamsize length);
		HMACHash finalizeHash();

	private:

		//hack to save us from including openssl/sha.h everywhere
		struct Context;
		std::shared_ptr<Context> m_context;

		void createContext();

		std::array<unsigned char, 16> m_key;
	};

	/// Generates a HMAC hash based on the contents of an std::istream.
	/// @returns Binary HMAC hash.
	HMACHash hmac(std::istream &source);
	/// Writes the hexadecimal string of a binary HMAC hash to an std::ostream.
	/// @param sink The output stream.
	/// @param value The binary HMAC hash.
	void hmacPrintHex(std::ostream &sink, const HMACHash &value);
	/// Converts a hexadecimal HMAC hash to a binary SHA1 hash if possible.
	/// @param source The input stream to read from.
	/// @returns Converted binary HMAC hash.
	HMACHash hmacParseHex(std::istream &source);
}
