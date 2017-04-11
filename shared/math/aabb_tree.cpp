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

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstddef>
#include <ostream>
#include <cassert>
#include <vector>
#include <algorithm>
#include "aabb_tree.h"
#include "ray.h"
#include "binary_io/reader.h"
#include "binary_io/writer.h"


namespace wowpp
{
	namespace math
	{
		namespace 
		{
			class ModelFaceSorter 
			{
			public:
				ModelFaceSorter(const AABBTree::Vertex* vertices, const AABBTree::Index* indices, unsigned int axis)
					: m_vertices(vertices)
					, m_indices(indices)
					, m_axis(axis)
				{
				}

				bool operator ()(unsigned int lhs, unsigned int rhs) const
				{
					float a = getCenteroid(lhs);
					float b = getCenteroid(rhs);
					return (a != b) ? (a < b) : (lhs < rhs);
				}

				float getCenteroid(unsigned int face) const
				{
					auto& a = m_vertices[m_indices[face * 3 + 0]];
					auto& b = m_vertices[m_indices[face * 3 + 1]];
					auto& c = m_vertices[m_indices[face * 3 + 2]];
					return (a[m_axis] + b[m_axis] + c[m_axis]) / 3.0f;
				}

			private:
				const AABBTree::Vertex* m_vertices;
				const AABBTree::Index* m_indices;
				unsigned int m_axis;
			};
		}

		AABBTree::AABBTree(const std::vector<Vertex>& verts, const std::vector<Index>& indices)
		{
			build(verts, indices);
		}

		void AABBTree::build(const std::vector<Vertex>& verts, const std::vector<Index>& indices)
		{
			m_vertices = verts;
			m_indices = indices;

			m_faceBounds.clear();
			m_faceIndices.clear();

			size_t numFaces = indices.size() / 3;

			m_faceBounds.reserve(numFaces);
			m_faceIndices.reserve(numFaces);

			for (unsigned int i = 0; i < (unsigned int)numFaces; ++i)
			{
				m_faceIndices.push_back(i);
				m_faceBounds.push_back(calculateFaceBounds(&i, 1));
			}

			m_freeNode = 1;
			m_nodes.clear();
			m_nodes.reserve(int(numFaces * 1.5f));

			buildRecursive(0, m_faceIndices.data(), (unsigned int)numFaces);
			m_faceBounds.clear();

			// Reorder the model indices according to the face indices
			std::vector<Index> sortedIndices(m_indices.size());
			for (size_t i = 0; i < numFaces; ++i)
			{
				unsigned int index = m_faceIndices[i] * 3;
				sortedIndices[i * 3 + 0] = m_indices[index + 0];
				sortedIndices[i * 3 + 1] = m_indices[index + 1];
				sortedIndices[i * 3 + 2] = m_indices[index + 2];
			}

			m_indices.swap(sortedIndices);
			m_faceIndices.clear();
		}

		bool AABBTree::intersectRay(Ray& ray, Index* faceIndex/* = nullptr*/, RaycastFlags flags/* = raycast_flags::None*/) const
		{
			float distance = ray.hitDistance;
			trace(ray, faceIndex, flags);
			return ray.hitDistance < distance;
		}

		BoundingBox AABBTree::getBoundingBox() const
		{
			if (m_nodes.empty())
				return BoundingBox{};
			return m_nodes.front().bounds;
		}

		unsigned int AABBTree::partitionMedian(Node& node, Index* faces, unsigned int numFaces)
		{
			unsigned int axis = getLongestAxis(node.bounds.getSize());
			ModelFaceSorter predicate(m_vertices.data(), m_indices.data(), axis);

			std::nth_element(faces, faces + numFaces / 2, faces + numFaces, predicate);
			return numFaces / 2;
		}

		unsigned int AABBTree::partitionSurfaceArea(Node& node, Index* faces, unsigned int numFaces)
		{
			unsigned int bestAxis = 0;
			unsigned int bestIndex = 0;

			float bestCost = std::numeric_limits<float>::max();

			for (unsigned int axis = 0; axis < 3; ++axis)
			{
				ModelFaceSorter predicate(m_vertices.data(), m_indices.data(), axis);
				std::sort(faces, faces + numFaces, predicate);

				// Two passes over data to calculate upper and lower bounds
				std::vector<float> cumulativeLower(numFaces);
				std::vector<float> cumulativeUpper(numFaces);

				BoundingBox lower, upper;

				for (unsigned int i = 0; i < numFaces; ++i)
				{
					lower.combine(m_faceBounds[faces[i]]);
					upper.combine(m_faceBounds[faces[numFaces - i - 1]]);

					cumulativeLower[i] = lower.getSurfaceArea();
					cumulativeUpper[numFaces - i - 1] = upper.getSurfaceArea();
				}

				float invTotalArea = 1.0f / cumulativeUpper[0];

				// Test split positions
				for (unsigned int i = 0; i < numFaces - 1; ++i)
				{
					float below = cumulativeLower[i] * invTotalArea;
					float above = cumulativeUpper[i] * invTotalArea;

					float cost = 0.125f + (below * i + above * (numFaces - i));
					if (cost <= bestCost)
					{
						bestCost = cost;
						bestIndex = i;
						bestAxis = axis;
					}
				}
			}

			// Sort faces by best axis
			ModelFaceSorter predicate(m_vertices.data(), m_indices.data(), bestAxis);
			std::sort(faces, faces + numFaces, predicate);

			return bestIndex + 1;
		}

