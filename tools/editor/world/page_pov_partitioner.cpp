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
#include "page_pov_partitioner.h"
#include "page_visibility_listener.h"

namespace wowpp
{
	namespace paging
	{
		template <class F>
		void forEachPageInSquare(
			const PagePosition &terrainSize,
			const PagePosition &center,
			std::size_t radius,
			const F &pageHandler
			)
		{
			const PagePosition begin(
				std::max(center[0], radius) - radius,
				std::max(center[1], radius) - radius
				);
			const PagePosition end(
				std::min(center[0] + radius + 1, terrainSize[0]),
				std::min(center[1] + radius + 1, terrainSize[1])
				);

			for (size_t y = begin[1]; y < end[1]; ++y)
			{
				for (size_t x = begin[0]; x < end[0]; ++x)
				{
					pageHandler(PagePosition(x, y));
				}
			}
		}

		size_t distance(size_t first, size_t second)
		{
			if (first < second)
			{
				std::swap(first, second);
			}
			return (first - second);
		}

		bool isInRange(const PagePosition &center, size_t range, const PagePosition &other)
		{
			return
				(distance(other[0], center[0]) <= range) &&
				(distance(other[1], center[1]) <= range);
		}

		PagePOVPartitioner::PagePOVPartitioner(const PagePosition &size, std::size_t sight, PagePosition centeredPage, IPageVisibilityListener &listener)
			: m_size(size)
			, m_sight(sight)
			, m_previouslyCenteredPage(centeredPage)
			, m_listener(listener)
		{
			forEachPageInSquare(
				m_size,
				m_previouslyCenteredPage,
				m_sight,
				[&listener](const PagePosition & page)
			{
				listener.onPageVisibilityChanged(page, true);
			});
		}

		void PagePOVPartitioner::updateCenter(const PagePosition &centeredPage)
		{
			if (m_previouslyCenteredPage == centeredPage)
			{
				return;
			}

			forEachPageInSquare(
				m_size,
				m_previouslyCenteredPage,
				m_sight,
				[this, centeredPage](const PagePosition & page)
			{
				if (!isInRange(centeredPage, this->m_sight, page))
				{
					this->m_listener.onPageVisibilityChanged(page, false);
				}
			});

			forEachPageInSquare(
				m_size,
				centeredPage,
				m_sight,
				[this](const PagePosition & page)
			{
				if (!isInRange(this->m_previouslyCenteredPage, this->m_sight, page))
				{
					this->m_listener.onPageVisibilityChanged(page, true);
				}
			});

			m_previouslyCenteredPage = centeredPage;
		}

	}
}
