
#include "world.h"

namespace wowpp
{
	namespace editor
	{
		World World::sm_instance;

		World::World()
		{
		}

		World & World::getInstance()
		{
			return sm_instance;
		}
	}
}
