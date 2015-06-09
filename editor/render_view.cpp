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

#include "render_view.h"
#include "program.h"
#include "common/make_unique.h"
#include "log/default_log_levels.h"
#include "OgreSceneNode.h"
#include "OgreManualObject.h"
#include "ogre_mpq_archive.h"
#include "OgreArchiveManager.h"
#include "OgreMaterialManager.h"
#include "OgreMaterial.h"
#include "OgreTechnique.h"
#include "OgrePass.h"
#include "OgreTextureUnitState.h"
#include "ogre_blp_codec.h"
#include <boost/filesystem.hpp>
#include <cassert>

namespace wowpp
{
	namespace editor
	{
		RenderView::RenderView(QWidget *Parent/* = nullptr*/)
			: QGLWidget(Parent)
		{
		}

		void RenderView::initializeGL()
		{
			Program *program = static_cast<Program*>(QCoreApplication::instance());
			if (!program)
			{
				//TODO
				return;
			}

			// Get config
			const Configuration &c = program->getConfiguration();

			// Get graphics module
			Graphics &g = program->getGraphics();

			// Create render window and scene objects
			Ogre::String winHandle;
#ifdef WIN32
			// Windows code
			winHandle += Ogre::StringConverter::toString((unsigned long)(this->parentWidget()->winId()));
#elif __APPLE__
			// Mac code, tested on Mac OSX 10.6 using Qt 4.7.4 and Ogre 1.7.3
			winHandle = Ogre::StringConverter::toString((long)winId());
#else
			// Unix code
			QX11Info info = x11Info();
			winHandle = Ogre::StringConverter::toString((unsigned long)(info.display()));
			winHandle += ":";
			winHandle += Ogre::StringConverter::toString((unsigned int)(info.screen()));
			winHandle += ":";
			winHandle += Ogre::StringConverter::toString((unsigned long)(this->parentWidget()->winId()));
#endif

			Ogre::NameValuePairList params;

            setAutoBufferSwap(false);
            setAutoFillBackground(false);
            
#ifndef __APPLE__
			// code for Windows and Linux
			params["parentWindowHandle"] = winHandle;
			m_OgreWindow = Ogre::Root::getSingleton().createRenderWindow("QOgreWidget_RenderWindow",
				this->width(),
				this->height(),
				false,
				&params);

            setAttribute(Qt::WA_PaintOnScreen, true);
            setAttribute(Qt::WA_NoBackground);
            
			m_OgreWindow->setActive(true);
			WId ogreWinId = 0x0;
			m_OgreWindow->getCustomAttribute("WINDOW", &ogreWinId);

			assert(ogreWinId);

			// bug fix, extract geometry
			QRect geo = this->frameGeometry();

			// create new window
			this->create(ogreWinId);

			// set geometrie infos to new window
			this->setGeometry(geo);
#else
			// code for Mac
			params["externalWindowHandle"] = winHandle;
			params["macAPI"] = "cocoa";
			params["macAPICocoaUseNSView"] = "true";
			m_OgreWindow = Ogre::Root::getSingleton().createRenderWindow("QOgreWidget_RenderWindow",
				width(), height(), false, &params);
			m_OgreWindow->setActive(true);
			makeCurrent();
#endif

			m_Camera = g.getSceneManager().createCamera("QOgreWidget_Cam");
			m_Camera->setPosition(Ogre::Vector3(1676.71f, 121.67f, 1678.31f));
			//m_Camera->lookAt(Ogre::Vector3(0, 0, 0));
			m_Camera->setNearClipDistance(0.5f);
			m_Camera->setFarClipDistance(1500.0f);
			m_Camera->setFOVy(Ogre::Degree(45.0f));
			m_Camera->setAspectRatio(static_cast<Ogre::Real>(width()) / static_cast<Ogre::Real>(height()));
			
			// Setup resources
			static bool resourcesLoaded = false;
			if (!resourcesLoaded)
			{
				resourcesLoaded = true;

				// Register archive factory
				Ogre::ArchiveManager::getSingleton().addArchiveFactory(new MPQArchiveFactory());

				// Startup BLP image codec
				BLPCodec::startup();

				// WoW game path like "C:\Program Files (x86)\World of Warcraft"
				// Remember: We only support Vanilla WoW (2.4.3)
				const boost::filesystem::path wowDataPath(c.wowGamePath);
				const boost::filesystem::path dataPath(c.dataPath);

				// Setup resource locations
				Ogre::ResourceGroupManager &resMgr = Ogre::ResourceGroupManager::getSingleton();
				// Internal
				resMgr.addResourceLocation((dataPath / "editor").string(), "FileSystem", resMgr.DEFAULT_RESOURCE_GROUP_NAME);
				// WoW
				resMgr.addResourceLocation((wowDataPath / "Data/patch-2.mpq").string(), "MPQ", "WoW");
				resMgr.addResourceLocation((wowDataPath / "Data/patch.mpq").string(), "MPQ", "WoW");
				resMgr.addResourceLocation((wowDataPath / "Data/common.mpq").string(), "MPQ", "WoW");
				resMgr.addResourceLocation((wowDataPath / "Data/expansion.mpq").string(), "MPQ", "WoW");
				resMgr.initialiseAllResourceGroups();
			}

			// 
			Ogre::Viewport *m_Viewport = m_OgreWindow->addViewport(m_Camera);
			m_Viewport->setBackgroundColour(Ogre::ColourValue(0.1f, 0.2f, 0.3f));

			// Create camera controller
			m_controller.reset(new CameraController(*m_Camera));

			// Setup viewport grid
			m_grid = make_unique<ViewportGrid>(program->getGraphics());
			
			// Setup world editor
			auto *map = program->getProject().maps.getEditableById(0);
			if (map)
			{
				m_worldEditor = make_unique<WorldEditor>(
					g,
					*m_Camera,
					*map);
			}

			// Setup the update timer
			connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(repaint()));
			m_updateTimer.start(1);
		}

