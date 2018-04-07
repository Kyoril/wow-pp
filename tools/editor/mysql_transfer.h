#pragma once

#include "common/typedefs.h"

namespace wowpp
{
	namespace MySQL
	{
		struct Connection;
	}

	namespace editor
	{
		/// 
		struct ITransferProgressWatcher
		{
		public:
			virtual ~ITransferProgressWatcher() {}

		public:
			virtual void reportError(const String& error) = 0;
			virtual void reportProgressChange(float progress) = 0;
		};

		/// Represents an mysql transfer task.
		struct TransferTask
		{
			/// Name of the task that is displayed in the status label.
			String taskName;
			/// Callback which is executed before the transfer starts.
			std::function<void(wowpp::MySQL::Connection& conn)> beforeTransfer;
			/// Callback which is executed when the work should be done.
			std::function<bool(wowpp::MySQL::Connection& conn, ITransferProgressWatcher& watcher)> doWork;
			/// Callback which is executed after the transfer.
			std::function<void()> afterTransfer;
		};
	}
}