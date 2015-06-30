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

#pragma once

#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include "world_instance.h"
#include "common/id_generator.h"
#include "common/timer_queue.h"
#include "data/data_load_context.h"
#include "data/map_entry.h"
#include <vector>
#include <memory>

namespace wowpp
{
	class Project;
	class PlayerManager;

	/// Manages all available world instances of the server.
	class WorldInstanceManager final : private boost::noncopyable
	{
	public:

		typedef std::vector<std::unique_ptr<WorldInstance>> Instances;

	public:

		explicit WorldInstanceManager(boost::asio::io_service &ioService, 
			PlayerManager &playerManager,
			IdGenerator<UInt32> &idGenerator,
			IdGenerator<UInt64> &objectIdGenerator,
			Project &project,
			UInt32 worldNodeId);

		/// Creates a new world instance of a specific map id.
		WorldInstance *createInstance(const MapEntry &map);
		/// Called once per frame to update all worlds.
		void update(const boost::system::error_code &error);
		/// 
		WorldInstance *getInstanceById(UInt32 instanceId);
		/// 
		WorldInstance *getInstanceByMapId(UInt32 MapId);

		/// 
		inline PlayerManager &getPlayerManager() { return m_playerManager; }
		/// 
		inline TimerQueue &getTimerQueue() { return *m_timerQueue; }

	private:

		void triggerUpdate();

	private:

		boost::asio::io_service &m_ioService;
		PlayerManager &m_playerManager;
		IdGenerator<UInt32> &m_idGenerator;
		IdGenerator<UInt64> &m_objectIdGenerator;
		boost::asio::deadline_timer m_updateTimer;
		Instances m_instances;
		Project &m_project;
		std::unique_ptr<TimerQueue> m_timerQueue;
		UInt32 m_worldNodeId;
	};
}
