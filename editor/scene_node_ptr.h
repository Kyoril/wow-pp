
#pragma once

#include <memory>
#include "OgreSceneNode.h"

namespace wowpp
{
	namespace ogre_utils
	{
		struct OgreSceneNodeDeleter final
		{
			void operator ()(Ogre::SceneNode *sceneNode) const;
		};

		typedef std::unique_ptr<Ogre::SceneNode, OgreSceneNodeDeleter> SceneNodePtr;
	}
}
