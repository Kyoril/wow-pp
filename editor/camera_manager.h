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

#include "OgreCamera.h"
#include "OgreSceneNode.h"
#include "OgreFrameListener.h"
#include <QKeyEvent>
#include <QMouseEvent>

// enum CameraStyle should be in other namespace than OgreBites::CameraStyle
namespace OgreQtBites
{
	enum CameraStyle   // enumerator values for different styles of camera movement
	{
		CS_FREELOOK,
		CS_ORBIT,
		CS_MANUAL
	};

	/*=============================================================================
	| Utility class for controlling the camera in samples.
	=============================================================================*/
	class SdkQtCameraMan
	{
	public:
		SdkQtCameraMan(Ogre::Camera* cam)
			: mCamera(0)
			, mTarget(0)
			, mOrbiting(false)
			, mZooming(false)
			, mTopSpeed(15)
			, mVelocity(Ogre::Vector3::ZERO)
			, mGoingForward(false)
			, mGoingBack(false)
			, mGoingLeft(false)
			, mGoingRight(false)
			, mGoingUp(false)
			, mGoingDown(false)
			, mFastMove(false)
		{

			setCamera(cam);
			setStyle(CS_FREELOOK);
		}

		virtual ~SdkQtCameraMan() {}

		/*-----------------------------------------------------------------------------
		| Swaps the camera on our camera man for another camera.
		-----------------------------------------------------------------------------*/
		virtual void setCamera(Ogre::Camera* cam)
		{
			mCamera = cam;
		}

		virtual Ogre::Camera* getCamera()
		{
			return mCamera;
		}

		/*-----------------------------------------------------------------------------
		| Sets the target we will revolve around. Only applies for orbit style.
		-----------------------------------------------------------------------------*/
		virtual void setTarget(Ogre::SceneNode* target)
		{
			if (target != mTarget)
			{
				mTarget = target;
				if (target)
				{
					setYawPitchDist(Ogre::Degree(0), Ogre::Degree(15), 150);
					mCamera->setAutoTracking(true, mTarget);
				}
				else
				{
					mCamera->setAutoTracking(false);
				}

			}


		}

		virtual Ogre::SceneNode* getTarget()
		{
			return mTarget;
		}

		/*-----------------------------------------------------------------------------
		| Sets the spatial offset from the target. Only applies for orbit style.
		-----------------------------------------------------------------------------*/
		virtual void setYawPitchDist(Ogre::Radian yaw, Ogre::Radian pitch, Ogre::Real dist)
		{
			mCamera->setPosition(mTarget->_getDerivedPosition());
			mCamera->setOrientation(mTarget->_getDerivedOrientation());
			mCamera->yaw(yaw);
			mCamera->pitch(-pitch);
			mCamera->moveRelative(Ogre::Vector3(0, 0, dist));
		}

		/*-----------------------------------------------------------------------------
		| Sets the camera's top speed. Only applies for free-look style.
		-----------------------------------------------------------------------------*/
		virtual void setTopSpeed(Ogre::Real topSpeed)
		{
			mTopSpeed = topSpeed;
		}

		virtual Ogre::Real getTopSpeed()
		{
			return mTopSpeed;
		}

		/*-----------------------------------------------------------------------------
		| Sets the movement style of our camera man.
		-----------------------------------------------------------------------------*/
		virtual void setStyle(CameraStyle style)
		{
			if (mStyle != CS_ORBIT && style == CS_ORBIT)
			{
				setTarget(mTarget ? mTarget : mCamera->getSceneManager()->getRootSceneNode());
				mCamera->setFixedYawAxis(true);
				manualStop();
				setYawPitchDist(Ogre::Degree(0), Ogre::Degree(15), 150);
			}
			else if (mStyle != CS_FREELOOK && style == CS_FREELOOK)
			{
				mCamera->setAutoTracking(false);
				mCamera->setFixedYawAxis(true);
			}
			else if (mStyle != CS_MANUAL && style == CS_MANUAL)
			{
				mCamera->setAutoTracking(false);
				manualStop();
			}
			mStyle = style;

		}

