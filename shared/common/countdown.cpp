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

#include "countdown.h"
#include "timer_queue.h"
#include <functional>
#include <memory>
#include <cassert>

namespace wowpp
{
	struct Countdown::Impl
			: std::enable_shared_from_this<Impl>
	{
		explicit Impl(TimerQueue &timers, Countdown &countdown)
			: m_timers(timers)
			, m_delayCount(0)
			, m_countdown(&countdown)
		{
		}

		void setEnd(GameTime endTime)
		{
			++m_delayCount;

			m_countdown->running = true;
			m_timers.addEvent(
			    std::bind(&Impl::onPossibleEnd, shared_from_this(), m_delayCount),
			    endTime);
		}

		void cancel()
		{
			++m_delayCount;
			if (m_countdown)
			{
				m_countdown->running = false;
			}
		}

		void kill()
		{
			assert(m_countdown);
			m_countdown = nullptr;
		}

	private:

		TimerQueue &m_timers;
		size_t m_delayCount;
		Countdown *m_countdown;


		void onPossibleEnd(size_t delayNumber)
		{
			if (!m_countdown)
			{
				return;
			}

			if (m_delayCount != delayNumber)
			{
				return;
			}

			m_countdown->running = false;
			m_countdown->ended();
		}
	};


	Countdown::Countdown(TimerQueue &timers)
		: running(false)
		, m_impl(std::make_shared<Impl>(timers, *this))
	{
	}

	Countdown::~Countdown()
	{
		m_impl->kill();
		running = false;
	}

	void Countdown::setEnd(GameTime endTime)
	{
		m_impl->setEnd(endTime);
	}

	void Countdown::cancel()
	{
		m_impl->cancel();
	}
}
