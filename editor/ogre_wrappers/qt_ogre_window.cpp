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
#include "qt_ogre_window.h"
#include "ogre_blp_codec.h"
#include "ogre_mpq_archive.h"
#include "ogre_dbc_file_manager.h"
#include "common/make_unique.h"
#include <QPainter>
#include "log/default_log_levels.h"

QtOgreWindow::QtOgreWindow(QWindow *parent /*= nullptr*/)
	: QWindow(parent)
	, m_ogreRoot(nullptr)
	, m_ogreWindow(nullptr)
	, m_ogreCamera(nullptr)
	, m_updatePending(false)
	, m_mpqArchives(nullptr)
    , m_animating(false)
{
	setAnimating(true);
	installEventFilter(this);
	m_ogreBackground = Ogre::ColourValue(0.0f, 0.5f, 1.0f);
}

QtOgreWindow::~QtOgreWindow()
{
	m_scene.reset();

	delete m_ogreRoot;
}

void QtOgreWindow::render(QPainter *painter)
{
	Q_UNUSED(painter);
}

void QtOgreWindow::render()
{
	/*
	How we tied in the render function for OGre3D with QWindow's render function. This is what gets call
	repeatedly. Note that we don't call this function directly; rather we use the renderNow() function
	to call this method as we don't want to render the Ogre3D scene unless everything is set up first.
	That is what renderNow() does.

	Theoretically you can have one function that does this check but from my experience it seems better
	to keep things separate and keep the render function as simple as possible.
	*/
	Ogre::WindowEventUtilities::messagePump();
	m_ogreRoot->renderOneFrame();
}

void QtOgreWindow::initialize()
{
	m_ogreRoot = new Ogre::Root("plugins.cfg");

	wowpp::editor::BLPCodec::startup();

	m_mpqArchives = OGRE_NEW wowpp::editor::MPQArchiveFactory();
	Ogre::ArchiveManager::getSingleton().addArchiveFactory(m_mpqArchives);

	Ogre::ConfigFile ogreConfig;
	ogreConfig.load("resources.cfg");
	Ogre::ConfigFile::SectionIterator seci = ogreConfig.getSectionIterator();
	Ogre::String secName, typeName, archName;
	while (seci.hasMoreElements())
	{
		secName = seci.peekNextKey();
		Ogre::ConfigFile::SettingsMultiMap *settings = seci.getNext();
		Ogre::ConfigFile::SettingsMultiMap::iterator i;
		for (i = settings->begin(); i != settings->end(); ++i)
		{
			typeName = i->first;
			archName = i->second;
			Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
				archName, typeName, secName);
		}
	}

	const Ogre::RenderSystemList& rsList = m_ogreRoot->getAvailableRenderers();
	Ogre::RenderSystem* rs = rsList[0];

	Ogre::StringVector renderOrder;
#if defined(Q_OS_WIN)
	renderOrder.push_back("Direct3D9");
	renderOrder.push_back("Direct3D11");
#endif
	renderOrder.push_back("OpenGL");
	renderOrder.push_back("OpenGL 3+");

	for (Ogre::StringVector::iterator iter = renderOrder.begin(); iter != renderOrder.end(); iter++)
	{
		for (Ogre::RenderSystemList::const_iterator it = rsList.begin(); it != rsList.end(); it++)
		{
			if ((*it)->getName().find(*iter) != Ogre::String::npos)
			{
				rs = *it;
				break;
			}
		}
		if (rs != nullptr) break;
	}
	if (rs == nullptr)
	{
		if (!m_ogreRoot->restoreConfig())
		{
			if (!m_ogreRoot->showConfigDialog())
				OGRE_EXCEPT(Ogre::Exception::ERR_INVALIDPARAMS,
				"Abort render system configuration",
				"QTOgreWindow::initialize");
		}
	}

	/*
	Setting size and VSync on windows will solve a lot of problems
	*/
	QString dimensions = QString("%1 x %2").arg(this->width()).arg(this->height());
	rs->setConfigOption("Video Mode", dimensions.toStdString());
	rs->setConfigOption("Full Screen", "No");
	rs->setConfigOption("VSync", "Yes");
	m_ogreRoot->setRenderSystem(rs);
	m_ogreRoot->initialise(false);

	Ogre::NameValuePairList parameters;
	/*
	Flag within the parameters set so that Ogre3D initializes an OpenGL context on it's own.
	*/
	if (rs->getName().find("GL") <= rs->getName().size())
		parameters["currentGLContext"] = Ogre::String("false");

	/*
	We need to supply the low level OS window handle to this QWindow so that Ogre3D knows where to draw
	the scene. Below is a cross-platform method on how to do this.
	If you set both options (externalWindowHandle and parentWindowHandle) this code will work with OpenGL
	and DirectX.
	*/
#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
	parameters["externalWindowHandle"] = Ogre::StringConverter::toString((size_t)(this->winId()));
	parameters["parentWindowHandle"] = Ogre::StringConverter::toString((size_t)(this->winId()));
