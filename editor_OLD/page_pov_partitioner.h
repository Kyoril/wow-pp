
#pragma once

#include "page.h"

namespace wowpp
{
	namespace paging
	{
		struct IPageVisibilityListener;

		size_t distance(size_t first, size_t second);
		bool isInRange(const PagePosition &center, size_t range, const PagePosition &other);

		class PagePOVPartitioner
		{
		public:

			explicit PagePOVPartitioner(
				const PagePosition &size,
				std::size_t sight,
				PagePosition centeredPage,
				IPageVisibilityListener &listener);

			void updateCenter(const PagePosition &centeredPage);

		private:

			const PagePosition m_size;
			const std::size_t m_sight;
			PagePosition m_previouslyCenteredPage;
			IPageVisibilityListener &m_listener;
		};
	}
}
