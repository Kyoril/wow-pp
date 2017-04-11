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
#include "timer_queue.h"
#include "macros.h"
#include "clock.h"

namespace wowpp
{
	TimerQueue::TimerQueue(boost::asio::io_service &service)
		: m_timer(service)
	{
	}

	GameTime TimerQueue::getNow() const
	{
		return getCurrentTime();
	}

	void TimerQueue::addEvent(EventCallback callback, GameTime time)
	{
		m_queue.push(EventEntry(callback, time));
		setTimer();
	}

	void TimerQueue::update(const boost::system::error_code &error)
	{
		if (error)
		{
			return;
		}

		m_timerTime.reset();

		const auto now = getNow();
		while (!m_queue.empty())
		{
			const auto &next = m_queue.top();

			if (now >= next.time)
			{
				//ASSERT(getCurrentTime() >= next.time);
				const auto callback = next.callback;
				m_queue.pop();
				callback();
			}
			else
			{
				setTimer();
				break;
			}
		}
	}

	void TimerQueue::setTimer()
	{
		const auto nextEventTime = m_queue.top().time;

		// Is the timer active?
		if (m_timerTime)
		{
			if (nextEventTime < *m_timerTime)
			{
				m_timer.cancel();
			}
			else
			{
				return;
			}
		}

		const auto now = getNow();
		m_timerTime = nextEventTime;
		const auto delay = (std::max(nextEventTime, now) - now);

		m_timer.expires_from_now(boost::posix_time::milliseconds(delay));
		m_timer.async_wait(std::bind(&TimerQueue::update, this, std::placeholders::_1));
	}
}
