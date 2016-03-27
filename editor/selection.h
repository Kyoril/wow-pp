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

#pragma once

#include "selected.h"

namespace wowpp
{
	/// @brief Base class for selection.
	class Selection final
	{
	public:

		typedef boost::signals2::signal<void()> SelectionChangedSignal;

		SelectionChangedSignal changed;

		typedef size_t Index;
		// List is used here, since in the most cases, we won't have extremely large
		// selections (so iteration is not a big problem). Also, all operations on selected
		// objects would effect ALL selected objects so iteration would be required anyway.
		typedef std::list<std::unique_ptr<Selected>> SelectionList;

	public:

		explicit Selection();

		/// Clears the selection.
		void clear();
		/// Adds an object to the selection.
		void addSelected(std::unique_ptr<Selected> selected);
		/// Removes an object from the selection.
		void removeSelected(Index index);
		/// Returns the number of selected objects.
		/// @returns Number of selected objects.
		const size_t getSelectedObjectCount() const;
		/// Gets the set of selected objects.
		/// @returns Set of selected objects.
		const SelectionList &getSelectedObjects() const;
		/// Indicates whether the selection is empty.
		/// @returns True if the selection is empty (no object is selected).
		const bool empty() const;

	private:

		SelectionList m_selected;
	};
}
