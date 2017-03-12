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
#include "adt_page.h"
#include "log/default_log_levels.h"
#include "OgreMatrix4.h"
#include "OgreVector3.h"
#include "OgreQuaternion.h"

namespace wowpp
{
	namespace adt
	{
#define MAKE_CHUNK_HEADER(first, second, third, fourth) \
	((UInt32)fourth) | (((UInt32)third) << 8) | (((UInt32)second) << 16) | (((UInt32)first) << 24)
		// MAIN Chunks
		static const UInt32 MVERChunk = MAKE_CHUNK_HEADER('M', 'V', 'E', 'R');		// 4 bytes, usually 0x12 format version
		static const UInt32 MHDRChunk = MAKE_CHUNK_HEADER('M', 'H', 'D', 'R');		// some offsets for faster access
		static const UInt32 MCINChunk = MAKE_CHUNK_HEADER('M', 'C', 'I', 'N');		// 
		static const UInt32 MTEXChunk = MAKE_CHUNK_HEADER('M', 'T', 'E', 'X');		// list of texture file names used by this page
		static const UInt32 MWIDChunk = MAKE_CHUNK_HEADER('M', 'W', 'I', 'D');		// list of wmo file names used by this page
		static const UInt32 MWMOChunk = MAKE_CHUNK_HEADER('M', 'W', 'M', 'O');		// 
		static const UInt32 MMIDChunk = MAKE_CHUNK_HEADER('M', 'M', 'I', 'D');		// 
		static const UInt32 MMDXChunk = MAKE_CHUNK_HEADER('M', 'M', 'D', 'X');		// 
		static const UInt32 MDDFChunk = MAKE_CHUNK_HEADER('M', 'D', 'D', 'F');		// 
		static const UInt32 MODFChunk = MAKE_CHUNK_HEADER('M', 'O', 'D', 'F');		// 
		static const UInt32 MCNKChunk = MAKE_CHUNK_HEADER('M', 'C', 'N', 'K');		// 
		static const UInt32 MH2OChunk = MAKE_CHUNK_HEADER('M', 'H', '2', 'O');		// 
		static const UInt32 MFBOChunk = MAKE_CHUNK_HEADER('M', 'F', 'B', 'O');		// 
		static const UInt32 MTXPChunk = MAKE_CHUNK_HEADER('M', 'T', 'X', 'P');		// 
		static const UInt32 MTXFChunk = MAKE_CHUNK_HEADER('M', 'T', 'X', 'F');		// 
		// SUB Chunks of MCNK chunk
		static const UInt32 MCVTSubChunk = MAKE_CHUNK_HEADER('M', 'C', 'V', 'T');	// 
		static const UInt32 MCNRSubChunk = MAKE_CHUNK_HEADER('M', 'C', 'N', 'R');	//
        static const UInt32 MCSHSubChunk = MAKE_CHUNK_HEADER('M', 'C', 'S', 'H');	//
        static const UInt32 MCALSubChunk = MAKE_CHUNK_HEADER('M', 'C', 'A', 'L');	//
        static const UInt32 MCLYSubChunk = MAKE_CHUNK_HEADER('M', 'C', 'L', 'Y');	//
#undef MAKE_CHUNK_HEADER

		namespace read
		{
			static bool readMVERChunk(adt::Page &page, const Ogre::DataStreamPtr &ptr, UInt32 chunkSize)
			{
				UInt32 formatVersion;
				if (!ptr->read(&formatVersion, sizeof(UInt32)))
				{
					ELOG("Could not read MVER chunk!");
					return false;
				}

				// Log file format version
				if (formatVersion != 0x12)
				{
					ILOG("Expected ADT file format 0x12, but found 0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') 
						<< formatVersion << " instead. Please make sure that you use World of Warcraft Version 2.4.3!");
					return false;
				}

				return true;
			}

			static bool readMHDRChunk(adt::Page &page, const Ogre::DataStreamPtr &ptr, UInt32 chunkSize)
			{
				ptr->skip(chunkSize);
				return true;
			}

			static bool readMTEXChunk(adt::Page &page, const Ogre::DataStreamPtr &ptr, UInt32 chunkSize)
			{
				String buffer;
				buffer.resize(chunkSize);
				if (!ptr->read(&buffer[0], chunkSize))
				{
					return false;
				}

				// Parse for textures
				char *reader = &buffer[0];
				while (*reader)
				{
					String texture = String(reader);
					reader += texture.size() + 1;

					page.terrain.textures.push_back(texture);
				}

				return true;
			}

