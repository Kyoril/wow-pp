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

#include "main_window.h"
#include "editor_application.h"
#include "object_editor.h"
#include "load_map_dialog.h"
#include "ui_main_window.h"
#include "ogre_blp_codec.h"
#include "ogre_mpq_archive.h"
#include "world_editor.h"
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QSettings>
#include "game/map.h"

namespace wowpp
{
	namespace editor
	{
		MainWindow::MainWindow(EditorApplication &app)
			: QMainWindow()
			, m_application(app)
			, m_ui(new Ui::MainWindow())
		{
			m_ui->setupUi(this);

			// Connect slots
			connect(m_ui->actionSave, SIGNAL(triggered()), &m_application, SLOT(saveUnsavedChanges()));
			connect(m_ui->actionObjectEditor, SIGNAL(triggered()), &m_application, SLOT(showObjectEditor()));
			connect(m_ui->actionTriggerEditor, SIGNAL(triggered()), &m_application, SLOT(showTriggerEditor()));

			// Layout will be deleted automatically on window destruction
			QVBoxLayout *layout = new QVBoxLayout(m_ui->renderWidget);
			layout->setMargin(0);

			// Window will be automatically deleted on destruction
			m_ogreWindow = new QtOgreWindow();

			// Container will be automatically deleted on destruction
			QWidget *container = QWidget::createWindowContainer(m_ogreWindow, nullptr);
			layout->addWidget(container, 1);

			readSettings();
		}

		void MainWindow::closeEvent(QCloseEvent *qEvent)
		{
			if (m_application.shutdown())
			{
				QSettings settings("WoW++", "WoW++ Editor");
				settings.setValue("geometry", saveGeometry());
				settings.setValue("windowState", saveState());

				qEvent->accept();
			}
			else
			{
				qEvent->ignore();
			}
		}

		void MainWindow::readSettings()
		{
			QSettings settings("WoW++", "Wow++ Editor");
			restoreGeometry(settings.value("geometry").toByteArray());
			restoreState(settings.value("windowState").toByteArray());
		}

		void MainWindow::on_actionExit_triggered()
		{
			close();
		}

		void MainWindow::on_actionLoadMap_triggered()
		{
			LoadMapDialog dialog(m_application);
			int result = dialog.exec();
			if (result == QDialog::Accepted)
			{
				auto *entry = dialog.getSelectedMap();
				if (!entry)
					return;

				auto *sceneMgr = m_ogreWindow->getSceneManager();
				auto *camera = m_ogreWindow->getCamera();
				if (!sceneMgr || !camera)
					return;

				// Reset scene
				m_ogreWindow->setScene(std::unique_ptr<IScene>());

				auto list = Ogre::ResourceGroupManager::getSingleton().getResourceGroups();
				for (auto &name : list)
				{
					Ogre::ResourceGroupManager::getSingleton().unloadUnreferencedResourcesInGroup(name, false);
				}

				camera->setNearClipDistance(0.5f);
				camera->setFarClipDistance(1500.0f);
				camera->setOrientation(Ogre::Quaternion(Ogre::Degree(90.0f), Ogre::Vector3::UNIT_X));
				camera->setFixedYawAxis(true, Ogre::Vector3::UNIT_Z);

				camera->setPosition(0.0f, 0.0f, 110.9062f);
				camera->pitch(Ogre::Degree(-45.0f));

				std::unique_ptr<WorldEditor> scene(
					new WorldEditor(*sceneMgr, *camera, *entry));
				m_ogreWindow->setScene(std::move(scene));

				std::unique_ptr<Map> mapInst(new Map(
					*entry, m_application.getConfiguration().dataPath));
				auto *tile = mapInst->getTile(TileIndex2D(32, 32));
				if (!tile)
				{
					return;
				}

				auto material = Ogre::MaterialManager::getSingleton().createOrRetrieve("LineOfSightBlock", "General", true);
				Ogre::MaterialPtr matPtr = material.first.dynamicCast<Ogre::Material>();
				matPtr->removeAllTechniques();
				auto *teq = matPtr->createTechnique();
				teq->removeAllPasses();
				teq->setCullingMode(Ogre::CULL_NONE);
				teq->setManualCullingMode(Ogre::ManualCullingMode::MANUAL_CULL_NONE);
				auto *pass = teq->createPass();
				pass->setPolygonMode(Ogre::PM_SOLID);
				pass->setSceneBlending(Ogre::SceneBlendType::SBT_TRANSPARENT_ALPHA);
				//pass->setDiffuse(0.7f, 0.7f, 0.7f, 1.0f);
				pass->setVertexColourTracking(Ogre::TVC_DIFFUSE);
				pass = teq->createPass();
				pass->setPolygonMode(Ogre::PM_WIREFRAME);
				pass->setSceneBlending(Ogre::SceneBlendType::SBT_MODULATE);
				pass->setDiffuse(0.0f, 0.0f, 0.0f, 1.0f);
				pass->setAmbient(0.0f, 0.0f, 0.0f);

				const UInt32 collisionTriIndex = 2916;

				// Create collision for this map
				Ogre::ManualObject *obj = sceneMgr->createManualObject();
				obj->begin("LineOfSightBlock", Ogre::RenderOperation::OT_TRIANGLE_LIST);
				obj->estimateVertexCount(tile->collision.vertexCount);
				obj->estimateIndexCount(tile->collision.triangleCount * 3);
				for (auto &vert : tile->collision.vertices)
				{
					obj->position(vert.x, vert.y, vert.z);
					obj->colour(Ogre::ColourValue(0.5f, 0.5f, 0.5f));
				}
				UInt32 triIndex = 0;
				for (auto &tri : tile->collision.triangles)
				{
					if (triIndex != collisionTriIndex)
					{
						obj->index(tri.indexA);
						obj->index(tri.indexB);
						obj->index(tri.indexC);
					}
					triIndex++;
				}
				obj->end();

				obj->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_TRIANGLE_LIST);
				auto &tri = tile->collision.triangles[collisionTriIndex];
				auto &vA = tile->collision.vertices[tri.indexA];
				auto &vB = tile->collision.vertices[tri.indexB];
				auto &vC = tile->collision.vertices[tri.indexC];
				obj->position(vA.x, vA.y, vA.z);
				obj->colour(Ogre::ColourValue::Red);
				obj->position(vB.x, vB.y, vB.z);
				obj->colour(Ogre::ColourValue::Red);
				obj->position(vC.x, vC.y, vC.z);
				obj->colour(Ogre::ColourValue::Red);
				obj->triangle(0, 1, 2);
				obj->end();

				math::Vector3 vStart(-186.254196f, -430.123688f, 55.836506f);
				math::Vector3 vEnd(-192.914993f, -448.210999f, 56.433899f);

				obj->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_LINE_STRIP);
				obj->position(vStart.x, vStart.y, vStart.z);
				obj->colour(Ogre::ColourValue::Green);
				obj->position(vEnd.x, vEnd.y, vEnd.z);
				obj->colour(Ogre::ColourValue::Green);
				obj->index(0); obj->index(1);
				obj->end();

				camera->setPosition(vStart.x, vStart.y, vStart.z);
				camera->setFarClipDistance(0.0f);
				sceneMgr->getRootSceneNode()->attachObject(obj);
			}
		}

	}
}
