
#include "scene_node_ptr.h"

namespace wowpp
{
	namespace ogre_utils
	{
		void OgreSceneNodeDeleter::operator()(Ogre::SceneNode *sceneNode) const
		{
			if (sceneNode)
			{
				sceneNode->detachAllObjects();
				sceneNode->getParentSceneNode()->removeAndDestroyChild(sceneNode->getName());
			}
		}
	}
}
