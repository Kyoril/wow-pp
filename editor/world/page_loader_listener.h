
#pragma once

namespace wowpp
{
	namespace paging
	{
		class PageNeighborhood;
		class Page;

		struct IPageLoaderListener
		{
			virtual ~IPageLoaderListener();

			virtual void onPageLoad(const Page &page) = 0;
			virtual void onPageAvailabilityChanged(const PageNeighborhood &page, bool isAvailable) = 0;
		};
	}
}
