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
#include <boost/noncopyable.hpp>
#include <vector>
#include <memory>

namespace wowpp
{
	class World;

	/// Manages all connected worlds.
	class WorldManager : public boost::noncopyable
	{
	public:

		typedef std::vector<std::unique_ptr<World>> Worlds;

	public:

		/// Initializes a new instance of the world manager class.
		/// @param playerCapacity The maximum number of connections that can be connected at the same time.
		explicit WorldManager(
		    size_t playerCapacity
		);
		~WorldManager();

		/// Notifies the manager that a world node has been disconnected which will
		/// delete the player instance.
		void worldDisconnected(World &world);
		/// Gets a dynamic array containing all connected player instances.
		const Worlds &getWorlds() const;
		/// Determines whether the player capacity limit has been reached.
		bool hasWorldCapacityBeenReached() const;
		/// Adds a new world instance to the manager.
		void addWorld(std::unique_ptr<World> added);
		/// Gets a world which supports a specific map id if available.
		World *getWorldByMapId(UInt32 mapId);
		/// Gets a world which supports a specific instance id if available.
		World *getWorldByInstanceId(UInt32 instanceId);

	private:

		Worlds m_worlds;
		size_t m_worldCapacity;
	};
}
