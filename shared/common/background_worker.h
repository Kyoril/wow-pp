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

#include "common/typedefs.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>

namespace wowpp
{
	class BackgroundWorker
	{
	public:

		explicit BackgroundWorker();
		~BackgroundWorker();

		template <class W>
		void addWork(W &&work)
		{
			m_queue.post(std::forward<W>(work));
		}

		void flush();

	private:

		boost::asio::io_service m_queue;
		boost::scoped_ptr<boost::asio::io_service::work> m_keepWorkerAlive;
		boost::thread m_worker;
	};
}
