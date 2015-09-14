//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
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
#include <array>

namespace wowpp
{
	/// This class contains data from an adt file.
	class ADTFile final : public MPQFile
	{
	public:

		/// 
		struct MVERChunk final
		{
			UInt32 fourcc;
			UInt32 size;
			UInt32 version;

			/// 
			MVERChunk()
				: fourcc(0)
				, size(0)
				, version(0)
			{
			}
		};

		/// 
		struct MHDRChunk final
		{
			UInt32 fourcc;
			UInt32 size;
			UInt32 pad;
			UInt32 offsMCIN;           // MCIN
			UInt32 offsTex;            // MTEX
			UInt32 offsModels;         // MMDX
			UInt32 offsModelsIds;      // MMID
			UInt32 offsMapObejcts;     // MWMO
			UInt32 offsMapObejctsIds;  // MWID
			UInt32 offsDoodsDef;       // MDDF
			UInt32 offsObjectsDef;     // MODF
			UInt32 offsMFBO;           // MFBO
			UInt32 offsMH2O;           // MH2O
			UInt32 data1;
			UInt32 data2;
			UInt32 data3;
			UInt32 data4;
			UInt32 data5;

			/// 
			MHDRChunk()
				: fourcc(0)
				, size(0)
				, pad(0)
				, offsMCIN(0)
				, offsTex(0)
				, offsModels(0)
				, offsModelsIds(0)
				, offsMapObejcts(0)
				, offsMapObejctsIds(0)
				, offsDoodsDef(0)
				, offsObjectsDef(0)
				, offsMFBO(0)
				, offsMH2O(0)
				, data1(0)
				, data2(0)
				, data3(0)
				, data4(0)
				, data5(0)
			{
			}
		};

		/// 
		struct MCVTChunk final
		{
			UInt32 fourcc;
			UInt32 size;
			std::array<float, 9 * 9 + 8 * 8> heights;

			/// 
			MCVTChunk()
				: fourcc(0)
				, size(0)
			{
				heights.fill(0.0f);
			}
		};
		
		/// 
		struct MCLQChunk final
		{
			UInt32 fourcc;
			UInt32 size;
			float height1;
			float height2;
			
			struct LiquidData
			{
				UInt32 light;
				float height;

				LiquidData()
					: light(0)
					, height(0.0f)
				{
				}
			};
			std::array<LiquidData, 9 * 9> liquid;
			std::array<UInt8, 8 * 8> flags;
			std::array<UInt32, 21> data;

			/// 
			MCLQChunk()
				: fourcc(0)
				, size(0)
				, height1(0.0f)
				, height2(0.0f)
			{
				flags.fill(0);
				data.fill(0);
			}
		};

		/// 
		struct MCNKChunk final
		{
			UInt32 fourcc;
			UInt32 size;
			UInt32 flags;
			UInt32 ix;
			UInt32 iy;
			UInt32 nLayers;
			UInt32 nDoodadRefs;
			UInt32 offsMCVT;        // height map
			UInt32 offsMCNR;        // Normal vectors for each vertex
			UInt32 offsMCLY;        // Texture layer definitions
			UInt32 offsMCRF;        // A list of indices into the parent file's MDDF chunk
			UInt32 offsMCAL;        // Alpha maps for additional texture layers
			UInt32 sizeMCAL;
			UInt32 offsMCSH;        // Shadow map for static shadows on the terrain
			UInt32 sizeMCSH;
			UInt32 areaid;
			UInt32 nMapObjRefs;
			UInt16 holes;           // locations where models pierce the heightmap
			UInt16 pad;
			UInt16 s[2];
			UInt32 data1;
			UInt32 data2;
			UInt32 data3;
			UInt32 predTex;
			UInt32 nEffectDoodad;
			UInt32 offsMCSE;
			UInt32 nSndEmitters;
			UInt32 offsMCLQ;         // Liqid level (old)
			UInt32 sizeMCLQ;         //
			float  zpos;
			float  xpos;
			float  ypos;
			UInt32 offsMCCV;         // offsColorValues in WotLK
			UInt32 props;
			UInt32 effectId;

			/// 
			MCNKChunk()
				: fourcc(0)
				, size(0)
				, flags(0)
				, ix(0)
				, iy(0)
				, nLayers(0)
				, nDoodadRefs(0)
				, offsMCVT(0)
				, offsMCNR(0)
				, offsMCLY(0)
				, offsMCRF(0)
				, offsMCAL(0)
				, sizeMCAL(0)
				, offsMCSH(0)
				, sizeMCSH(0)
				, areaid(0)
				, nMapObjRefs(0)
				, holes(0)
				, pad(0)
				, data1(0)
				, data2(0)
				, data3(0)
				, predTex(0)
				, nEffectDoodad(0)
				, offsMCSE(0)
				, nSndEmitters(0)
				, offsMCLQ(0)
				, sizeMCLQ(0)
				, zpos(0.0f)
				, xpos(0.0f)
				, ypos(0.0f)
				, offsMCCV(0)
				, props(0)
				, effectId(0)
			{
				s[0] = 0; s[1] = 0;
			}
		};

		/// 
		struct MCINChunk final
		{
			UInt32 fourcc;
			UInt32 size;

			/// 
			struct ADTCell
			{
				UInt32 offMCNK;
				UInt32 size;
				UInt32 flags;
				UInt32 asyncId;

				/// 
				ADTCell()
					: offMCNK(0)
					, size(0)
					, flags(0)
					, asyncId(0)
				{
				}
			};
			std::array<ADTCell, 16 * 16> cells;

			/// 
			MCINChunk()
				: fourcc(0)
				, size(0)
			{
			}
		};

	public:

		/// @copydoc MPQFile::MPQFile()
		explicit ADTFile(String fileName);

		/// @copydoc MPQFile::load
		bool load() override;

		/// 
		const MVERChunk &getMVERChunk() const { return m_versionChunk; }
		/// 
		const MHDRChunk &getMHDRChunk() const { return m_headerChunk; }
		/// 
		const MCINChunk &getMCINChunk() const { return m_mcinChunk; }
		/// 
		const MCNKChunk &getMCNKChunk(UInt32 index) const { return m_mcnkChunks[index]; }
		const MCVTChunk &getMCVTChunk(UInt32 index) const { return m_heightChunks[index]; }
		const MCLQChunk &getMCLQChunk(UInt32 index) const { return m_liquidChunks[index]; }

	private:

		bool m_isValid;
		MVERChunk m_versionChunk;
		MHDRChunk m_headerChunk;
		size_t m_offsetBase;
		MCINChunk m_mcinChunk;
		std::array<MCNKChunk, 16 * 16> m_mcnkChunks;
		std::array<MCVTChunk, 16 * 16> m_heightChunks;
		std::array<MCLQChunk, 16 * 16> m_liquidChunks;
	};
}
