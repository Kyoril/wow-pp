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

#include "clock.h"
#include "simple.hpp"

namespace wowpp
{
	class TimerQueue;

	/// Represents a countdown method which can be used as a timer.
	class Countdown
	{
	public:

		typedef simple::signal<void ()> EndSignal;

		/// This signal is fired when the countdown ended.
		EndSignal ended;
		/// Determines whether the countdown is still running.
		bool running;

		/// Default constructor.
		/// @param timers The timer queue to use.
		explicit Countdown(TimerQueue &timers);
		/// Destructor. This will also stop the countdown if still running.
		~Countdown();

		/// Sets the new end of this timer in milliseconds (see getCurrentTime() in clock.h).
		/// If the countdown isn't running, this will start the countdown. If the countdown is
		/// running, this will expand (or shorten) the execution time.
		void setEnd(GameTime endTime);
		/// Cancels the countdown if it is running. Note that this will not trigger the ended
		/// signal.
		void cancel();

	private:

		struct Impl;

		std::shared_ptr<Impl> m_impl;
	};
}
