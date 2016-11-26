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
#include "debug_drawer.h"
#include "game/map.h"
#include "log/default_log_levels.h"
#include "DebugDraw.h"

namespace wowpp
{
	OgreDebugDraw::OgreDebugDraw(Ogre::SceneManager & sceneManager)
		: m_manager(sceneManager)
		, m_type(DU_DRAW_POINTS)
	{
		m_node.reset(m_manager.getRootSceneNode()->createChildSceneNode());
		m_object.reset(m_manager.createManualObject());
		m_node->attachObject(m_object.get());

		m_vertices.reserve(4);
		m_colors.reserve(4);
	}
	void OgreDebugDraw::clear()
	{
		m_object->clear();
		m_vertices.clear();
		m_colors.clear();
	}
	void OgreDebugDraw::begin(duDebugDrawPrimitives prim, float size)
	{
		m_type = prim;

		Ogre::RenderOperation::OperationType ot;
		switch (prim)
		{
			case DU_DRAW_LINES: ot = Ogre::RenderOperation::OT_LINE_LIST;		break;
			case DU_DRAW_POINTS: ot = Ogre::RenderOperation::OT_POINT_LIST;		break;
			case DU_DRAW_TRIS: ot = Ogre::RenderOperation::OT_TRIANGLE_LIST;	break;
			case DU_DRAW_QUADS: ot = Ogre::RenderOperation::OT_TRIANGLE_LIST;	break;
			default:
				ot = Ogre::RenderOperation::OT_POINT_LIST;
				break;
		}
		m_object->begin("Editor/DetourDebug", ot);
	}
	void OgreDebugDraw::vertex(const float * pos, unsigned int color)
	{
		vertex(pos[0], pos[1], pos[2], color);
	}
	void OgreDebugDraw::vertex(const float x, const float y, const float z, unsigned int color)
	{
		Vertex v = recastToWoWCoord(Vertex(x, y, z));

		Ogre::ColourValue c;
		duIntToCol(color, &c.r);

		switch (m_type)
		{
			case DU_DRAW_LINES:
			case DU_DRAW_POINTS:
			case DU_DRAW_TRIS:
				m_object->position(v.x, v.y, v.z);
				m_object->colour(c);
				break;
			case DU_DRAW_QUADS:
			{
				// Append to the list
				m_vertices.push_back(std::move(v));
				m_colors.push_back(std::move(c));

				if (m_vertices.size() == 4)
				{
					// Append data
					m_object->position(m_vertices[0].x, m_vertices[0].y, m_vertices[0].z);
					m_object->colour(m_colors[0]);
					m_object->position(m_vertices[1].x, m_vertices[1].y, m_vertices[1].z);
					m_object->colour(m_colors[1]);
					m_object->position(m_vertices[2].x, m_vertices[2].y, m_vertices[2].z);
					m_object->colour(m_colors[2]);

					m_object->position(m_vertices[0].x, m_vertices[0].y, m_vertices[0].z);
					m_object->colour(m_colors[0]);
					m_object->position(m_vertices[2].x, m_vertices[2].y, m_vertices[2].z);
					m_object->colour(m_colors[2]);
					m_object->position(m_vertices[3].x, m_vertices[3].y, m_vertices[3].z);
					m_object->colour(m_colors[3]);

					// Empty list
					m_vertices.clear();
					m_colors.clear();
				}

				break;
			}
		}
	}
	void OgreDebugDraw::vertex(const float * pos, unsigned int color, const float * uv)
	{
		vertex(pos[0], pos[1], pos[2], color);
	}
	void OgreDebugDraw::vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v)
	{
		vertex(x, y, z, color);
	}
	void OgreDebugDraw::end()
	{
		m_object->end();
	}
}
