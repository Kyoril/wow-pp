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
#include "copy_with_progress.h"

namespace wowpp
{
	namespace updating
	{
		void copyWithProgress(
		    const UpdateParameters &parameters,
		    std::istream &source,
		    std::ostream &sink,
		    const std::string &name,
		    boost::uintmax_t compressedSize,
		    bool doZLibUncompress
		)
		{
			boost::iostreams::filtering_ostream decompressor;

			if (doZLibUncompress)
			{
				boost::iostreams::zlib_params params;
				params.noheader = false;
				decompressor.push(boost::iostreams::zlib_decompressor());
			}

			decompressor.push(sink);

			boost::uintmax_t written = 0;
			parameters.progressHandler.updateFile(name, compressedSize, written);

			for (;;)
			{
				std::array<char, 1024 * 16> buffer;
				source.read(buffer.data(), buffer.size());
				const auto readSize = source.gcount();
				if ((written + readSize) > compressedSize)
				{
					throw std::runtime_error(name + ": Received more than expected");
				}
				decompressor.write(buffer.data(), readSize);
				written += readSize;

				parameters.progressHandler.updateFile(name, compressedSize, written);

				if (!source)
				{
					if (written < compressedSize)
					{
						throw std::runtime_error(name + ": Received incomplete file");
					}
					break;
				}
			}

			decompressor.flush();
		}
	}
}
