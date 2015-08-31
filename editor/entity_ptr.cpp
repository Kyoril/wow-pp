
#include "entity_ptr.h"
#include "OgreSceneManager.h"
#include "OgreSceneNode.h"

namespace wowpp
{
	namespace ogre_utils
	{
		void OgreEntityDeleter::operator()(Ogre::Entity *entity) const
		{
			if (entity)
			{
				if (entity->isAttached()) entity->getParentSceneNode()->detachObject(entity);
				entity->_getManager()->destroyEntity(entity);
			}
		}
	}
}
