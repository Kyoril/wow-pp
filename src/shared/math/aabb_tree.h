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

// 
// Part of this code taken from namreeb's (https://github.com/namreeb) and tripleslash's (https://github.com/tripleslash)
// nav lib project, to allow units to walk on terrain of any slope angle.
// 

#pragma once

#include "bounding_box.h"

namespace io
{
	class Reader;
	class Writer;
}

namespace wowpp
{
	namespace math
	{
		struct Ray;

		namespace raycast_flags
		{
			enum Type
			{
				None			= 0,
				EarlyExit		= 1,
				IgnoreBackface	= 2
			};
		}

		typedef raycast_flags::Type RaycastFlags;


		class AABBTree final
		{
			friend io::Writer &operator << (io::Writer &w, AABBTree const &tree);
			friend io::Reader &operator >> (io::Reader &r, AABBTree &tree);

		public:

			/// Represents a vertex in the tree
			typedef Vector3 Vertex;
			typedef std::uint32_t Index;

		private:

			/// 
			struct Node
			{
				union
				{
					/// 
					std::uint32_t children = 0;
					/// 
					std::uint32_t startFace;
				};

				/// 
				unsigned int numFaces = 0;
				/// 
				BoundingBox bounds;
			};

		public:
			
			/// Default constructor.
			AABBTree() = default;
			/// Destructor
			~AABBTree() = default;
			/// Initializes an AABBTree and builds it using the provided vertices and indices.
			/// @param verts Vertices to build.
			/// @param indices Indices to build.
			AABBTree(const std::vector<Vertex>& verts, const std::vector<Index>& indices);

			/// 
			/// @param verts
			/// @param indices
			void build(const std::vector<Vertex>& verts, const std::vector<Index>& indices);
			/// 
			/// @param ray 
			/// @param faceIndex
			/// @returns 
			bool intersectRay(Ray& ray, Index* faceIndex = nullptr, RaycastFlags flags = raycast_flags::None) const;
			/// Gets the bounding box of this tree.
			/// @returns Bounding box of this tree.
			BoundingBox getBoundingBox() const;

			const std::vector<Node> &getNodes() const { return m_nodes; }
			const std::vector<Vertex> &getVertices() const { return m_vertices; }
			const std::vector<Index> &getIndices() const { return m_indices; }

		private:

			/// 
			/// @param node
			/// @param faces
			/// @param numFaces
			/// @returns 
			unsigned int partitionMedian(Node& node, Index* faces, unsigned int numFaces);
			/// 
			/// @param node
			/// @param faces
			/// @param numFaces
			/// @returns 
			unsigned int partitionSurfaceArea(Node& node, Index* faces, unsigned int numFaces);
			/// 
			/// @param nodeIndex
			/// @param faces
			/// @param numFaces
			void buildRecursive(unsigned int nodeIndex, Index* faces, unsigned int numFaces);
			/// 
			/// @param faces
			/// @param numFaces
			/// @returns 
			BoundingBox calculateFaceBounds(Index* faces, unsigned int numFaces) const;
			/// 
			/// @param ray
			/// @param faceIndex
			void trace(Ray& ray, Index* faceIndex, RaycastFlags flags) const;
			/// 
			/// @param nodeIndex
			/// @param ray
			/// @param faceIndex
			void traceRecursive(unsigned int nodeIndex, Ray& ray, Index* faceIndex, RaycastFlags flags) const;
			/// 
			/// @param node
			/// @param ray
			/// @param faceIndex
			void traceInnerNode(const Node& node, Ray& ray, Index* faceIndex, RaycastFlags flags) const;
			/// 
			/// @param node
			/// @param ray
			/// @param faceIndex
			bool traceLeafNode(const Node& node, Ray& ray, Index* faceIndex, RaycastFlags flags) const;

		private:

			/// 
			/// @param v
			/// @returns 
			static unsigned int getLongestAxis(const Vector3& v);

		private:

			unsigned int m_freeNode = 0;
			std::vector<Node> m_nodes;
			std::vector<Vertex> m_vertices;
			std::vector<Index> m_indices;
			std::vector<BoundingBox> m_faceBounds;
			std::vector<unsigned int> m_faceIndices;
		};

		io::Writer &operator << (io::Writer &w, AABBTree const &tree);
		io::Reader &operator >> (io::Reader &r, AABBTree &tree);
	}
}
