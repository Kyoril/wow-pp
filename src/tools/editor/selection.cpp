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
#include "selection.h"
#include "common/macros.h"

namespace wowpp
{
	Selection::Selection()
	{
	}

	void Selection::clear()
	{
		for (auto &selected : m_selected)
		{
			selected->deselect();
		}

		m_selected.clear();
		changed();
	}

	void Selection::addSelected(std::unique_ptr<Selected> selected)
	{
		m_selected.push_back(std::move(selected));
		changed();
	}

	void Selection::removeSelected(Index index)
	{
		ASSERT(index < m_selected.size());

		auto it = m_selected.begin();
		std::advance(it, index);

		(*it)->deselect();

		m_selected.erase(it);
		changed();
	}

	const size_t Selection::getSelectedObjectCount() const
	{
		return m_selected.size();
	}

	const Selection::SelectionList & Selection::getSelectedObjects() const
	{
		return m_selected;
	}

	const bool Selection::empty() const
	{
		return m_selected.empty();
	}

}
