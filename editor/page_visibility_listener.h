
#pragma once

#include "page.h"
#include "common/vector.h"

namespace wowpp
{
	namespace paging
	{
		struct IPageVisibilityListener
		{
			virtual ~IPageVisibilityListener();

			virtual void onPageVisibilityChanged(const PagePosition &page, bool isVisible) = 0;
		};
	}
}
