
#pragma once

#include "page.h"
#include "page_neighborhood.h"
#include "page_visibility_listener.h"
#include "adt_page.h"

#include <functional>
#include <memory>
#include <map>

namespace wowpp
{
	namespace paging
	{
		// Forwards
		struct IPageLoaderListener;
	}

	class WorldPageLoader : public paging::IPageVisibilityListener
	{
	public:

		typedef std::function<void()> Work;
		typedef std::function<void(Work)> DispatchWork;

		explicit WorldPageLoader(
			paging::IPageLoaderListener &resultListener,
			DispatchWork dispatchWork,
			DispatchWork synchronize);

	private:

		virtual void onPageVisibilityChanged(const paging::PagePosition &page, bool isVisible) override;
		void asyncPerformLoadOperation(std::weak_ptr<paging::Page> page);

	private:

		paging::IPageLoaderListener &m_resultListener;
		std::map<paging::PagePosition, std::shared_ptr<paging::Page>> m_pages;
		const DispatchWork m_dispatchWork;
		const DispatchWork m_synchronize;
	};
}
