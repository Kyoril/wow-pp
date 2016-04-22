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

#include "configuration.h"
#include "database.h"

namespace wowpp
{
	/// This class manages all components of the realm server.
	class Program final : public boost::noncopyable
	{
	public:

		/// Creates a new instance of the Program class.
		explicit Program(bool asService);

		/// Starts the server application.
		/// @returns True if the application should be restarted.
		bool run();
		/// Shutdown the server application.
		/// @param restart If set to true, the application will restart.
		void shutdown(bool restart = false);

	private:

		bool m_isService;
		boost::asio::io_service m_ioService;
		Configuration m_configuration;
		std::unique_ptr<IDatabase> m_database;
		bool m_shouldRestart;
	};
}
