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

#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include "world_instance.h"
#include "common/id_generator.h"
#include "common/timer_queue.h"
#include <vector>
#include <memory>

namespace wowpp
{
	namespace proto
	{
		class MapEntry;
		class Project;
	}
	namespace game
	{
		struct ITriggerHandler;
	}
	class PlayerManager;
	class Universe;

	/// Manages all available world instances of the server.
	class WorldInstanceManager final : private boost::noncopyable
	{
	public:

		typedef std::vector<std::unique_ptr<WorldInstance>> Instances;

	public:

		explicit WorldInstanceManager(boost::asio::io_service &ioService,
			Universe &universe,
			game::ITriggerHandler &triggerHandler,
			IdGenerator<UInt32> &idGenerator,
			IdGenerator<UInt64> &objectIdGenerator,
			proto::Project &project,
			UInt32 worldNodeId,
			const String &dataPath);

		/// Creates a new world instance of a specific map id.
		WorldInstance *createInstance(const proto::MapEntry &map);
		/// Called once per frame to update all worlds.
		void update(const boost::system::error_code &error);
		/// 
		WorldInstance *getInstanceById(UInt32 instanceId);
		/// 
		WorldInstance *getInstanceByMapId(UInt32 MapId);
		Universe &getUniverse() { return m_universe;  }

	private:

		void triggerUpdate();

	private:

		boost::asio::io_service &m_ioService;
		Universe &m_universe;
		game::ITriggerHandler &m_triggerHandler;
		IdGenerator<UInt32> &m_idGenerator;
		IdGenerator<UInt64> &m_objectIdGenerator;
		boost::asio::deadline_timer m_updateTimer;
		Instances m_instances;
		proto::Project &m_project;
		UInt32 m_worldNodeId;
		const String &m_dataPath;
	};
}
