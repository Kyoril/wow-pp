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

#include "common/typedefs.h"
#include "defines.h"

namespace wowpp
{
	class GameUnit;

	/// This class is meant to control a units movement. This class should be
	/// inherited, so that, for example, a player character will be controlled
	/// by network input, while a creature is controlled by AI input.
	class UnitMover
	{
	public:
		
		/// 
		explicit UnitMover(GameUnit &unit);
		/// 
		virtual ~UnitMover();

		/// 
		GameUnit &getMoved() const { return m_unit; }
		/// 
		virtual void update() = 0;
		/// 
		virtual bool isCurrentlyMoving() const = 0;
		/// 
		virtual void onBeforeLeave();
		/// Interpolates the controlled units position to get the position at 
		/// a specified time.
		/// @param when The time value.
		virtual game::Position getPositionAtTime(GameTime when) = 0;

	private:

		GameUnit &m_unit;
	};
}
