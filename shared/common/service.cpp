#include "service.h"
#include <exception>
#ifdef __linux__
#	include <unistd.h>
#	include <sys/stat.h>
#endif

namespace wowpp
{
	create_service_result::Enum createService(/*
	    const std::function<void ()> &openLogFiles*/)
	{
#ifdef __linux__
		{
			const pid_t pid = fork();
			if (pid < 0)
			{
				throw std::runtime_error("fork failed");
			}
			else if (pid != 0)
			{
				//this is the parent process
				return create_service_result::IsObsoleteProcess;
			}
		}
		umask(0);
#endif

		//openLogFiles();

#ifdef __linux__
		{
			const pid_t sid = setsid();
			if (sid < 0)
			{
				throw std::runtime_error("setsid failed");
			}
		}

		/*if (chdir("/") < 0)
		{
			throw std::runtime_error("chdir failed");
		}*/

		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
#endif

		return create_service_result::IsServiceProcess;
	}
}
