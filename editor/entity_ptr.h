
#pragma once

#include <memory>
#include "OgreEntity.h"

namespace wowpp
{
	namespace ogre_utils
	{
		struct OgreEntityDeleter final
		{
			void operator ()(Ogre::Entity *entity) const;
		};

		typedef std::unique_ptr<Ogre::Entity, OgreEntityDeleter> EntityPtr;
	}
}
