
#include "viewport_grid.h"
#include "graphics.h"
#include "OgreVector3.h"
#include "OgreColourValue.h"
#include "OgreSceneNode.h"
#include "OgreManualObject.h"
#include "OgreSceneManager.h"
#include "OgreMaterialManager.h"
#include "OgreMaterial.h"
#include "OgreTechnique.h"
#include "OgrePass.h"

namespace wowpp
{
	namespace editor
	{
		ViewportGrid::ViewportGrid(Graphics &graphics)
			: m_graphics(graphics)
			, m_largeGrid(16)
			, m_numRows(32)
			, m_numCols(32)
			, m_gridSize(33.3333f)
			, m_darkColor(1.0f, 0.15f, 0.15f, 1.0f)
			, m_lightColor(0.55f, 0.55f, 0.55f, 1.0f)
		{
			m_node = m_graphics.getSceneManager().getRootSceneNode()->createChildSceneNode();
			m_manualObj = m_graphics.getSceneManager().createManualObject();
			m_manualObj->setQueryFlags(0);
			m_manualObj->setRenderQueueGroup(Ogre::RENDER_QUEUE_SKIES_EARLY + 1);
			m_manualObj->setCastShadows(false);
			m_node->attachObject(m_manualObj);

			// Create a new material if it does not exist
			if (!Ogre::MaterialManager::getSingleton().resourceExists("Editor/ViewportGridOrtho"))
			{
				Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().create("Editor/ViewportGridOrtho", "General");
				mat->getTechnique(0)->getPass(0)->setLightingEnabled(false);
				mat->getTechnique(0)->getPass(0)->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);
			}

			update(true);
		}

		ViewportGrid::~ViewportGrid()
		{
			m_graphics.getSceneManager().destroyManualObject(m_manualObj);
			m_manualObj = nullptr;

			m_node->getParentSceneNode()->removeAndDestroyChild(m_node->getName());
			m_node = nullptr;
		}

		void ViewportGrid::update(bool initial)
		{
			// Start update
			if (initial)
			{
				m_manualObj->begin("Editor/ViewportGridOrtho", Ogre::RenderOperation::OT_LINE_LIST);
			}
			else
			{
				m_manualObj->beginUpdate(0);
			}

			//TODO: Choose between perspective and orthographic mode
			updatePerspective();

			// Finish update
			m_manualObj->end();
		}

		void ViewportGrid::updatePosition(const Ogre::Vector3 &cameraPos)
		{
			float grid = m_gridSize;
			grid *= m_largeGrid;

			Ogre::Vector3 newPosition(cameraPos.x, cameraPos.y, cameraPos.z);

			const Ogre::Vector3 mask(1.0f, 1.0f, 0.0f);
			newPosition *= mask;
			newPosition = snapToGrid(newPosition);

			m_node->setPosition(newPosition);
		}

		Ogre::Vector3 ViewportGrid::snapToGrid(const Ogre::Vector3 &position)
		{
			Ogre::Vector3 newPosition(position.x, position.y, position.z);

			newPosition.x = static_cast<float>(floorf(newPosition.x / m_gridSize + 0.5f)) * m_gridSize;
			newPosition.y = static_cast<float>(floorf(newPosition.y / m_gridSize + 0.5f)) * m_gridSize;
			newPosition.z = static_cast<float>(floorf(newPosition.z / m_gridSize + 0.5f)) * m_gridSize;

			return newPosition;
		}

		float ViewportGrid::getGridSize() const
		{
			return m_gridSize;
		}

		void ViewportGrid::setGridSize(float gridSize)
		{
			if (m_gridSize != gridSize)
			{
				m_gridSize = gridSize;
				update(false);
			}
		}

		size_t ViewportGrid::getNumRows() const
		{
			return m_numRows;
		}

		void ViewportGrid::setNumRows(size_t numRows)
		{
			if (m_numRows != numRows)
			{
				m_numRows = numRows;
				update(false);
			}
		}

		size_t ViewportGrid::getNumCols() const
		{
			return m_numCols;
		}

		void ViewportGrid::setNumCols(size_t numCols)
		{
			if (m_numCols != numCols)
			{
				m_numCols = numCols;
				update(false);
			}
		}

		const Ogre::Vector3 & ViewportGrid::getPosition() const
		{
			return m_node->getPosition();
		}

		bool ViewportGrid::isVisible() const
		{
			return m_manualObj->isVisible();
		}

		void ViewportGrid::setVisible(bool visible)
		{
			m_manualObj->setVisible(visible);
		}

		void ViewportGrid::updatePerspective()
		{
			float width = m_gridSize * m_numCols;
			float length = m_gridSize * m_numRows;

			Ogre::Vector3 start, end;
			Ogre::Vector3 gridOrigin((-width / 2.0f), 0.0f, (-length / 2.0f));
			Ogre::ColourValue color;

			for (size_t i = 0; i < m_numRows; ++i)
			{
				start.x = i * m_gridSize;
				start.y = 0.0f;
				start.z = 0.0f;

				end.x = start.z;
				end.y = width;
				end.z = 0.0f;

				if (i % m_largeGrid != 0)
				{
					color = m_lightColor;
				}
				else
				{
					color = m_darkColor;
				}

				m_manualObj->position(gridOrigin + start);
				m_manualObj->colour(color);
				m_manualObj->position(gridOrigin + end);
				m_manualObj->colour(color);
			}

			for (size_t j = 0; j < m_numCols; ++j)
			{
				start.x = 0.0f;
				start.y = j * m_gridSize;
				start.z = 0.0f;

				end.x = length;
				end.y = start.x;
				end.z = 0.0f;

				if (j % m_largeGrid != 0)
				{
					color = m_lightColor;;
				}
				else
				{
					color = m_darkColor;
				}

				m_manualObj->position(gridOrigin + start);
				m_manualObj->colour(color);
				m_manualObj->position(gridOrigin + end);
				m_manualObj->colour(color);
			}
		}
	}
}
