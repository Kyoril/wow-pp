
#pragma once

#include <queue>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

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
			boost::mutex::scoped_lock lock(m_workMutex);
			m_work.push(work);
			m_workToDo.notify_one();
		}

		void stop()
		{
			boost::mutex::scoped_lock lock(m_workMutex);
			m_run = false;
			m_workToDo.notify_one();
		}

		bool isWorking()
		{
			boost::mutex::scoped_lock lock(m_workMutex);
			return !m_work.empty();
		}

		bool hasResults()
		{
			boost::mutex::scoped_lock lock(m_workMutex);
			return !m_results.empty();
		}

		bool getResult(TResult &out_result)
		{
			boost::mutex::scoped_lock lock(m_resultsMutex);
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
					boost::unique_lock<boost::mutex> lock(m_workMutex);

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
					boost::mutex::scoped_lock lock(m_resultsMutex);
					m_results.push(result);
				}
			}
		}

	private:

		typedef std::queue<TWork> Work;
		typedef std::queue<TResult> Results;


		bool m_run;
		Work m_work;
		boost::mutex m_workMutex;
		boost::condition_variable m_workToDo;
		Results m_results;
		boost::mutex m_resultsMutex;
	};
}
