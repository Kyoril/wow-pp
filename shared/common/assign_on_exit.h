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

namespace wowpp
{
	/// This class delays the assignment of a value to the moment
	/// of it's destruction.
	template<typename T>
	class AssignOnExit final : boost::noncopyable
	{
	public:

		/// Initializes a new instance of this class.
		/// @param dest The destination where the value should be assigned to.
		/// @param value Value to assign.
		AssignOnExit(T &dest, const T &value)
			: m_dest(dest)
			, m_value(value)
		{
		}

		~AssignOnExit()
		{
			m_dest = m_value;
		}

	private:

		T &m_dest;
		T m_value;
	};
}
