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
#include "selected_creature_spawn.h"
#include "common/make_unique.h"

namespace wowpp
{
	namespace editor
	{
		SelectedCreatureSpawn::SelectedCreatureSpawn(
			ObjectEventHandler eventHandler,
			Ogre::Entity &entity,
			proto::UnitSpawnEntry &entry
			)
			: Selected()
			, m_eventHandler(eventHandler)
			, m_entity(entity)
			, m_entry(entry)
		{
			m_entity.getParentSceneNode()->showBoundingBox(true);
		}

		SelectedCreatureSpawn::~SelectedCreatureSpawn()
		{
		}

		void SelectedCreatureSpawn::translate(const math::Vector3 &delta)
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
		}

		void SelectedCreatureSpawn::rotate(const Vector<float, 4> &delta)
		{
			// Raise event
			rotationChanged(*this);
		}

		void SelectedCreatureSpawn::scale(const math::Vector3 &delta)
		{

			// Raise event
			scaleChanged(*this);
		}

		void SelectedCreatureSpawn::remove()
		{

		}

		void SelectedCreatureSpawn::deselect()
		{
			m_entity.getParentSceneNode()->showBoundingBox(false);
		}

		math::Vector3 SelectedCreatureSpawn::getPosition() const
		{
			return math::Vector3(m_entry.positionx(), m_entry.positiony(), m_entry.positionz());
		}

		Vector<float, 4> SelectedCreatureSpawn::getOrientation() const
		{
			return Vector<float, 4>(1.0f, 0.0f, 0.0f, 0.0f);
		}

		math::Vector3 SelectedCreatureSpawn::getScale() const
		{
			return math::Vector3(1.0f, 1.0f, 1.0f);
		}
	}
}
