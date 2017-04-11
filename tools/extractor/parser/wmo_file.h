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
	struct MVERChunk final
	{
		UInt32 cc;
		UInt32 size;
		UInt32 version;

		explicit MVERChunk()
			: cc(0)
			, size(0)
			, version(0)
		{
		}
	};

	struct MOHDChunk final
	{
		UInt32 textureCount;
		UInt32 groupCount;
		UInt32 portalCount;
		UInt32 lightCount;
		UInt32 doodadNameCount;
		UInt32 doodadDefCount;
		UInt32 doodadSetCount;
		UInt32 colorRGBX;
		UInt32 wmoId;
		math::Vector3 boundsMin;
		math::Vector3 boundsMax;
		UInt32 flags;

		explicit MOHDChunk()
			: textureCount(0)
			, groupCount(0)
			, portalCount(0)
			, lightCount(0)
			, doodadNameCount(0)
			, doodadDefCount(0)
			, colorRGBX(0)
			, wmoId(0)
			, flags(0)
		{
		}
	};

	struct MOGPChunk final
	{
		UInt32 groupName;
		UInt32 descriptiveName;
		UInt32 flags;
		math::Vector3 boundsMin;
		math::Vector3 boundsMax;
		UInt16 moprIndex;
		UInt16 mopfUseCount;
		UInt16 numberOfBatchesA;
		UInt16 numberOfBatchesInterior;
		UInt16 numberOfBatchesExterior;
		std::array<UInt8, 4> fogIndices;
		UInt32 liquidType;
		UInt32 wmoGroupId;	// area
		UInt32 unknown1;
		UInt32 unknown2;

		explicit MOGPChunk()
			: groupName(0)
			, descriptiveName(0)
			, flags(0)
			, moprIndex(0)
			, mopfUseCount(0)
			, numberOfBatchesA(0)
		{
		}
	};

	/// This class contains data from a wmo file.
	class WMOFile final : public MPQFile
	{
	public:

		/// 
		explicit WMOFile(String fileName);

		/// @copydoc MPQFile::load()
		bool load() override;
		/// Determines whether this is a root WMO or a group WMO.
		bool isRootWMO() const { return m_isRoot; }

		/// Returns a constant reference to the MOHD chunk data.
		/// Only filled with valid data if this is a root WMO.
		const MOHDChunk &getMOHDChunk() const { return m_rootHeader; }
		/// 
		const MOGPChunk &getMOGPChunk() const { return m_groupHeader; }

		const std::vector<math::Vector3> &getVertices() const { return m_vertices; }
		const std::vector<UInt32> &getIndices() const { return m_indices; }
		const std::vector<std::unique_ptr<WMOFile>> &getGroups() const { return m_wmoGroups; }
		const bool isCollisionTriangle(UInt32 triangleIndex) const;

	private:

		MOHDChunk m_rootHeader;
		MOGPChunk m_groupHeader;
		bool m_isValid;
		bool m_isRoot;
		std::vector<std::unique_ptr<WMOFile>> m_wmoGroups;
		std::vector<math::Vector3> m_vertices;
		std::vector<UInt32> m_indices;
		std::vector<char> m_triangleProps;
	};
}
