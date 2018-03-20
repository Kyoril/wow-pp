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
	template <class TResult, class TWork>
	struct WorkQueue
	{
	public:

		WorkQueue()
			: m_run(true)
		{
		}

		void addWork(TWork work)
		{
			std::mutex::scoped_lock lock(m_workMutex);
			m_work.push(work);
			m_workToDo.notify_one();
		}

		void stop()
		{
			std::mutex::scoped_lock lock(m_workMutex);
			m_run = false;
			m_workToDo.notify_one();
		}

		bool isWorking()
		{
			std::mutex::scoped_lock lock(m_workMutex);
			return !m_work.empty();
		}

		bool hasResults()
		{
			std::mutex::scoped_lock lock(m_workMutex);
			return !m_results.empty();
		}

		bool getResult(TResult &out_result)
		{
			std::mutex::scoped_lock lock(m_resultsMutex);
			if (m_results.empty())
			{
				return false;
			}
			else
			{
				out_result = m_results.front();
				m_results.pop();
				return true;
			}
		}

		void run()
		{
			while (true)
			{
				TWork work;

				{
					std::unique_lock<std::mutex> lock(m_workMutex);

					while (m_run &&
					        m_work.empty())
					{
						m_workToDo.wait(lock);
					}

					if (!m_run)
					{
						return;
					}

					work = m_work.front();
					m_work.pop();
				}

				const TResult result = work();

				{
					std::mutex::scoped_lock lock(m_resultsMutex);
					m_results.push(result);
				}
			}
		}

	private:

		typedef std::queue<TWork> Work;
		typedef std::queue<TResult> Results;


		bool m_run;
		Work m_work;
		std::mutex m_workMutex;
		std::condition_variable m_workToDo;
		Results m_results;
		std::mutex m_resultsMutex;
	};
}
