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

#include "typedefs.h"

namespace wowpp
{
	/// This class is used to generate new ids by using an internal counter.
	template<typename T>
	class IdGenerator
	{
	private:

		IdGenerator<T>(const IdGenerator<T> &Other) = delete;
		IdGenerator<T> &operator=(const IdGenerator<T> &Other) = delete;

	public:

		/// Default constructor.
		/// @param idOffset The first id to use. This is needed, if sometimes, a value of 0 indicates
		///                 an invalid value, and thus, the counter should start at 1.
		IdGenerator(T idOffset = 0)
			: m_nextId(idOffset)
		{
		}

		/// Generates a new id.
		/// @returns New id.
		T generateId()
		{
			return m_nextId++;
		}
		/// Notifies the generator about a used id. The generator will then adjust the next generated
		/// id, so that there will be no overlaps.
		void notifyId(T id)
		{
			if (id >= m_nextId) m_nextId = id + 1;
		}

	private:

		T m_nextId;
	};
}
