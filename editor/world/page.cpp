
#include "page.h"

namespace wowpp
{
	namespace paging
	{
		Page::Page(const PagePosition &position)
			: m_position(position)
		{
		}

		const PagePosition & Page::getPosition() const
		{
			return m_position;
		}

	}
}
