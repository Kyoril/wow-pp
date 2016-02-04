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
#include "game/defines.h"
#include "circle.h"
#include <functional>
#include <memory>

namespace wowpp
{
	class UnitWatcher;
	namespace proto
	{
		class MapEntry;
	}
	class GameUnit;

	/// 
	class UnitFinder
	{
	public:
		
		/// 
		/// @param map 
		explicit UnitFinder(const proto::MapEntry &map);
		/// Default destructor.
		virtual ~UnitFinder();

		/// 
		const proto::MapEntry &getMapEntry() const { return m_map; }
		/// 
		/// @param findable 
		virtual void addUnit(GameUnit &findable) = 0;
		/// 
		/// @param findable 
		virtual void removeUnit(GameUnit &findable) = 0;
		/// 
		/// @param updated
		/// @param previousPos 
		virtual void updatePosition(GameUnit &updated, const math::Vector3 &previousPos) = 0;
		/// 
		/// @param shape 
		/// @param resultHandler
		virtual void findUnits(const Circle &shape, const std::function<bool(GameUnit &)> &resultHandler) = 0;
		/// 
		/// @param shape 
		virtual std::unique_ptr<UnitWatcher> watchUnits(const Circle &shape) = 0;

	private:

		const proto::MapEntry &m_map;
	};
}
