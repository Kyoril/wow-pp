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
#include "background_worker.h"
#include <mutex>
#include <condition_variable>
#include <thread>

namespace wowpp
{
	BackgroundWorker::BackgroundWorker()
		: m_keepWorkerAlive(new boost::asio::io_service::work(m_queue))
		, m_worker([this]()
	{
		m_queue.run();
	})
	{
	}

	BackgroundWorker::~BackgroundWorker()
	{
		flush();
		m_keepWorkerAlive.reset();
		m_worker.join();
	}

	void BackgroundWorker::flush()
	{
		//this works with exactly one worker thread
		std::mutex mutex;
		bool hasFinished = false;
		std::condition_variable finished;
		m_queue.post(
		    [&]()
		{
			{
				std::unique_lock<std::mutex> lock(mutex);
				hasFinished = true;
				finished.notify_one();
			}
		});
		std::unique_lock<std::mutex> lock(mutex);
		while (!hasFinished)
		{
			finished.wait(lock);
		}
	}
}
