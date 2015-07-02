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

#include "render_widget.h"
#include <QOpenGLContext>

namespace wowpp
{
	namespace editor
	{
		RenderWidget::RenderWidget(QWidget *parent)
			: QGLWidget(parent)
		{
		}

		void RenderWidget::initializeGL()
		{
			// Setup clear color
			glClearColor(0.0f, 0.1f, 0.3f, 0.0f);

			// Setup the update timer
			connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateGL()));
			unpause();
		}

		void RenderWidget::paintGL()
		{
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		}

		void RenderWidget::resizeGL(int w, int h)
		{
			glViewport(0, 0, w, h);

			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
		}

		void RenderWidget::pause()
		{
			m_updateTimer.stop();
		}

		void RenderWidget::unpause()
		{
			m_updateTimer.start(16);
		}

	}
}
