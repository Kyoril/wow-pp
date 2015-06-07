
#include "page_neighborhood.h"
#include <cassert>

namespace wowpp
{
	namespace paging
	{
		PageNeighborhood::PageNeighborhood(Page &mainPage)
		{
			m_pages.fill(nullptr);
			setPageByRelativePosition(PagePosition(0, 0), &mainPage);
		}

		void PageNeighborhood::setPageByRelativePosition(const PagePosition &position, Page *page)
		{
			std::size_t index = toIndex(position);
			assert(index < m_pages.size() && "Index out of range");

			m_pages[toIndex(position)] = page;
		}

		Page * PageNeighborhood::getPageByRelativePosition(const PagePosition &position) const
		{
			return m_pages[toIndex(position)];
		}

		Page & PageNeighborhood::getMainPage() const
		{
			return *getPageByRelativePosition(PagePosition(0, 0));
		}

		std::size_t PageNeighborhood::toIndex(const PagePosition &position)
		{
			return (position[1] * 2 + position[0]);
		}

	}
}
