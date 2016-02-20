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

#include "relative_reader.h"

namespace wowpp
{
	namespace virtual_dir
	{
		RelativeReader::RelativeReader(IReader &parent, Path relation)
			: m_parent(parent)
			, m_relation(std::move(relation))
		{
		}

		IReader &RelativeReader::getParent() const
		{
			return m_parent;
		}

		const Path &RelativeReader::getRelation() const
		{
			return m_relation;
		}

		file_type::Enum RelativeReader::getType(
		    const Path &fileName
		)
		{
			const auto fullPath = joinPaths(m_relation, fileName);
			return m_parent.getType(fullPath);
		}

		std::unique_ptr<std::istream> RelativeReader::readFile(
		    const Path &fileName,
		    bool openAsText)
		{
			const auto fullPath = joinPaths(m_relation, fileName);
			return m_parent.readFile(fullPath, openAsText);
		}

		std::set<Path> RelativeReader::queryEntries(
		    const Path &fileName
		)
		{
			const auto fullPath = joinPaths(m_relation, fileName);
			return m_parent.queryEntries(fullPath);
		}
	}
}