#else
	parameters["externalWindowHandle"] = Ogre::StringConverter::toString((unsigned long)(this->winId()));
	parameters["parentWindowHandle"] = Ogre::StringConverter::toString((unsigned long)(this->winId()));
#endif

#if defined(Q_OS_MAC)
	parameters["macAPI"] = "cocoa";
	parameters["macAPICocoaUseNSView"] = "true";
#endif

	/*
	Note below that we supply the creation function for the Ogre3D window the width and height
	from the current QWindow object using the "this" pointer.
	*/
	m_ogreWindow = m_ogreRoot->createRenderWindow("QT Window",
		this->width(),
		this->height(),
		false,
		&parameters);
	m_ogreWindow->setVisible(true);

	m_ogreSceneMgr = m_ogreRoot->createSceneManager(Ogre::ST_GENERIC);

	m_ogreCamera = m_ogreSceneMgr->createCamera("MainCamera");
	m_ogreCamera->setPosition(Ogre::Vector3(0.0f, 0.0f, 3.0f));
	m_ogreCamera->lookAt(Ogre::Vector3(0.0f, 0.0f, -300.0f));
	m_ogreCamera->setNearClipDistance(0.1f);
	m_ogreCamera->setFarClipDistance(200.0f);

	Ogre::Viewport* pViewPort = m_ogreWindow->addViewport(m_ogreCamera);
	pViewPort->setBackgroundColour(m_ogreBackground);

	m_ogreCamera->setAspectRatio(
		Ogre::Real(m_ogreWindow->getWidth()) / Ogre::Real(m_ogreWindow->getHeight()));
	m_ogreCamera->setAutoAspectRatio(true);

	// Initialize dbc file manager
	OGRE_NEW wowpp::OgreDBCFileManager();

	Ogre::TextureManager::getSingleton().setDefaultNumMipmaps(5);
	Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

	m_cameraMan.reset(new OgreQtBites::SdkQtCameraMan(m_ogreCamera));

	createScene();

	m_ogreRoot->addFrameListener(this);

	auto material = Ogre::MaterialManager::getSingleton().createOrRetrieve("LineOfSightBlock", "General", true);
	Ogre::MaterialPtr matPtr = material.first.dynamicCast<Ogre::Material>();
	matPtr->removeAllTechniques();
	auto *teq = matPtr->createTechnique();
	teq->removeAllPasses();
	teq->setCullingMode(Ogre::CULL_NONE);
	teq->setManualCullingMode(Ogre::ManualCullingMode::MANUAL_CULL_NONE);
	auto *pass = teq->createPass();
	pass->setPolygonMode(Ogre::PM_SOLID);
	pass->setVertexColourTracking(Ogre::TVC_DIFFUSE);
	pass = teq->createPass();
	pass->setPolygonMode(Ogre::PM_WIREFRAME);
	pass->setSceneBlending(Ogre::SceneBlendType::SBT_MODULATE);
	pass->setDiffuse(0.0f, 0.0f, 0.0f, 1.0f);
	pass->setAmbient(0.0f, 0.0f, 0.0f);
	pass->setFog(false);
}

void QtOgreWindow::createScene()
{
	m_ogreSceneMgr->setAmbientLight(Ogre::ColourValue(0.5f, 0.5f, 0.5f));

	Ogre::Light* light = m_ogreSceneMgr->createLight("MainLight");
	light->setPosition(20.0f, 80.0f, 50.0f);
}

void QtOgreWindow::setAnimating(bool animating)
{
	m_animating = animating;

	if (animating)
		renderLater();
}

void QtOgreWindow::renderLater()
{
	if (!m_updatePending)
	{
		m_updatePending = true;
		QApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
	}
}

void QtOgreWindow::renderNow()
{
	if (!isExposed())
		return;

	if (m_ogreRoot == nullptr)
	{
		initialize();
	}

	try
	{
		render();
	}
	catch (const Ogre::Exception &)
	{

	}

	if (m_animating)
		renderLater();
}

bool QtOgreWindow::eventFilter(QObject *target, QEvent *e)
{
	if (target == this)
	{
		if (e->type() == QEvent::Resize)
		{
			if (isExposed() && m_ogreWindow != nullptr)
			{
				m_ogreWindow->resize(this->width(), this->height());
			}
		}
	}

	return false;
}

void QtOgreWindow::keyPressEvent(QKeyEvent *e)
{
	if (m_cameraMan)
		m_cameraMan->injectKeyDown(*e);

	if (m_scene)
		m_scene->onKeyPressed(e);
}

void QtOgreWindow::keyReleaseEvent(QKeyEvent *e)
{
	if (m_cameraMan)
		m_cameraMan->injectKeyUp(*e);

	if (m_scene)
		m_scene->onKeyReleased(e);
}