		virtual CameraStyle getStyle()
		{
			return mStyle;
		}

		/*-----------------------------------------------------------------------------
		| Manually stops the camera when in free-look mode.
		-----------------------------------------------------------------------------*/
		virtual void manualStop()
		{
			if (mStyle == CS_FREELOOK)
			{
				mGoingForward = false;
				mGoingBack = false;
				mGoingLeft = false;
				mGoingRight = false;
				mGoingUp = false;
				mGoingDown = false;
				mVelocity = Ogre::Vector3::ZERO;
			}
		}

		virtual bool frameRenderingQueued(const Ogre::FrameEvent& evt)
		{
			if (mStyle == CS_FREELOOK)
			{
				// build our acceleration vector based on keyboard input composite
				Ogre::Vector3 accel = Ogre::Vector3::ZERO;
				if (mGoingForward) accel += mCamera->getDirection();
				if (mGoingBack) accel -= mCamera->getDirection();
				if (mGoingRight) accel += mCamera->getRight();
				if (mGoingLeft) accel -= mCamera->getRight();
				if (mGoingUp) accel += mCamera->getUp();
				if (mGoingDown) accel -= mCamera->getUp();

				// if accelerating, try to reach top speed in a certain time
				Ogre::Real topSpeed = mFastMove ? mTopSpeed * 10 : mTopSpeed;
				if (accel.squaredLength() != 0)
				{
					accel.normalise();
					mVelocity += accel * topSpeed * evt.timeSinceLastFrame * 10;
				}
				// if not accelerating, try to stop in a certain time
				else mVelocity -= mVelocity * evt.timeSinceLastFrame * 10;

				Ogre::Real tooSmall = std::numeric_limits<Ogre::Real>::epsilon();

				// keep camera velocity below top speed and above epsilon
				if (mVelocity.squaredLength() > topSpeed * topSpeed)
				{
					mVelocity.normalise();
					mVelocity *= topSpeed;
				}
				else if (mVelocity.squaredLength() < tooSmall * tooSmall)
					mVelocity = Ogre::Vector3::ZERO;

				if (mVelocity != Ogre::Vector3::ZERO) mCamera->move(mVelocity * evt.timeSinceLastFrame);
			}

			return true;
		}

		/*-----------------------------------------------------------------------------
		| Processes key presses for free-look style movement.
		-----------------------------------------------------------------------------*/
		virtual void injectKeyDown(const QKeyEvent& evt)
		{
			if (mStyle == CS_FREELOOK)
			{
				if (evt.key() == Qt::Key_W || evt.key() == Qt::Key_Up) mGoingForward = true;
				else if (evt.key() == Qt::Key_S || evt.key() == Qt::Key_Down) mGoingBack = true;
				else if (evt.key() == Qt::Key_A || evt.key() == Qt::Key_Left) mGoingLeft = true;
				else if (evt.key() == Qt::Key_D || evt.key() == Qt::Key_Right) mGoingRight = true;
				else if (evt.key() == Qt::Key_E) mGoingUp = true;
				else if (evt.key() == Qt::Key_Q) mGoingDown = true;
				else if (evt.key() == Qt::Key_Shift) mFastMove = true;
			}
		}

		/*-----------------------------------------------------------------------------
		| Processes key releases for free-look style movement.
		-----------------------------------------------------------------------------*/
		virtual void injectKeyUp(const QKeyEvent& evt)
		{
			if (mStyle == CS_FREELOOK)
			{
				if (evt.key() == Qt::Key_W || evt.key() == Qt::Key_Up) mGoingForward = false;
				else if (evt.key() == Qt::Key_S || evt.key() == Qt::Key_Down) mGoingBack = false;
				else if (evt.key() == Qt::Key_A || evt.key() == Qt::Key_Left) mGoingLeft = false;
				else if (evt.key() == Qt::Key_D || evt.key() == Qt::Key_Right) mGoingRight = false;
				else if (evt.key() == Qt::Key_E) mGoingUp = false;
				else if (evt.key() == Qt::Key_Q) mGoingDown = false;
				else if (evt.key() == Qt::Key_Shift) mFastMove = false;
			}
		}

