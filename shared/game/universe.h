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

#include "common/timer_queue.h"
#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <utility>

namespace wowpp
{
	///
	class Universe final : public boost::noncopyable
	{
	public:

		explicit Universe(boost::asio::io_service &ioService, TimerQueue &timers);

		TimerQueue &getTimers() {
			return m_timers;
		}

		template<class Work>
		void post(Work &&work)
		{
			m_ioService.post(std::forward<Work>(work));
		}

	private:

		boost::asio::io_service &m_ioService;
		TimerQueue &m_timers;
	};
}