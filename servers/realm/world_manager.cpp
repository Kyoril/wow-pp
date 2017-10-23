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
#include "world_manager.h"
#include "world.h"
#include "binary_io/string_sink.h"

namespace wowpp
{
	WorldManager::WorldManager(
	    size_t worldCapacity)
		: m_worldCapacity(worldCapacity)
	{
	}

	WorldManager::~WorldManager()
	{
	}

	void WorldManager::worldDisconnected(World &world)
	{
		const auto w = std::find_if(
		                   m_worlds.begin(),
						   m_worlds.end(),
		                   [&world](const WorldPtr &w)
		{
			return (&world == w.get());
		});
		ASSERT(w != m_worlds.end());
		m_worlds.erase(w);
	}

	const WorldManager::Worlds &WorldManager::getWorlds() const
	{
		return m_worlds;
	}
	
	bool WorldManager::hasWorldCapacityBeenReached() const
	{
		return m_worlds.size() >= m_worldCapacity;
	}

	void WorldManager::addWorld(std::shared_ptr<World> added)
	{
		ASSERT(added);
		m_worlds.push_back(added);
	}

	World * WorldManager::getWorldByMapId(UInt32 mapId)
	{
		const auto w = std::find_if(
			m_worlds.begin(),
			m_worlds.end(),
			[mapId](const WorldPtr &w)
		{
			return w->isMapSupported(mapId);
		});

		if (w != m_worlds.end())
		{
			return w->get();
		}

		return nullptr;
	}

	World * WorldManager::getWorldByInstanceId(UInt32 instanceId)
	{
		const auto w = std::find_if(
			m_worlds.begin(),
			m_worlds.end(),
			[instanceId](const WorldPtr &w)
		{
			return w->hasInstance(instanceId);
		});

		if (w != m_worlds.end())
		{
			return w->get();
		}

		return nullptr;
	}

}
