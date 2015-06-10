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

#include "OgreRenderWindow.h"
#include "OgreCamera.h"
#include "OgreViewport.h"
#include "GL/glew.h"
#include <QtOpenGL/QGLWidget>
#include <QTimer>
#include "camera_controller.h"
#include "viewport_grid.h"
#include <memory>

namespace wowpp
{
	namespace editor
	{
		/// Represents the main window of the editor application.
		class RenderView : public QGLWidget
		{
			Q_OBJECT

		public:

			/// Initializes a new instance of the main window of the editor application.
			explicit RenderView(QWidget *Parent = nullptr);

			Ogre::Camera *getCamera() { return m_Camera; }

		protected:

			virtual void initializeGL() override;
			virtual void paintGL() override;
			virtual void resizeGL(int width, int height) override;
			virtual void mousePressEvent(QMouseEvent *mouseEvent) override;
			virtual void mouseMoveEvent(QMouseEvent *mouseEvent) override;
			virtual void mouseReleaseEvent(QMouseEvent *mouseEvent) override;
			virtual void keyPressEvent(QKeyEvent *keyEvent) override;
			virtual void keyReleaseEvent(QKeyEvent *keyEvent) override;

		private:

			Ogre::RenderWindow *m_OgreWindow;
			Ogre::Camera *m_Camera;
			Ogre::Viewport *m_Viewport;
			std::unique_ptr<CameraController> m_controller;
			std::unique_ptr<ViewportGrid> m_grid;
			QTimer m_updateTimer;
		};
	}
}
