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

#include "mpq_file.h"
#include "math/vector3.h"

namespace wowpp
{
	/// This class contains data from a m2 file.
	class M2File final : public MPQFile
	{
	public:

		/// 
		explicit M2File(String fileName);

		/// @copydoc MPQFile::load()
		bool load() override;

		/*
		const std::vector<math::Vector3> &getVertices() const { return m_vertices; }
		const std::vector<UInt16> &getIndices() const { return m_indices; }
		const std::vector<std::unique_ptr<WMOFile>> &getGroups() const { return m_wmoGroups; }
		const bool isCollisionTriangle(UInt32 triangleIndex) const;
		*/

	private:

		/*
		MOHDChunk m_rootHeader;
		MOGPChunk m_groupHeader;
		*/
		bool m_isValid;
		/*bool m_isRoot;
		std::vector<std::unique_ptr<WMOFile>> m_wmoGroups;
		std::vector<math::Vector3> m_vertices;
		std::vector<UInt16> m_indices;
		std::vector<char> m_triangleProps;
		*/
	};
}
