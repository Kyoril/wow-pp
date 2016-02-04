
#pragma once

#include <array>
#include "page.h"


namespace wowpp
{
	namespace paging
	{
		class PageNeighborhood
		{
		public:

			explicit PageNeighborhood(Page &mainPage);

			void setPageByRelativePosition(const PagePosition &position, Page *page);
			Page *getPageByRelativePosition(const PagePosition &position) const;
			Page &getMainPage() const;

		private:

			static std::size_t toIndex(const PagePosition &position);

		private:

			std::array<Page *, 4> m_pages;
		};
	}
}
