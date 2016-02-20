//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software 
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// World of Warcraft, and all World of Warcraft or Warcraft art, images,
// and lore are copyrighted by Blizzard Entertainment, Inc.
// 

#include "pch.h"
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