			static bool readMMDXChunk(adt::Page &page, const Ogre::DataStreamPtr &ptr, UInt32 chunkSize)
			{
				String buffer;
				buffer.resize(chunkSize);
				if (!ptr->read(&buffer[0], chunkSize))
				{
					return false;
				}

				// Parse for textures
				char *reader = &buffer[0];
				while (*reader)
				{
					String model = String(reader);
					reader += model.size() + 1;

					page.terrain.m2Ids.push_back(model);
				}

				return true;
			}

			static bool readMMIDChunk(adt::Page &page, const Ogre::DataStreamPtr &ptr, UInt32 chunkSize)
			{
				ptr->skip(chunkSize);
				return true;
			}

			static bool readMDDFChunk(adt::Page &page, const Ogre::DataStreamPtr &ptr, UInt32 chunkSize)
			{
				size_t numEntries = chunkSize / sizeof(terrain::model::M2Placement);
				page.terrain.m2Placements.resize(numEntries);
				for (size_t i = 0; i < numEntries; ++i)
				{
					auto &placement = page.terrain.m2Placements[i];
					ptr->read(&placement, sizeof(terrain::model::M2Placement));
				}

				return true;
			}
            
            struct MCINEntry
            {
                UInt32 offsMCNK;            // absolute offset.
                UInt32 size;                // the size of the MCNK chunk, this is refering to.
                UInt32 flags;               // these two are always 0. only set in the client.
                UInt32 asyncId;
            };

			struct MCNKHeader
			{
				/*0000*/	UInt32 flags;
				/*0004*/	UInt32 IndexX;
				/*0008*/	UInt32 IndexY;
				/*0012*/	UInt32 nLayers;				// maximum 4
				/*0016*/	UInt32 nDoodadRefs;
				/*0020*/	UInt32 ofsHeight;
				/*0024*/	UInt32 ofsNormal;
				/*0028*/	UInt32 ofsLayer;
				/*0032*/	UInt32 ofsRefs;
				/*0036*/	UInt32 ofsAlpha;
				/*0040*/	UInt32 sizeAlpha;
				/*0044*/	UInt32 ofsShadow;				// only with flags&0x1
				/*0048*/	UInt32 sizeShadow;
				/*0052*/	UInt32 areaid;
				/*0056*/	UInt32 nMapObjRefs;
				/*0060*/	UInt16 holes;
							UInt16 pad;
				/*0064*/	UInt32 ReallyLowQualityTextureingMap[4];	// It is used to determine which detail doodads to show. Values are an array of two bit unsigned integers, naming the layer.
				/*0080*/	UInt32 predTex;				// 03-29-2005 By ObscuR; TODO: Investigate
				/*0084*/	UInt32 noEffectDoodad;				// 03-29-2005 By ObscuR; TODO: Investigate
				/*0088*/	UInt32 ofsSndEmitters;
				/*0092*/	UInt32 nSndEmitters;				//will be set to 0 in the client if ofsSndEmitters doesn't point to MCSE!
				/*0096*/	UInt32 ofsLiquid;
				/*0100*/	UInt32 sizeLiquid;  			// 8 when not used; only read if >8.
				/*0104*/	float x, y, z;
				/*0116*/	UInt32 ofsMCCV; 				// only with flags&0x40, had UINT32 textureId; in ObscuR's structure.
				/*0120*/	UInt32 ofsMCLV; 				// 
				/*0124*/	UInt32 unused; 			        // currently unused
			};

			static bool readMCVTSubChunk(adt::Page &page, const Ogre::DataStreamPtr &ptr, UInt32 chunkSize, const MCNKHeader &header)
			{
				assert(header.IndexX < constants::TilesPerPage);
				assert(header.IndexY < constants::TilesPerPage);
				assert(chunkSize == sizeof(float) * constants::VertsPerTile);

				// Calculate tile index
				UInt32 tileIndex = header.IndexY + header.IndexX * constants::TilesPerPage;
				auto &tileHeight = page.terrain.heights[tileIndex];
				auto &tileHole = page.terrain.holes[tileIndex];
				if (!ptr->read(&tileHeight[0], sizeof(float) * tileHeight.size()))
				{
					ELOG("Could not read MCVT subchunk (eof reached?)");
					return false;
				}

				// Add tile height
				for (auto &height : tileHeight)
				{
					height += header.z;
				}

				tileHole = header.holes;
				return true;
			}
            
