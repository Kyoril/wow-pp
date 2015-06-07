
#pragma once

namespace wowpp
{
	namespace editor
	{
		class World final
		{
		private:

			explicit World();

		public:

			static World &getInstance();

		private:

			static World sm_instance;
		};
	}
}
