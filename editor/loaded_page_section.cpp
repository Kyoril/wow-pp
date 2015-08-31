
#include "loaded_page_section.h"
#include "page_neighborhood.h"
#include "page_pov_partitioner.h"
#include "page.h"

namespace wowpp
{
	namespace paging
	{
		LoadedPageSection::LoadedPageSection(PagePosition center, std::size_t range, IPageLoaderListener &sectionListener)
			: m_center(center)
			, m_range(range)
			, m_sectionListener(sectionListener)
		{
		}

		void LoadedPageSection::updateCenter(const PagePosition &center)
		{
			if (m_center == center)
			{
				return;
			}

			std::vector<PageNeighborhood> becomingVisible;

			for (auto i = m_outOfSection.begin(); i != m_outOfSection.end();)
			{
				if (isInRange(center, m_range, (*i).first->getPosition()))
				{
					becomingVisible.push_back(i->second);
					m_sectionListener.onPageAvailabilityChanged(i->second, true);
					i = m_outOfSection.erase(i);
				}
				else
				{
					++i;
				}
			}

			for (auto i = m_insideOfSection.begin(); i != m_insideOfSection.end();)
			{
				if (isInRange(center, m_range, (*i).first->getPosition()))
				{
					++i;
				}
				else
				{
					m_outOfSection.insert(std::make_pair(&i->second.getMainPage(), i->second));
					m_sectionListener.onPageAvailabilityChanged(i->second, false);
					i = m_insideOfSection.erase(i);
				}
			}

			for(const auto & pages : becomingVisible)
			{
				m_insideOfSection.insert(std::make_pair(&pages.getMainPage(), pages));
			}

			m_center = center;
		}

		void LoadedPageSection::onPageAvailabilityChanged(const PageNeighborhood &pages, bool isAvailable)
		{
			auto &mainPage = pages.getMainPage();
			if (isInRange(m_center, m_range, mainPage.getPosition()))
			{
				setVisibility(m_insideOfSection, pages, isAvailable);
				m_sectionListener.onPageAvailabilityChanged(pages, isAvailable);
			}
			else
			{
				setVisibility(m_outOfSection, pages, isAvailable);
			}
		}

		void LoadedPageSection::setVisibility(PageMap &map, const PageNeighborhood &pages, bool isVisible)
		{
			if (isVisible)
			{
				map.insert(std::make_pair(&pages.getMainPage(), pages));
			}
			else
			{
				map.erase(&pages.getMainPage());
			}
		}

		void LoadedPageSection::onPageLoad(const Page &page)
		{
			m_sectionListener.onPageLoad(page);
		}

	}
}