void QtOgreWindow::mouseMoveEvent(QMouseEvent *e)
{
	if (e->buttons() & Qt::LeftButton)
	{
	/*	QPointF delta = e->localPos() - m_clickPos;
		if (!delta.isNull())
		{
			Ogre::Rect r;

			if (std::signbit(delta.x()))
			{
				r.left = static_cast<long>(m_clickPos.x());
				r.right = static_cast<long>(e->localPos().x());
			}
			else
			{
				r.left = static_cast<long>(e->localPos().x());
				r.right = static_cast<long>(m_clickPos.x());
			}

			if (std::signbit(delta.y()))
			{
				r.top = static_cast<long>(m_clickPos.y());
				r.bottom = static_cast<long>(e->localPos().y());
			}
			else
			{
				r.top = static_cast<long>(e->localPos().y());
				r.bottom = static_cast<long>(m_clickPos.y());
			}

			m_selectionBox->setRectangle(r);
		}

		return;*/
	}

	static int lastX = e->x();
	static int lastY = e->y();
	int relX = e->x() - lastX;
	int relY = e->y() - lastY;
	lastX = e->x();
	lastY = e->y();

	if (m_cameraMan && (e->buttons() & Qt::RightButton))
		m_cameraMan->injectMouseMove(relX, relY);

	if (m_scene)
		m_scene->onMouseMoved(e);
}

void QtOgreWindow::wheelEvent(QWheelEvent *e)
{
	if (m_cameraMan)
		m_cameraMan->injectWheelMove(*e);
}

void QtOgreWindow::mousePressEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton)
	{
		m_clickPos = e->localPos();

		// Create selection box
		/*m_selectionBox = wowpp::make_unique<wowpp::editor::SelectionBox>(
			*m_ogreSceneMgr, *m_ogreCamera
			);*/
	}

	if (m_cameraMan)
		m_cameraMan->injectMouseDown(*e);

	if (m_scene)
		m_scene->onMousePressed(e);
}

void QtOgreWindow::mouseReleaseEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton)
	{
		// Clear selection rect
		//m_selectionBox.reset();
	}

	if (m_cameraMan)
		m_cameraMan->injectMouseUp(*e);

	if (m_scene) 
		m_scene->onMouseReleased(e);

	if (e->button() == Qt::LeftButton)
	{
		QPoint pos = e->pos();
		Ogre::Ray mouseRay = m_ogreCamera->getCameraToViewportRay(
			(Ogre::Real)pos.x() / m_ogreWindow->getWidth(),
			(Ogre::Real)pos.y() / m_ogreWindow->getHeight());
		Ogre::RaySceneQuery* pSceneQuery = m_ogreSceneMgr->createRayQuery(mouseRay);
		pSceneQuery->setSortByDistance(true);
		Ogre::RaySceneQueryResult vResult = pSceneQuery->execute();
		for (size_t ui = 0; ui < vResult.size(); ui++)
		{
			if (vResult[ui].movable)
			{
				if (vResult[ui].movable->getMovableType().compare("Entity") == 0)
				{
					m_scene->onSelection(*((Ogre::Entity*)vResult[ui].movable));
				}
			}
		}
		m_ogreSceneMgr->destroyQuery(pSceneQuery);
	}
}

void QtOgreWindow::mouseDoubleClickEvent(QMouseEvent * e)
{
	if (e->button() == Qt::LeftButton)
	{
		if (m_scene)
		{
			m_scene->onDoubleClick(e);
		}
	}
}

void QtOgreWindow::exposeEvent(QExposeEvent *e)
{
	Q_UNUSED(e);

	if (isExposed())
		renderNow();
}

bool QtOgreWindow::event(QEvent *e)
{
	/*
	QWindow's "message pump". The base method that handles all QWindow events. As you will see there
	are other methods that actually process the keyboard/other events of Qt and the underlying OS.

	Note that we call the renderNow() function which checks to see if everything is initialized, etc.
	before calling the render() function.
	*/

	switch (e->type())
	{
	case QEvent::UpdateRequest:
		m_updatePending = false;
		renderNow();
		return true;
	default:
		return QWindow::event(e);
	}
}

bool QtOgreWindow::frameRenderingQueued(const Ogre::FrameEvent &evt)
{
	if (m_scene)
	{
		m_scene->update(evt.timeSinceLastFrame);
	}

	if (m_cameraMan)
		m_cameraMan->frameRenderingQueued(evt);

	return true;
}

void QtOgreWindow::log(const Ogre::String &msg)
{
	if (Ogre::LogManager::getSingletonPtr() != nullptr) Ogre::LogManager::getSingletonPtr()->logMessage(msg);
}

void QtOgreWindow::log(const QString &msg)
{
	log(Ogre::String(msg.toStdString().c_str()));
}

void QtOgreWindow::setScene(std::unique_ptr<IScene> scene)
{
	m_scene = std::move(scene);
}
