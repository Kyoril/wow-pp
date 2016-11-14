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

namespace wowpp
{
	OgreDebugDraw::OgreDebugDraw(Ogre::SceneManager & sceneManager)
		: m_manager(sceneManager)
	{
		m_node.reset(m_manager.getRootSceneNode()->createChildSceneNode());
		m_object.reset(m_manager.createManualObject());
	}
	void OgreDebugDraw::begin(duDebugDrawPrimitives prim, float size)
	{
		Ogre::RenderOperation::OperationType ot;
		switch (prim)
		{
			case DU_DRAW_LINES: ot = Ogre::RenderOperation::OT_LINE_STRIP;		break;
			case DU_DRAW_POINTS: ot = Ogre::RenderOperation::OT_POINT_LIST;		break;
			case DU_DRAW_TRIS: ot = Ogre::RenderOperation::OT_TRIANGLE_LIST;	break;
			case DU_DRAW_QUADS: ot = Ogre::RenderOperation::OT_TRIANGLE_LIST;	break;
			default:
				ot = Ogre::RenderOperation::OT_TRIANGLE_LIST;
				break;
		}
		m_object->begin("", ot);
	}
	void OgreDebugDraw::vertex(const float * pos, unsigned int color)
	{
		m_object->position(pos[0], pos[1], pos[2]);
	}
	void OgreDebugDraw::vertex(const float x, const float y, const float z, unsigned int color)
	{
		m_object->position(x, y, z);
	}
	void OgreDebugDraw::vertex(const float * pos, unsigned int color, const float * uv)
	{
		m_object->position(pos[0], pos[1], pos[2]);
	}
	void OgreDebugDraw::vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v)
	{
		m_object->position(x, y, z);
	}
	void OgreDebugDraw::end()
	{
		m_object->end();
	}
}
