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
#include "tile_index.h"
#include "common/linear_set.h"
#include "game/tile_subscriber.h"

namespace wowpp
{
	class GameObject;

	class VisibilityTile
	{
	public:

		typedef LinearSet<GameObject *> GameObjects;
		typedef LinearSet<ITileSubscriber *> Watchers;

	public:
		explicit VisibilityTile();
		virtual ~VisibilityTile();

		inline void setPosition(const TileIndex2D &position) {
			m_position = position;
		}
		inline const TileIndex2D &getPosition() const {
			return m_position;
		}
		inline GameObjects &getGameObjects() {
			return m_objects;
		}
		inline const GameObjects &getGameObjects() const {
			return m_objects;
		}
		inline Watchers &getWatchers() {
			return m_watchers;
		}
		inline const Watchers &getWatchers() const {
			return m_watchers;
		}

	private:

		TileIndex2D m_position;
		GameObjects m_objects;
		Watchers m_watchers;
	};
}
