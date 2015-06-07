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

#include <QMouseEvent>
#include <QKeyEvent>
#include "OgreVector3.h"

namespace Ogre
{
	class Camera;
	class Quaternion;
}

namespace wowpp
{
	namespace editor
	{
		/// 
		class CameraController
		{
		public:

			explicit CameraController(Ogre::Camera &camera);

			/// Sets the camera position to a fixed point in the world.
			virtual void setCameraPosition(const Ogre::Vector3 &position);
			/// Sets the camera rotation to a fixed value.
			virtual void setCameraRotation(const Ogre::Quaternion &orientation);
			/// 
			virtual void onKeyPressed(QKeyEvent *keyEvent);
			/// 
			virtual void onKeyReleased(QKeyEvent *keyEvent);
			/// 
			virtual void onMousePressed(QMouseEvent *mouseEvent);
			/// 
			virtual void onMouseMoved(QMouseEvent *mouseEvent);
			/// 
			virtual void onMouseReleased(QMouseEvent *mouseEvent);
			/// 
			virtual void update(float dt);

		private:

			Ogre::Camera &m_camera;
			Ogre::Vector3 m_direction;
		};
	}
}