            static bool readMCNRSubChunk(adt::Page &page, const Ogre::DataStreamPtr &ptr, UInt32 chunkSize, const MCNKHeader &header)
            {
                assert(header.IndexX < constants::TilesPerPage);
                assert(header.IndexY < constants::TilesPerPage);

                // Calculate tile index
                UInt32 tileIndex = header.IndexY + header.IndexX * constants::TilesPerPage;
                auto &tileNormals = page.terrain.normals[tileIndex];
                
                std::array<std::array<Int8, 3>, constants::VertsPerTile> normals;
                if (!ptr->read(&normals[0], sizeof(Int8) * 3 * normals.size()))
                {
                    ELOG("Could not read MCNR subchunk (eof reached?)");
                    return false;
                }
                
                // Add tile height
                UInt32 index = 0;
                for (auto &normal : normals)
                {
                    tileNormals[index][0] = static_cast<float>(normal[0]) / 127.0f;
                    tileNormals[index][1] = static_cast<float>(normal[1]) / 127.0f;
                    tileNormals[index][2] = static_cast<float>(normal[2]) / 127.0f;
                    index++;
                }
                
                // Unknown 13 bytes...
                ptr->skip(13);
                
                return true;
            }

			struct MapChunkLayerDef
			{
				UInt32 textureId;
				UInt32 flags;
				UInt32 offsetInMCAL;
				Int32 effectId;
			};

			static bool readMCLYSubChunk(adt::Page &page, const Ogre::DataStreamPtr &ptr, UInt32 chunkSize, const MCNKHeader &header)
			{
				assert(header.IndexX < constants::TilesPerPage);
				assert(header.IndexY < constants::TilesPerPage);

				size_t endPos = ptr->tell() + chunkSize;

				UInt32 numLayers = chunkSize / sizeof(MapChunkLayerDef);
				if (numLayers > 4)
				{
					ptr->seek(endPos);
					return false;
				}

				// Calculate tile index
				UInt32 tileIndex = header.IndexY + header.IndexX * constants::TilesPerPage;
				auto &tileTextures = page.terrain.textureIds[tileIndex];

				for (UInt32 i = 0; i < numLayers; ++i)
				{
					MapChunkLayerDef def;
					if (!(ptr->read(&def, sizeof(MapChunkLayerDef))))
					{
						ELOG("Could not read MCLY sub chunk");
						ptr->seek(endPos);
						return false;
					}

					if (def.flags & 0x200)
					{
						WLOG("Tile " << header.IndexX << " x " << header.IndexY << " uses compressed alpha layer");
					}

					// Add texture id
					tileTextures.push_back(def.textureId);
				}

				ptr->seek(endPos);
				return true;
			}

			static bool readMCALSubChunk(adt::Page &page, const Ogre::DataStreamPtr &ptr, UInt32 chunkSize, const MCNKHeader &header)
			{
				assert(header.IndexX < constants::TilesPerPage);
				assert(header.IndexY < constants::TilesPerPage);

				size_t endPos = ptr->tell() + chunkSize;

				UInt32 numLayers = chunkSize / 2048;
				if (numLayers > 3)
				{
					ptr->seek(endPos);
					return true;
				}

				// Calculate tile index
				UInt32 tileIndex = header.IndexY + header.IndexX * constants::TilesPerPage;
				auto &alphaMaps = page.terrain.alphaMaps[tileIndex];

				for (UInt32 i = 0; i < numLayers; ++i)
				{
					std::array<Int8, 64 * 32> alpha;
					if (!(ptr->read(&alpha, alpha.size())))
					{
						ELOG("Could not read MCAL sub chunk");
						ptr->seek(endPos);
						return false;
					}

					// Convert alpha map
					terrain::model::AlphaMap map;
					UInt32 p = 0, p2 = 0;
					for (int j = 0; j < 64; j++)
					{
						for (int i = 0; i < 32; i++)
						{
							UInt8 c = alpha[p2++];
							map[p++] = (c & 0x0f) << 4;
							map[p++] = (c & 0xf0);
						}
					}

					alphaMaps.push_back(map);
				}

				ptr->seek(endPos);
				return true;
			}