		/*-----------------------------------------------------------------------------
		| Processes mouse movement differently for each style.
		-----------------------------------------------------------------------------*/
		virtual void injectMouseMove(int relX, int relY)
		{
			//            static int lastX = evt.x();
			//            static int lastY = evt.y();
			//            int relX = evt.x() - lastX;
			//            int relY = evt.y() - lastY;
			//            lastX = evt.x();
			//            lastY = evt.y();
			if (mStyle == CS_ORBIT)
			{
				Ogre::Real dist = (mCamera->getPosition() - mTarget->_getDerivedPosition()).length();

				if (mOrbiting)   // yaw around the target, and pitch locally
				{
					mCamera->setPosition(mTarget->_getDerivedPosition());

					mCamera->yaw(Ogre::Degree(-relX * 0.025f));
					mCamera->pitch(Ogre::Degree(-relY * 0.025f));

					mCamera->moveRelative(Ogre::Vector3(0, 0, dist));

					// don't let the camera go over the top or around the bottom of the target
				}
				else if (mZooming)  // move the camera toward or away from the target
				{
					// the further the camera is, the faster it moves
					mCamera->moveRelative(Ogre::Vector3(0, 0, relY * 0.004f * dist));
				}
			}
			else if (mStyle == CS_FREELOOK)
			{
				mCamera->yaw(Ogre::Degree(-relX * 0.15f));
				mCamera->pitch(Ogre::Degree(-relY * 0.15f));
			}
		}

		/*-----------------------------------------------------------------------------
		| Processes mouse movement differently for each style.
		-----------------------------------------------------------------------------*/
		virtual void injectWheelMove(const QWheelEvent& evt)
		{
			int relZ = evt.delta();
			if (mStyle == CS_ORBIT)
			{
				Ogre::Real dist = (mCamera->getPosition() - mTarget->_getDerivedPosition()).length();

				if (relZ != 0)  // move the camera toward or away from the target
				{
					// the further the camera is, the faster it moves
					mCamera->moveRelative(Ogre::Vector3(0, 0, -relZ * 0.0008f * dist));
				}
			}
		}

		/*-----------------------------------------------------------------------------
		| Processes mouse presses. Only applies for orbit style.
		| Left button is for orbiting, and right button is for zooming.
		-----------------------------------------------------------------------------*/
		virtual void injectMouseDown(const QMouseEvent& evt)
		{
			if (mStyle == CS_ORBIT)
			{
				if (evt.buttons() & Qt::LeftButton) mOrbiting = true;
				else if (evt.buttons() & Qt::RightButton) mZooming = true;
			}
		}

		/*-----------------------------------------------------------------------------
		| Processes mouse releases. Only applies for orbit style.
		| Left button is for orbiting, and right button is for zooming.
		-----------------------------------------------------------------------------*/
		virtual void injectMouseUp(const QMouseEvent& evt)
		{
			if (mStyle == CS_ORBIT)
			{
				if (evt.buttons() & Qt::LeftButton) mOrbiting = false;
				else if (evt.buttons() & Qt::RightButton) mZooming = false;
			}
		}

	protected:

		Ogre::Camera* mCamera;
		CameraStyle mStyle;
		Ogre::SceneNode* mTarget;
		bool mOrbiting;
		bool mZooming;
		Ogre::Real mTopSpeed;
		Ogre::Vector3 mVelocity;
		bool mGoingForward;
		bool mGoingBack;
		bool mGoingLeft;
		bool mGoingRight;
		bool mGoingUp;
		bool mGoingDown;
		bool mFastMove;
	};
}