
#include "world_page_loader.h"
#include "page_loader_listener.h"

namespace wowpp
{
	WorldPageLoader::WorldPageLoader(paging::IPageLoaderListener &resultListener, DispatchWork dispatchWork, DispatchWork synchronize)
		: m_resultListener(resultListener)
		, m_dispatchWork(std::move(dispatchWork))
		, m_synchronize(std::move(synchronize))
	{
	}

	void WorldPageLoader::onPageVisibilityChanged(const paging::PagePosition &page, bool isVisible)
	{
		const auto existingPage = m_pages.find(page);
		if (existingPage == m_pages.end())
		{
			if (isVisible)
			{
				const auto strongPage = std::make_shared<paging::Page>(page);
				m_pages[page] = strongPage;

				const std::weak_ptr<paging::Page> weakPage = strongPage;
				m_dispatchWork([this, weakPage]()
				{
					this->asyncPerformLoadOperation(weakPage);
				});
			}
		}
		else
		{
			if (!isVisible)
			{
				const paging::PageNeighborhood pages(*existingPage->second);

				m_resultListener.onPageAvailabilityChanged(
					pages,
					false
					);

				// Destroy page
				m_pages.erase(existingPage);
			}
		}
	}

	void WorldPageLoader::asyncPerformLoadOperation(std::weak_ptr<paging::Page> page)
	{
		const auto strongPage = page.lock();
		if (!strongPage)
		{
			// Request was cancelled
			return;
		}

		m_resultListener.onPageLoad(*strongPage);

		const auto notify = [this, page]()
		{
			const auto strongPage = page.lock();
			if (!strongPage)
			{
				// Request cancelled
				return;
			}

			const paging::PageNeighborhood pages(*strongPage);

			m_resultListener.onPageAvailabilityChanged(
				pages,
				true
				);
		};
		m_synchronize(notify);
	}
}
