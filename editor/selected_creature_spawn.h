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
#include "shared/proto_data/maps.pb.h"
#include "ogre_wrappers/billboard_set_ptr.h"

namespace wowpp
{
	namespace proto
	{
		class MapEntry;
	}

	namespace editor
	{
		class SelectedCreatureSpawn final : public Selected
		{
		public:

			//TODO: Find a better way to notify the renderer
			typedef std::function<void()> ObjectEventHandler;

			explicit SelectedCreatureSpawn(
				proto::MapEntry &map,
				ObjectEventHandler eventHandler,
				Ogre::Entity &entity,
				proto::UnitSpawnEntry &entry
				);
			~SelectedCreatureSpawn();

			/// @copydoc faudra::Selected::translate()
			void translate(const math::Vector3 &delta) override;
			/// @copydoc faudra::Selected::rotate()
			void rotate(const Vector<float, 4> &delta) override;
			/// @copydoc faudra::Selected::scale()
			void scale(const math::Vector3 &delta) override;
			/// @copydoc faudra::Selected::remove()
			void remove() override;
			/// @copydoc faudra::Selected::deselect()
			void deselect() override;
			/// @copydoc faudra::Selected::getPosition()
			math::Vector3 getPosition() const override;
			/// @copydoc faudra::Selected::getOrientation()
			Vector<float, 4> getOrientation() const override;
			/// @copydoc faudra::Selected::getScale()
			math::Vector3 getScale() const override;

		private:

			proto::MapEntry &m_map;
			ObjectEventHandler m_eventHandler;
			Ogre::Entity &m_entity;
			proto::UnitSpawnEntry &m_entry;
			ogre_utils::BillboardSetPtr m_waypoints;
		};

	}
}
