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
#include "update_source_file.h"

namespace wowpp
{
	namespace updating
	{
		UpdateSourceFile::UpdateSourceFile()
		{
		}

		UpdateSourceFile::UpdateSourceFile(
		    boost::any internalData,
		    std::unique_ptr<std::istream> content,
		    boost::optional<boost::uintmax_t> size)
			: internalData(std::move(internalData))
			, content(std::move(content))
			, size(size)
		{
		}

		UpdateSourceFile::UpdateSourceFile(UpdateSourceFile  &&other)
		{
			swap(other);
		}

		UpdateSourceFile &UpdateSourceFile::operator = (UpdateSourceFile && other)
		{
			swap(other);
			return *this;
		}

		void UpdateSourceFile::swap(UpdateSourceFile &other)
		{
			using std::swap;
			using boost::swap;

			swap(internalData, other.internalData);
			swap(content, other.content);
			swap(size, other.size);
		}


		void checkExpectedFileSize(
		    const std::string &fileName,
		    boost::uintmax_t expected,
		    const UpdateSourceFile &found
		)
		{
			if (found.size &&
			        *found.size != expected)
			{
				throw std::runtime_error(
				    fileName + ": Size expected to be " +
				    boost::lexical_cast<std::string>(expected) +
				    " but found " +
				    boost::lexical_cast<std::string>(*found.size));
			}
		}
	}
}
