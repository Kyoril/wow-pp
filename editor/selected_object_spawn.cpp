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
#include "selected_object_spawn.h"
#include "common/make_unique.h"
#include "proto_data/project.h"

namespace wowpp
{
	namespace editor
	{
		SelectedObjectSpawn::SelectedObjectSpawn(
			proto::MapEntry &map,
			ObjectEventHandler eventHandler,
			Ogre::Entity &entity,
			proto::ObjectSpawnEntry &entry
			)
			: Selected()
			, m_map(map)
			, m_eventHandler(eventHandler)
			, m_entity(entity)
			, m_entry(entry)
		{
			m_entity.getParentSceneNode()->showBoundingBox(true);
		}

		SelectedObjectSpawn::~SelectedObjectSpawn()
		{
		}

		void SelectedObjectSpawn::translate(const math::Vector3 &delta)
		{
			m_entry.set_positionx(m_entry.positionx() + delta.x);
			m_entry.set_positiony(m_entry.positiony() + delta.y);
			m_entry.set_positionz(m_entry.positionz() + delta.z);

			m_entity.getParentSceneNode()->setPosition(
				Ogre::Vector3(m_entry.positionx(),
					m_entry.positiony(),
					m_entry.positionz()));

			// Raise event
			positionChanged(*this);

			// Changed
			m_eventHandler();
		}

		void SelectedObjectSpawn::rotate(const Vector<float, 4> &delta)
		{
			Ogre::Quaternion qOri(delta[0], delta[1], delta[2], delta[3]);

			// TODO: Apply rotation (but only on the z axis, which is the yaw axis!)

			// Raise event
			rotationChanged(*this);
		}

		void SelectedObjectSpawn::scale(const math::Vector3 &delta)
		{
			// Unit spawns do not support custom scale right now
		}

		void SelectedObjectSpawn::remove()
		{
			// Remove spawn point from the list of spawns
			auto *spawns = m_map.mutable_objectspawns();
			for (auto it = spawns->begin(); it != spawns->end(); ++it)
			{
				auto &entry = *it;
				if (&entry == &m_entry)
				{
					it = spawns->erase(it);

					// Reset user data
					m_entity.setUserAny(Ogre::Any());

					// Manually destroy entity
					m_entity.setVisible(false);		// This is a hack, find a better way

					// Changed
					m_eventHandler();
					return;
				}
			}
		}

		void SelectedObjectSpawn::deselect()
		{
			m_entity.getParentSceneNode()->showBoundingBox(false);
		}

		math::Vector3 SelectedObjectSpawn::getPosition() const
		{
			return math::Vector3(m_entry.positionx(), m_entry.positiony(), m_entry.positionz());
		}

		Vector<float, 4> SelectedObjectSpawn::getOrientation() const
		{
			return Vector<float, 4>(m_entry.rotationz(), m_entry.rotationw(), m_entry.rotationx(), m_entry.rotationy());
		}

		math::Vector3 SelectedObjectSpawn::getScale() const
		{
			return math::Vector3(1.0f, 1.0f, 1.0f);
		}
	}
}
