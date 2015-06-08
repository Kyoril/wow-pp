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
#include <QCursor>

namespace wowpp
{
	namespace editor
	{
		CameraController::CameraController(Ogre::Camera &camera)
			: m_camera(camera)
			, m_rmbDown(false)
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
					m_direction.z = -1.0f;
					break;
				}

				case Qt::Key_S:
				{
					m_direction.z = 1.0f;
					break;
				}

				case Qt::Key_A:
				{
					m_direction.x = -1.0f;
					break;
				}

				case Qt::Key_D:
				{
					m_direction.x = 1.0f;
					break;
				}
                    
                case Qt::Key_Q:
                {
                    m_direction.y = -1.0f;
                    break;
                }
                    
                case Qt::Key_E:
                {
                    m_direction.y = 1.0f;
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
					m_direction.z = 0.0f;
					break;
				}

				case Qt::Key_A:
				case Qt::Key_D:
				{
					m_direction.x = 0.0f;
					break;
				}
                    
                case Qt::Key_Q:
                case Qt::Key_E:
                {
                    m_direction.y = 0.0f;
                    break;
                }
			}
		}

		void CameraController::onMousePressed(QMouseEvent *mouseEvent)
		{
#ifndef __MACH__
			if (mouseEvent->button() & Qt::RightButton)
#else
            if (mouseEvent->button() & Qt::RightButton ||
                mouseEvent->button() & Qt::LeftButton)
#endif
			{
				m_rmbDown = true;
				m_lastMouse = mouseEvent->globalPos();
			}
		}

		void CameraController::onMouseMoved(QMouseEvent *mouseEvent)
		{
			if (m_rmbDown)
			{
				QPoint diff = m_lastMouse - mouseEvent->globalPos();
				QCursor::setPos(m_lastMouse);

				m_yaw = Ogre::Radian(static_cast<Ogre::Real>(diff.x()) * 0.025f);
				m_pitch = Ogre::Radian(static_cast<Ogre::Real>(diff.y()) * 0.025f);
				m_camera.yaw(m_yaw);
				m_camera.pitch(m_pitch);
			}
		}

		void CameraController::onMouseReleased(QMouseEvent *mouseEvent)
		{
#ifndef __MACH__
            if (mouseEvent->button() & Qt::RightButton)
#else
            if (mouseEvent->button() & Qt::RightButton ||
                mouseEvent->button() & Qt::LeftButton)
#endif
			{
				m_direction = Ogre::Vector3::ZERO;
				m_rmbDown = false;
			}
		}

		void CameraController::update(float dt)
		{
			if (m_rmbDown)
			{
				m_yaw = Ogre::Radian(0.0f);
				m_pitch = Ogre::Radian(0.0f);

				// Move the camera
				if (m_direction != Ogre::Vector3::ZERO)
				{
					m_camera.moveRelative(m_direction.normalisedCopy() * 33.3333f * dt);
				}
			}
		}

	}
}