//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
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

#include "world_instance_manager.h"
#include "data/project.h"
#include "common/vector.h"
#include "solid_visibility_grid.h"
#include "log/default_log_levels.h"
#include "universe.h"
#include <cassert>

namespace wowpp
{
	WorldInstanceManager::WorldInstanceManager(
		boost::asio::io_service &ioService,
		Universe &universe,
		IdGenerator<UInt32> &idGenerator, 
		IdGenerator<UInt64> &objectIdGenerator,
		Project &project,
		UInt32 worldNodeId)
		: m_ioService(ioService)
		, m_universe(universe)
		, m_idGenerator(idGenerator)
		, m_objectIdGenerator(objectIdGenerator)
		, m_updateTimer(ioService)
		, m_project(project)
		, m_worldNodeId(worldNodeId)
	{
		// Trigger the first update
		triggerUpdate();
	}

	WorldInstance * WorldInstanceManager::createInstance(const MapEntry &map)
	{
		UInt32 instanceId = createMapGUID(m_idGenerator.generateId(), map.id);
		//instanceId |= (m_worldNodeId << 24) & 0xFF;

		// Create world instance
		std::unique_ptr<WorldInstance> instance(new WorldInstance(
			*this,
			m_universe,
			map, 
			instanceId, 
			std::unique_ptr<VisibilityGrid>(new SolidVisibilityGrid(TileIndex2D(64, 64))),
			m_objectIdGenerator,
			std::bind(&RaceEntryManager::getById, &m_project.races, std::placeholders::_1),
			std::bind(&ClassEntryManager::getById, &m_project.classes, std::placeholders::_1),
			std::bind(&LevelEntryManager::getById, &m_project.levels, std::placeholders::_1)));
		m_instances.push_back(std::move(instance));

		// Return result
		assert(!m_instances.empty());
		return m_instances.back().get();
	}

	void WorldInstanceManager::triggerUpdate()
	{
		// Wait for next update
		m_updateTimer.expires_from_now(boost::posix_time::milliseconds(static_cast<Int64>(1000.0 / 33.0)));	// Update ~30 times per second
		m_updateTimer.async_wait(
			std::bind(&WorldInstanceManager::update, this, std::placeholders::_1));
	}

	void WorldInstanceManager::update(const boost::system::error_code &error)
	{
		// Was the timer stopped?
		if (error == boost::asio::error::operation_aborted)
		{
			// Nothing to do here - timer was stopped
			return;
		}
		else if (error)
		{
			// Something bad happened
			ELOG("Error during world instance update loop: " << error);
			return;
		}
		else
		{
			// Iterate through every world instance and update it
			for (auto &instance : m_instances)
			{
				instance->update();
			}

			// Trigger the next update
			triggerUpdate();
		}
	}

	WorldInstance * WorldInstanceManager::getInstanceById(UInt32 instanceId)
	{
		const auto i = std::find_if(
			m_instances.begin(),
			m_instances.end(),
			[instanceId](const std::unique_ptr<WorldInstance> &i)
		{
			return (instanceId == i->getId());
		});

		if (i != m_instances.end())
		{
			return (*i).get();
		}

		return nullptr;
	}

	WorldInstance * WorldInstanceManager::getInstanceByMapId(UInt32 MapId)
	{
		const auto i = std::find_if(
			m_instances.begin(),
			m_instances.end(),
			[MapId](const std::unique_ptr<WorldInstance> &i)
		{
			return (MapId == i->getMapId());
		});

		if (i != m_instances.end())
		{
			return (*i).get();
		}

		return nullptr;
	}
}
