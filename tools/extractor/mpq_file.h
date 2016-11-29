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

#include "common/typedefs.h"
#include "binary_io/reader.h"
#include "binary_io/container_source.h"
#include "stormlib/src/StormLib.h"

namespace mpq
{
	bool loadMPQFile(const std::string &file);
}

namespace wowpp
{
	/// This class represents a file which will be loaded from an MPQ archive.
	class MPQFile : public boost::noncopyable
	{
	public:

		/// Initializes the file and loads it's content from the loaded MPQ archive.
		explicit MPQFile(String fileName);
		virtual ~MPQFile() = default;

		/// Called to load the file.
		/// @returns False, if any errors occurred during the loading process.
		virtual bool load() = 0;
		/// Returns the file name of this MPQ file inside the archive.
		const String &getFileName() const { return m_fileName; }
		/// Returns the files base name (without path and extension).
		const String &getBaseName() const { return m_baseName; }

	protected:

		std::unique_ptr<io::ISource> m_source;
		io::Reader m_reader;
		std::vector<char> m_buffer;

	private:

		String m_baseName;
		String m_fileName;
	};
}
