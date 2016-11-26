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

#include "deps/debug_utils/DebugDraw.h"
#include "scene_node_ptr.h"
#include "manual_object_ptr.h"

namespace Ogre
{
	class ColourValue;
}

namespace wowpp
{
	namespace math
	{
		struct Vector3;
	}

	/// This class implements detours debug drawer interface and translates this into Ogre3D.
	class OgreDebugDraw : public duDebugDraw
	{
	public:

		/// Default constructor.
		/// @param sceneManager The scene manager which renders everything.
		OgreDebugDraw(Ogre::SceneManager &sceneManager);

		void clear();

		/// @copydoc duDebugDraw::depthMask(bool state)
		virtual void depthMask(bool state) override {}
		/// @copydoc duDebugDraw::texture(bool state)
		virtual void texture(bool state) override {}

		/// @copydoc duDebugDraw::begin(duDebugDrawPrimitives prim, float size = 1.0f)
		virtual void begin(duDebugDrawPrimitives prim, float size = 1.0f) override;
		/// @copydoc duDebugDraw::vertex(const float* pos, unsigned int color)
		virtual void vertex(const float* pos, unsigned int color) override;
		/// @copydoc duDebugDraw::vertex(const float x, const float y, const float z, unsigned int color)
		virtual void vertex(const float x, const float y, const float z, unsigned int color) override;
		/// @copydoc duDebugDraw::vertex(const float* pos, unsigned int color, const float* uv)
		virtual void vertex(const float* pos, unsigned int color, const float* uv) override;
		/// @copydoc duDebugDraw::vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v)
		virtual void vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v) override;
		/// @copydoc duDebugDraw::end()
		virtual void end() override;

	private:

		Ogre::SceneManager &m_manager;
		ogre_utils::SceneNodePtr m_node;
		ogre_utils::ManualObjectPtr m_object;
		std::vector<math::Vector3> m_vertices;
		std::vector<Ogre::ColourValue> m_colors;
		duDebugDrawPrimitives m_type;
	};
}
