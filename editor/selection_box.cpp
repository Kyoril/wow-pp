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
#include "selection_box.h"

namespace wowpp
{
	namespace editor
	{
		SelectionBox::SelectionBox(Ogre::SceneManager &sceneMgr, Ogre::Camera &camera)
			: m_sceneMgr(sceneMgr)
			, m_camera(camera)
			, m_firstUse(true)
			, m_frameObj(nullptr)
			, m_fillObj(nullptr)
		{
			Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().createOrRetrieve("Editor/SelectionBox", "General").first.staticCast<Ogre::Material>();
			auto *teq = mat->getTechnique(0);
			auto *pass = teq->getPass(0);
			pass->removeAllTextureUnitStates();
			pass->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);
			pass->setLightingEnabled(false);
			pass->setDepthWriteEnabled(false);
			pass->setDepthCheckEnabled(false);
			pass->setFog(true);

			m_rect.setNull();

			// Create frame object
			m_frameObj = m_sceneMgr.createManualObject();
			m_frameObj->setUseIdentityProjection(true);
			m_frameObj->setUseIdentityView(true);
			m_frameObj->setQueryFlags(0);
			m_frameObj->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);

			// Create fill object
			m_fillObj = m_sceneMgr.createManualObject();
			m_fillObj->setUseIdentityProjection(true);
			m_fillObj->setUseIdentityView(true);
			m_fillObj->setQueryFlags(0);
			m_fillObj->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);

			// Create bounding box
			Ogre::AxisAlignedBox aabInf;
			aabInf.setInfinite();
			m_frameObj->setBoundingBox(aabInf);
			m_fillObj->setBoundingBox(aabInf);

			// Create scene node
			m_sceneNode = m_sceneMgr.getRootSceneNode()->createChildSceneNode();
			m_sceneNode->attachObject(m_frameObj);
			m_sceneNode->attachObject(m_fillObj);
		}

		SelectionBox::~SelectionBox()
		{
			m_sceneNode->detachAllObjects();
			m_sceneNode->getParentSceneNode()->removeAndDestroyChild(m_sceneNode->getName());
			m_sceneNode = nullptr;
			m_sceneMgr.destroyManualObject(m_fillObj);
			m_fillObj = nullptr;
			m_sceneMgr.destroyManualObject(m_frameObj);
			m_frameObj = nullptr;
		}

		void SelectionBox::updateDisplay()
		{
			if (m_rect.isNull())
			{
				return;
			}

 			const Ogre::ColourValue frameColor(0.2f, 0.6f, 1.0f, 0.7f);
 			const Ogre::ColourValue fillColor(0.66f, 0.8f, 0.85f, 0.5f);
/*			const Ogre::ColourValue frameColor(0.85f, 0.55f, 0.0f, 0.7f);
			const Ogre::ColourValue fillColor(1.0f, 0.7f, 0.2f, 0.5f);*/

			// Convert pixel coordinates to relative screen space coordinates
			auto *viewport = m_camera.getViewport();
			const float l = static_cast<float>(m_rect.left) / static_cast<float>(viewport->getActualWidth()) * 2.0f - 1.0f;
			const float r = static_cast<float>(m_rect.right) / static_cast<float>(viewport->getActualWidth()) * 2.0f - 1.0f;
			const float t = static_cast<float>(m_rect.top) / static_cast<float>(viewport->getActualHeight()) * -2.0f + 1.0f;
			const float b = static_cast<float>(m_rect.bottom) / static_cast<float>(viewport->getActualHeight()) * -2.0f + 1.0f;

			// Update frame object
			if (m_firstUse)
			{
				m_frameObj->begin("Editor/SelectionBox", Ogre::RenderOperation::OT_LINE_STRIP);
			}
			else
			{
				m_frameObj->beginUpdate(0);
			}

			m_frameObj->position(r, t, 0.0f);
			m_frameObj->colour(frameColor);
			m_frameObj->position(l, t, 0.0f);
			m_frameObj->colour(frameColor);
			m_frameObj->position(l, b, 0.0f);
			m_frameObj->colour(frameColor);
			m_frameObj->position(r, b, 0.0f);
			m_frameObj->colour(frameColor);
			m_frameObj->position(r, t, 0.0f);
			m_frameObj->colour(frameColor);

			// Finish frame object
			m_frameObj->end();

			// Update fill object
			if (m_firstUse)
			{
				m_fillObj->begin("Editor/SelectionBox", Ogre::RenderOperation::OT_TRIANGLE_STRIP);
				m_firstUse = false; // Set to false now
			}
			else
			{
				m_fillObj->beginUpdate(0);
			}

			m_fillObj->position(l, b, 0.0f);
			m_fillObj->colour(fillColor);
			m_fillObj->position(r, b, 0.0f);
			m_fillObj->colour(fillColor);
			m_fillObj->position(l, t, 0.0f);
			m_fillObj->colour(fillColor);
			m_fillObj->position(r, t, 0.0f);
			m_fillObj->colour(fillColor);

			// Finish fill object
			m_fillObj->end();
		}

		void SelectionBox::setRectangle(const Ogre::Rect &rect)
		{
			m_rect = rect;

			updateDisplay();
		}

		const Ogre::Rect & SelectionBox::getRectangle() const
		{
			return m_rect;
		}

	}
}
