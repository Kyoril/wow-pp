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

#include "typedefs.h"
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <functional>
#include <queue>

namespace wowpp
{
	class TimerQueue : public boost::noncopyable
	{
	public:

		typedef std::function<void ()> EventCallback;

		explicit TimerQueue(boost::asio::io_service &service);
		GameTime getNow() const;
		void addEvent(EventCallback callback, GameTime time);

	private:

		struct EventEntry
		{
			EventCallback callback;
			GameTime time;

			struct IsLater
			{
				inline bool operator ()(const EventEntry &left, const EventEntry &right) const
				{
					return (left.time > right.time);
				}
			};

			EventEntry()
			{
			}

			EventEntry(const EventCallback &callback, GameTime time)
				: callback(callback)
				, time(time)
			{
			}
		};


		typedef std::priority_queue<EventEntry, std::vector<EventEntry>, EventEntry::IsLater> Queue;
		typedef boost::asio::deadline_timer Timer;

		Timer m_timer;
		boost::optional<GameTime> m_timerTime;
		Queue m_queue;

		void update(const boost::system::error_code &error);
		void setTimer();
	};
}
