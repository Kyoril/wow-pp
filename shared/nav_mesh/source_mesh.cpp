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

#include "source_mesh.h"

namespace wowpp
{
	namespace navigation
	{
		SourceMesh::SourceMesh()
		{
		}

		SourceMesh::SourceMesh(SourceMesh  &&other)
			: m_vertices(std::move(other.m_vertices))
			, m_indices(std::move(other.m_indices))
		{
		}

		SourceMesh &SourceMesh::operator = (SourceMesh && other)
		{
			if (this != &other)
			{
				swap(other);
			}
			return *this;
		}

		void SourceMesh::swap(SourceMesh &other)
		{
			m_vertices.swap(other.m_vertices);
			m_indices.swap(other.m_indices);
		}

		const VertexCoordinates &SourceMesh::getVertexCoordinates() const
		{
			return m_vertices;
		}

		const IndexBuffer &SourceMesh::getIndices() const
		{
			return m_indices;
		}

		void SourceMesh::addVertex(Coordinate x, Coordinate y, Coordinate z)
		{
			//TODO: consider exception-safety
			m_vertices.push_back(x);
			m_vertices.push_back(y);
			m_vertices.push_back(z);
		}

		void SourceMesh::addTriangle(VertexIndex v0, VertexIndex v1, VertexIndex v2)
		{
			//TODO: consider exception-safety
			m_indices.push_back(v0);
			m_indices.push_back(v1);
			m_indices.push_back(v2);
		}

		std::size_t SourceMesh::getVertexCount() const
		{
			return m_vertices.size() / 3;
		}

		std::size_t SourceMesh::getTriangleCount() const
		{
			return m_indices.size() / 3;
		}
	}
}
