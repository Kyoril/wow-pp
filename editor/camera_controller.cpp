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

#include "camera_controller.h"
#include "OgreCamera.h"

namespace wowpp
{
	namespace editor
	{
		CameraController::CameraController(Ogre::Camera &camera)
			: m_camera(camera)
			, m_direction(0.0f, 0.0f, 0.0f)
		{
		}

		void CameraController::setCameraPosition(const Ogre::Vector3 &position)
		{
			m_camera.setPosition(position);
		}

		void CameraController::setCameraRotation(const Ogre::Quaternion &orientation)
		{
			m_camera.setOrientation(orientation);
		}

		void CameraController::onKeyPressed(QKeyEvent *keyEvent)
		{
			switch (keyEvent->key())
			{
				case Qt::Key_W:
				{
					m_direction.y = 1.0f;
					break;
				}

				case Qt::Key_S:
				{
					m_direction.y = -1.0f;
					break;
				}

				case Qt::Key_A:
				{
					m_direction.x = 1.0f;
					break;
				}

				case Qt::Key_D:
				{
					m_direction.x = -1.0f;
					break;
				}
			}
		}

		void CameraController::onKeyReleased(QKeyEvent *keyEvent)
		{
			switch (keyEvent->key())
			{
				case Qt::Key_W:
				case Qt::Key_S:
				{
					m_direction.y = 0.0f;
					break;
				}

				case Qt::Key_A:
				case Qt::Key_D:
				{
					m_direction.x = 0.0f;
					break;
				}
			}
		}

		void CameraController::onMousePressed(QMouseEvent *mouseEvent)
		{

		}

		void CameraController::onMouseMoved(QMouseEvent *mouseEvent)
		{

		}

		void CameraController::onMouseReleased(QMouseEvent *mouseEvent)
		{

		}

		void CameraController::update(float dt)
		{
			// Move the camera
			if (m_direction != Ogre::Vector3::ZERO)
			{
				m_camera.moveRelative(m_direction.normalisedCopy() * 7.0f * dt);
			}
		}

	}
}