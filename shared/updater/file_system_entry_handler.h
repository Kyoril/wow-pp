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

#include "file_entry_handler.h"
#include "common/macros.h"

namespace wowpp
{
	namespace updating
	{
		struct FileSystemEntryHandler : IFileEntryHandler
		{
			virtual PreparedUpdate handleDirectory(
			    const PrepareParameters &parameters,
			    const UpdateListProperties &listProperties,
			    const sff::read::tree::Array<std::string::const_iterator> &entries,
			    const std::string &type,
			    const std::string &source,
			    const std::string &destination) override;

			virtual PreparedUpdate handleFile(
			    const PrepareParameters &parameters,
			    const sff::read::tree::Table<std::string::const_iterator> &entryDescription,
			    const std::string &source,
			    const std::string &destination,
			    boost::uintmax_t originalSize,
			    const SHA1Hash &sha1,
			    const std::string &compression,
			    boost::uintmax_t compressedSize
			) override;

			virtual PreparedUpdate finish(const PrepareParameters &parameters) override;
		};
	}
}
