
#pragma once

namespace wowpp
{
	namespace paging
	{
		class PageNeighborhood;

		struct IPageLoaderListener
		{
			virtual ~IPageLoaderListener();

			virtual void onPageAvailabilityChanged(const PageNeighborhood &page, bool isAvailable) = 0;
		};
	}
}