		void AABBTree::buildRecursive(unsigned int nodeIndex, Index* faces, unsigned int numFaces)
		{
			const unsigned int maxFacesPerLeaf = 6;

			// Allocate more nodes if out of nodes
			if (nodeIndex >= m_nodes.size())
			{
				int size = std::max(int(1.5f * m_nodes.size()), 512);
				m_nodes.resize(size);
			}

			auto& node = m_nodes[nodeIndex];
			node.bounds = calculateFaceBounds(faces, numFaces);

			if (numFaces <= maxFacesPerLeaf)
			{
				node.startFace = static_cast<std::uint32_t>(faces - m_faceIndices.data());
				assert(node.startFace == static_cast<std::size_t>(faces - m_faceIndices.data()));   // verify no truncation
				node.numFaces = numFaces;
			}
			else
			{
				unsigned int leftCount = partitionSurfaceArea(node, faces, numFaces);
				unsigned int rightCount = numFaces - leftCount;

				// Allocate 2 nodes
				node.children = m_freeNode;
				m_freeNode += 2;

				// Split faces in half and build each side recursively
				buildRecursive(node.children + 0, faces, leftCount);
				buildRecursive(node.children + 1, faces + leftCount, rightCount);
			}
		}

		BoundingBox AABBTree::calculateFaceBounds(Index* faces, unsigned int numFaces) const
		{
			float min = std::numeric_limits<float>::lowest();
			float max = std::numeric_limits<float>::max();

			Vector3 minExtents = { max, max, max };
			Vector3 maxExtents = { min, min, min };

			for (unsigned int i = 0; i < numFaces; ++i)
			{
				auto& v0 = m_vertices[m_indices[faces[i] * 3 + 0]];
				auto& v1 = m_vertices[m_indices[faces[i] * 3 + 1]];
				auto& v2 = m_vertices[m_indices[faces[i] * 3 + 2]];

				minExtents = takeMinimum(minExtents, v0);
				maxExtents = takeMaximum(maxExtents, v0);

				minExtents = takeMinimum(minExtents, v1);
				maxExtents = takeMaximum(maxExtents, v1);

				minExtents = takeMinimum(minExtents, v2);
				maxExtents = takeMaximum(maxExtents, v2);
			}

			return BoundingBox(minExtents, maxExtents);
		}

		void AABBTree::trace(Ray& ray, Index* faceIndex, RaycastFlags flags) const
		{
			if (m_indices.empty())
				return;

			struct StackEntry
			{
				unsigned int node;
				float dist;
			};

			StackEntry stack[50];
			stack[0].node = 0;
			stack[0].dist = 0.0f;

			float max = std::numeric_limits<float>::max();

			unsigned int stackCount = 1;
			while (!!stackCount)
			{
				// Pop node from back
				StackEntry& e = stack[--stackCount];

				// Ignore if another node has already come closer
				if (e.dist >= ray.hitDistance)
					continue;

				const Node& node = m_nodes.at(e.node);
				if (!node.numFaces)
				{
					// Find closest node
					auto& leftChild = m_nodes.at(node.children + 0);
					auto& rightChild = m_nodes.at(node.children + 1);

					float dist[2] = { max, max };
					auto result1 = ray.intersectsAABB(leftChild.bounds);
					if (result1.first) dist[0] = result1.second;
					auto result2 = ray.intersectsAABB(rightChild.bounds);
					if (result2.first) dist[1] = result2.second;

					unsigned int closest = dist[1] < dist[0]; // 0 or 1
					unsigned int furthest = closest ^ 1;

					if (dist[furthest] < ray.hitDistance)
					{
						StackEntry& n = stack[stackCount++];
						n.node = node.children + furthest;
						n.dist = dist[furthest];
					}

					if (dist[closest] < ray.hitDistance)
					{
						StackEntry& n = stack[stackCount++];
						n.node = node.children + closest;
						n.dist = dist[closest];
					}
				}
				else
					if (traceLeafNode(node, ray, faceIndex, flags))
						return;
			}
		}

