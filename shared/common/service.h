#pragma once

#include <functional>

namespace wowpp
{
	namespace create_service_result
	{
		enum Enum
		{
		    IsObsoleteProcess,
		    IsServiceProcess
		};
	}

	///daemonizes the process on Linux
	//calls openLogFiles in the remaining process
	create_service_result::Enum createService(/*
	    const std::function<void ()> &openLogFiles*/);
}
