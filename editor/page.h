
#pragma once

#include "common/vector.h"

namespace wowpp
{
	namespace paging
	{
		typedef Vector<std::size_t, 2> PagePosition;

		class Page
		{
		public:

			explicit Page(const PagePosition &position);

			const PagePosition &getPosition() const;

		private:

			const PagePosition m_position;
		};
	}
}