		void RenderView::paintGL()
		{
			float delta = 1.0f / m_OgreWindow->getLastFPS();

            // Update camera controller (TODO: Delta time)
			m_controller->update(delta);
         
			if (m_worldEditor)
				m_worldEditor->update(delta);

			assert(m_OgreWindow);
            m_OgreWindow->update(false);
         
            // Be sure to call "OgreWidget->repaint();" to call paintGL
            swapBuffers();
		}

		void RenderView::resizeGL(int width, int height)
		{
			assert(m_OgreWindow);
			m_OgreWindow->reposition(this->pos().x(),
				this->pos().y());
			m_OgreWindow->resize(width, height);
			m_Camera->setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
		}

		void RenderView::keyPressEvent(QKeyEvent *keyEvent)
		{
			// Redirect event
			m_controller->onKeyPressed(keyEvent);
		}

		void RenderView::keyReleaseEvent(QKeyEvent *keyEvent)
		{
			// Redirect event
			m_controller->onKeyReleased(keyEvent);
		}

		void RenderView::mousePressEvent(QMouseEvent *mouseEvent)
		{
			this->setCursor(Qt::BlankCursor);

			// Focus this widget so we can receive key events which will then be redirected
			// to the camera controller
			setFocus(Qt::MouseFocusReason);
			m_controller->onMousePressed(mouseEvent);
		}

		void RenderView::mouseMoveEvent(QMouseEvent *mouseEvent)
		{
			m_controller->onMouseMoved(mouseEvent);
		}

		void RenderView::mouseReleaseEvent(QMouseEvent *mouseEvent)
		{
			this->unsetCursor();

			m_controller->onMouseReleased(mouseEvent);

			// Remove the focus of this widget
			auto *p = parentWidget();
			if (p) p->setFocus();
		}
}
}
