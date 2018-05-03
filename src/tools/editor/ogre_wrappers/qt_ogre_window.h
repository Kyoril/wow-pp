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

#include "common/typedefs.h"
#include <QtWidgets/QApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QWindow>
#include <Ogre.h>
#include "camera_manager.h"
#include "selection_box.h"

namespace wowpp
{
	namespace editor
	{
		class MPQArchiveFactory;
	}
}

struct IScene
{
	virtual ~IScene() { }

	virtual void update(float delta) = 0;
	virtual void onSelection(Ogre::Entity &entity) = 0;
	virtual void onSetPoint(const Ogre::Vector3 &point) = 0;
	virtual void onAddUnitSpawn(wowpp::UInt32 entry, const Ogre::Vector3 &point) = 0;
	virtual void onKeyPressed(const QKeyEvent *e) = 0;
	virtual void onKeyReleased(const QKeyEvent *e) = 0;
	virtual void onMousePressed(const QMouseEvent *e) = 0;
	virtual void onMouseReleased(const QMouseEvent *e) = 0;
	virtual void onMouseMoved(const QMouseEvent *e) = 0;
	virtual void onDoubleClick(const QMouseEvent *e) = 0;
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

	void entitySelected(Ogre::Entity &entity);

protected:

	Ogre::Root *m_ogreRoot;
	Ogre::RenderWindow *m_ogreWindow;
	Ogre::SceneManager *m_ogreSceneMgr;
	Ogre::Camera *m_ogreCamera;
	Ogre::ColourValue m_ogreBackground;
	std::unique_ptr<IScene> m_scene;
	std::unique_ptr<OgreQtBites::SdkQtCameraMan> m_cameraMan;
	wowpp::editor::MPQArchiveFactory *m_mpqArchives;
	bool m_updatePending;
	bool m_animating;
	std::unique_ptr<wowpp::editor::SelectionBox> m_selectionBox;
	QPointF m_clickPos;

	virtual void keyPressEvent(QKeyEvent *e);
	virtual void keyReleaseEvent(QKeyEvent *e);
	virtual void mouseMoveEvent(QMouseEvent *e);
	virtual void wheelEvent(QWheelEvent *e);
	virtual void mousePressEvent(QMouseEvent *e);
	virtual void mouseReleaseEvent(QMouseEvent *e);
	virtual void mouseDoubleClickEvent(QMouseEvent* e);
	virtual void exposeEvent(QExposeEvent *e);
	virtual bool event(QEvent *e);

	virtual bool frameRenderingQueued(const Ogre::FrameEvent &evt);

	void log(const Ogre::String &msg);
	void log(const QString &msg);
};