		void AABBTree::traceRecursive(unsigned int nodeIndex, Ray& ray, Index* faceIndex, RaycastFlags flags) const
		{
			auto& node = m_nodes.at(nodeIndex);
			if (node.numFaces != 0)
				if (traceLeafNode(node, ray, faceIndex, flags))
					return;
			else
				traceInnerNode(node, ray, faceIndex, flags);
		}

		void AABBTree::traceInnerNode(const Node& node, Ray& ray, Index* faceIndex, RaycastFlags flags) const
		{
			auto& leftChild = m_nodes.at(node.children + 0);
			auto& rightChild = m_nodes.at(node.children + 1);

			float max = std::numeric_limits<float>::max();
			float distance[2] = { max, max };

			auto result1 = ray.intersectsAABB(leftChild.bounds);
			if (result1.first) distance[0] = result1.second;
			auto result2 = ray.intersectsAABB(rightChild.bounds);
			if (result2.first) distance[1] = result2.second;

			unsigned int closest = 0;
			unsigned int furthest = 1;

			if (distance[1] < distance[0])
				std::swap(closest, furthest);

			if (distance[closest] < ray.hitDistance)
				traceRecursive(node.children + closest, ray, faceIndex, flags);

			if (distance[furthest] < ray.hitDistance)
				traceRecursive(node.children + furthest, ray, faceIndex, flags);
		}

		bool AABBTree::traceLeafNode(const Node& node, Ray& ray, Index* faceIndex, RaycastFlags flags) const
		{
			for (auto i = node.startFace; i < node.startFace + node.numFaces; ++i)
			{
				auto& v0 = m_vertices[m_indices[i * 3 + 0]];
				auto& v1 = m_vertices[m_indices[i * 3 + 1]];
				auto& v2 = m_vertices[m_indices[i * 3 + 2]];

				auto result = ray.intersectsTriangle(v0, v1, v2, (flags & raycast_flags::IgnoreBackface) != 0);
				if (!result.first)
					continue;

				if (result.second < ray.hitDistance)
				{
					ray.hitDistance = result.second;
					if (faceIndex)
						*faceIndex = i;

					if (flags & raycast_flags::EarlyExit)
						return true;
				}
			}

			return false;
		}

		unsigned int AABBTree::getLongestAxis(const Vector3& v)
		{
			if (v.x > v.y && v.x > v.z)
				return 0;

			return (v.y > v.z) ? 1 : 2;
		}

		io::Writer &operator << (io::Writer &w, AABBTree const &tree)
		{
			const UInt32 magic = 'BVH1';
			w << io::write<UInt32>(magic);

			// Write vertices
			w << io::write_dynamic_range<UInt32>(tree.m_vertices);

			// Write indices
			w << io::write_dynamic_range<UInt32>(tree.m_indices);

			// Write nodes
			w << io::write<UInt32>(tree.m_nodes.size());
			for (const auto& node : tree.m_nodes)
			{
				assert(node.numFaces < 8);

				const std::uint8_t numFaces = static_cast<std::uint8_t>(node.numFaces);
				w << io::write<UInt8>(node.numFaces);
				if (node.numFaces != 0)
				{
					// Write leaf node
					w 
						<< io::write<UInt32>(node.startFace)
						<< node.bounds;
				}
				else
				{
					// Write inner node
					w 
						<< io::write<UInt32>(node.children)
						<< node.bounds;
				}
			}

			const UInt32 endMagic = 'FOOB';
			w << io::write<UInt32>(endMagic);

			return w;
		}

		io::Reader &operator >> (io::Reader &r, AABBTree &tree)
		{
			// Map magic
			UInt32 magic;
			r >> io::read<UInt32>(magic);
			if (magic != (UInt32)'BVH1')
			{
				r.setFailure();
				return r;
			}

			// Read vertices
			r >> io::read_container<UInt32>(tree.m_vertices);

			// Read indices
			r >> io::read_container<UInt32>(tree.m_indices);

			// Read nodes
			UInt32 nodeCount = 0;
			r >> io::read<UInt32>(nodeCount);

			tree.m_nodes.resize(nodeCount);
			for (UInt32 i = 0; i < nodeCount; ++i)
			{
				UInt8 numFaces = 0;
				r >> io::read<UInt8>(numFaces);

				tree.m_nodes[i].numFaces = static_cast<unsigned int>(numFaces);
				if (numFaces != 0)
				{
					// Read leaf node
					r 
						>> io::read<UInt32>(tree.m_nodes[i].startFace)
						>> tree.m_nodes[i].bounds;
				}
				else
				{
					// Read inner node
					r
						>> io::read<UInt32>(tree.m_nodes[i].children)
						>> tree.m_nodes[i].bounds;
				}
			}

			UInt32 endMagic;
			r >> io::read<UInt32>(endMagic);
			if (endMagic != (UInt32)'FOOB')
			{
				r.setFailure();
				return r;
			}

			return r;
		}
	}
}
