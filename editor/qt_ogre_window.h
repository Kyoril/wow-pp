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

#include <QtWidgets/QApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QWindow>
#include <Ogre.h>
#include <memory>
#include <utility>
#include "camera_manager.h"

struct IScene
{
	virtual ~IScene() { }

	virtual void update(float delta) = 0;
};

class QtOgreWindow : public QWindow, public Ogre::FrameListener
{
	Q_OBJECT

public:

	explicit QtOgreWindow(QWindow *parent = nullptr);
	~QtOgreWindow();

	virtual void render(QPainter *painter);
	virtual void render();
	virtual void initialize();
	virtual void createScene();

	void setAnimating(bool animating);

	void setScene(std::unique_ptr<IScene> scene);

	Ogre::SceneManager *getSceneManager() { return m_ogreSceneMgr; }
	Ogre::Camera *getCamera() { return m_ogreCamera; }

public slots:

	virtual void renderLater();
	virtual void renderNow();
	virtual bool eventFilter(QObject *target, QEvent *e);

signals:

	void entitySelected(Ogre::Entity* entity);

protected:

	Ogre::Root *m_ogreRoot;
	Ogre::RenderWindow *m_ogreWindow;
	Ogre::SceneManager *m_ogreSceneMgr;
	Ogre::Camera *m_ogreCamera;
	Ogre::ColourValue m_ogreBackground;
	std::unique_ptr<IScene> m_scene;
	std::unique_ptr<OgreQtBites::SdkQtCameraMan> m_cameraMan;
	bool m_updatePending;
	bool m_animating;

	virtual void keyPressEvent(QKeyEvent *e);
	virtual void keyReleaseEvent(QKeyEvent *e);
	virtual void mouseMoveEvent(QMouseEvent *e);
	virtual void wheelEvent(QWheelEvent *e);
	virtual void mousePressEvent(QMouseEvent *e);
	virtual void mouseReleaseEvent(QMouseEvent *e);
	virtual void exposeEvent(QExposeEvent *e);
	virtual bool event(QEvent *e);

	virtual bool frameRenderingQueued(const Ogre::FrameEvent &evt);

	void log(const Ogre::String &msg);
	void log(const QString &msg);
};
