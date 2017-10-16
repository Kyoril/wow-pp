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

namespace wowpp
{
	/// This class will execute functions in a separate background thread.
	class BackgroundWorker
	{
	public:

		/// Default constructor.
		explicit BackgroundWorker();
		/// Destructor.
		~BackgroundWorker();

		/// Adds a new job to the work queue.
		/// @param work The worker function.
		template <class W>
		void addWork(W &&work)
		{
			m_queue.post(std::forward<W>(work));
		}
		/// Waits until all pending jobs are finished.
		void flush();

	private:

		boost::asio::io_service m_queue;
		std::unique_ptr<boost::asio::io_service::work> m_keepWorkerAlive;
		std::thread m_worker;
	};
}