			static bool readMCNKChunk(adt::Page &page, const Ogre::DataStreamPtr &ptr, UInt32 chunkSize)
			{
				// Read the chunk header
				MCNKHeader header;
				if (!ptr->read(&header, sizeof(MCNKHeader)))
				{
					ELOG("Could not read MCNK chunk header!");
					return false;
				}

				// From here on, we will read subchunks
				const size_t subStart = ptr->tell();
				while (ptr->tell() < subStart + chunkSize)
				{
					// Read header of sub chunk
					UInt32 subHeader = 0, subSize = 0;
					bool result = (
						ptr->read(&subHeader, sizeof(UInt32)) &&
						ptr->read(&subSize, sizeof(UInt32)));
					if (!result)
					{
						return true;
					}

					switch (subHeader)
					{
						case MCVTSubChunk:
						{
							result = readMCVTSubChunk(page, ptr, subSize, header);
							break;
						}

						case MCNRSubChunk:
						{
                            result = readMCNRSubChunk(page, ptr, subSize, header);
                            break;
						}
                            
                        case MCSHSubChunk:
                        {
                            if (subSize != 512)
                                WLOG("\tMCSH sub chunk has different size than expected: " << subSize << " (expected 512)");
                            ptr->skip(subSize);
                            break;
                        }
                            
                        case MCALSubChunk:
                        {
							result = readMCALSubChunk(page, ptr, subSize, header);
                            break;
                        }
                            
                        case MCLYSubChunk:
                        {
							result = readMCLYSubChunk(page, ptr, subSize, header);
                            break;
                        }

						default:
						{
                            if (subStart + subSize >= ptr->size())
                            {
                                //TODO: Something is wrong here...
                                return true;
                            }
                            
                            ptr->skip(subSize);
                            break;
						}
					}

					if (!result)
					{
						WLOG("Error reading subchunk: " << subHeader);
						return false;
					}
				}

				return true;
			}
		}

		void load(const Ogre::DataStreamPtr &file, adt::Page &out_page)
		{
            std::array<read::MCINEntry, 16*16> MCINEntries;
            
			// Chunk data
			UInt32 chunkHeader = 0, chunkSize = 0;

			// Read all chunks until end of file reached
			while (!(file->eof()))
			{
				// Reset chunk data
				chunkHeader = 0;
				chunkSize = 0;

				// Read chunk header
				if (!(file->read(&chunkHeader, sizeof(UInt32))) ||
					chunkHeader == 0)
				{
					WLOG("Could not read chunk header");
					break;
				}

				// Read chunk size
				if (!(file->read(&chunkSize, sizeof(UInt32))))
				{
					WLOG("Could not read chunk size for chunk " << chunkHeader);
					break;
				}

				// Check chunk header
				bool result = true;
				if (chunkSize > 0)
				{
					switch (chunkHeader)
					{
						case MVERChunk:
						{
							result = read::readMVERChunk(out_page, file, chunkSize);
							break;
						}

						case MHDRChunk:
						{
							result = read::readMHDRChunk(out_page, file, chunkSize);
							break;
						}

						case MCINChunk:
						{
							if (chunkSize != 4096)
								WLOG("MCIN chunk size is not 4096, but " << chunkSize);
							if (!file->read(&MCINEntries[0], sizeof(read::MCINEntry) * MCINEntries.size()))
							{
								result = false;
							}
							break;
						}

						case MTEXChunk:
						{
							result = read::readMTEXChunk(out_page, file, chunkSize);
							break;
						}

						case MMIDChunk:
						{
							result = read::readMMIDChunk(out_page, file, chunkSize);
							break;
						}

						case MMDXChunk:
						{
							result = read::readMMDXChunk(out_page, file, chunkSize);
							break;
						}

						case MDDFChunk:
						{
							result = read::readMDDFChunk(out_page, file, chunkSize);
							break;
						}

						case MWMOChunk:
						case MODFChunk:
						case MH2OChunk:
						case MFBOChunk:
						case MTXPChunk:
						case MTXFChunk:
						case MWIDChunk:
						case MCNKChunk:
						{
							// We don't want to handle these, but we don't want warnings either
							file->skip(chunkSize);
							break;
						}

						default:
						{
							// Skip chunk data
							//WLOG("Undefined map chunk: " << chunkHeader << " (" << chunkSize << ")");
							file->skip(chunkSize);
							break;
						}
					}
				}
                
				// Something failed
				if (!result)
				{
					ELOG("Could not load ADT file: CHUNK " << chunkHeader << " (SIZE " << chunkSize << ") FAILED");
					//file->close();
					break;
				}
			}
            
            for (auto &entry : MCINEntries)
            {
                file->seek(entry.offsMCNK + 8);
				if (!read::readMCNKChunk(out_page, file, entry.size))
				{
					WLOG("Could not read MCNK chunk");
				}
            }
		}

	}
}
