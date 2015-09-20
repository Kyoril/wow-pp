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
#include <boost/uuid/sha1.hpp>
#include <memory>
#include <array>

namespace wowpp
{
	/// Represents a SHA1 hash in it's binary form
	typedef std::array<unsigned char, 20> SHA1Hash;

	/// Generates a SHA1 hash.
	class Boost_SHA1HashSink : public boost::iostreams::sink
	{
	public:

		typedef char char_type;
		typedef boost::iostreams::sink_tag category;

	public:

		Boost_SHA1HashSink();
		std::streamsize write(const char *data, std::streamsize length);
		SHA1Hash finalizeHash();

	private:

		std::shared_ptr<boost::uuids::detail::sha1> m_sha1;
	};

	/// Generates a SHA1 hash based on the contents of an std::istream.
	/// @returns Binary SHA1 hash.
	SHA1Hash sha1(std::istream &source);
	/// Writes the hexadecimal string of a binary SHA1 hash to an std::ostream.
	/// @param sink The output stream.
	/// @param value The binary SHA1 hash.
	void sha1PrintHex(std::ostream &sink, const SHA1Hash &value);
	/// Converts a hexadecimal SHA1 hash to a binary SHA1 hash if possible.
	/// @param source The input stream to read from.
	/// @returns Converted binary SHA1 hash.
	SHA1Hash sha1ParseHex(std::istream &source);
}
