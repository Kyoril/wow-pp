
#pragma once

#include "OgreColourValue.h"

namespace Ogre
{
	class Vector3;
	class ManualObject;
	class SceneNode;
}

namespace wowpp
{
	namespace editor
	{
		class Graphics;

		/// Displays a 3D grid in a viewport.
		class ViewportGrid final
		{
		public:

			explicit ViewportGrid(Graphics &graphics);
			~ViewportGrid();

			void update(bool initial);
			void updatePosition(const Ogre::Vector3 &cameraPos);
			Ogre::Vector3 snapToGrid(const Ogre::Vector3 &position);
			float getGridSize() const;
			void setGridSize(float gridSize);
			size_t getNumRows() const;
			void setNumRows(size_t numRows);
			size_t getNumCols() const;
			void setNumCols(size_t numCols);
			const Ogre::Vector3 &getPosition() const;
			bool isVisible() const;
			void setVisible(bool visible);

		private:

			void updatePerspective();

		private:

			Graphics &m_graphics;
			Ogre::SceneNode *m_node;
			Ogre::ManualObject *m_manualObj;
			Ogre::ColourValue m_lightColor;
			Ogre::ColourValue m_darkColor;
			size_t m_largeGrid;
			size_t m_numRows;
			size_t m_numCols;
			float m_gridSize;
		};
	}
}
