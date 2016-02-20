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

#include "crash_handler.h"
#include "version.h"
#include "log/default_log_levels.h"
#include <memory>

#ifdef _MSC_VER
#	include <windows.h>
#	include <dbghelp.h>
namespace wowpp
{
	const String CrashHandler::NativeCrashDumpEnding = ".dmp";

	typedef BOOL (__stdcall *MiniDumpWriteDump_t)(
	    IN HANDLE hProcess,
	    IN DWORD ProcessId,
	    IN HANDLE hFile,
	    IN MINIDUMP_TYPE DumpType,
	    IN CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, OPTIONAL
	    IN CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam, OPTIONAL
	    IN CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam OPTIONAL
	);

	static MiniDumpWriteDump_t g_MiniDumpWriteDump = 0;


	static void saveMiniDumpIfEnabled(LPEXCEPTION_POINTERS ex)
	{
		const auto &fileName = CrashHandler::get().getDumpFilePath();
		if (!fileName ||
		        !g_MiniDumpWriteDump)
		{
			return;
		}

#ifdef _M_IX86
		if (ex->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW)
		{
			// be sure that we have enought space...
			static char MyStack[1024 * 128];
			// it assumes that DS and SS are the same!!! (this is the case for Win32)
			// change the stack only if the selectors are the same (this is the case for Win32)
			//__asm push offset MyStack[1024*128];
			//__asm pop esp;
			__asm mov eax, offset MyStack[1024*128];
			__asm mov esp, eax;
		}
#endif

		HANDLE hFile = CreateFileA(fileName->c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			return;
		}

		MINIDUMP_EXCEPTION_INFORMATION stMDEI;
		stMDEI.ThreadId = GetCurrentThreadId();
		stMDEI.ExceptionPointers = ex;
		stMDEI.ClientPointers = TRUE;

		const auto type = static_cast<MINIDUMP_TYPE>(
		                      MiniDumpNormal |
		                      //MiniDumpWithProcessThreadData |
		                      //MiniDumpWithThreadInfo |
		                      //MiniDumpWithCodeSegs |
		                      //MiniDumpWithHandleData |
		                      //MiniDumpWithDataSegs |
		                      MiniDumpWithFullMemory |
		                      0
		                  );

		if (g_MiniDumpWriteDump(
		            GetCurrentProcess(),
		            GetCurrentProcessId(),
		            hFile,
		            type,
		            &stMDEI,
		            NULL,
		            NULL
		        ))
		{
		}

		CloseHandle(hFile);
	}

	static LONG WINAPI handleException(LPEXCEPTION_POINTERS ex)
	{
		saveMiniDumpIfEnabled(ex);

		CrashHandler::get().onCrash();
		return EXCEPTION_CONTINUE_SEARCH;
	}

	static void initializeExceptionHandler()
	{
		auto *const dbghelp = ::LoadLibraryA("dbghelp.dll");
		if (dbghelp)
		{
			g_MiniDumpWriteDump =
			    reinterpret_cast<MiniDumpWriteDump_t>(
			        GetProcAddress(dbghelp, "MiniDumpWriteDump"));
		}

		::SetUnhandledExceptionFilter(handleException);
	}

	void printCallStack(std::ostream &)
	{
		//TODO
	}
}

#elif defined (__MINGW32__)

namespace wowpp
{
	const String CrashHandler::NativeCrashDumpEnding = "";

	static void initializeExceptionHandler()
	{
		//not implemented
	}

	void printCallStack(std::ostream &out)
	{
		//not implemented
	}
}

#elif defined(__linux__) || defined(__APPLE__)
#	include <signal.h>
#	include <execinfo.h>
#	include <cxxabi.h>
namespace wowpp
{
	//no file ending for unix
	const String CrashHandler::NativeCrashDumpEnding;

	namespace
	{
		void handleSignal(int number)
		{
			{
				std::ostringstream fileName;
				fileName.imbue(std::locale(std::cout.getloc(),
				                           new boost::posix_time::time_facet("%Y_%m_%d_%H_%M_%S")));

				const boost::posix_time::ptime currentTime =
				    boost::posix_time::second_clock::local_time();
				fileName << "backtrace_" << currentTime << ".txt";

				std::ofstream backtraceFile(fileName.str());
				if (!backtraceFile)
				{
					return;
				}

				backtraceFile << "Received signal " << number;
				{
					const char *const signalName = strsignal(number);
					if (signalName)
					{
						backtraceFile << " (" << signalName << ")";
					}
				}
				backtraceFile << '\n';
				printCallStack(backtraceFile);
			}

			//std::cerr is bloated, it could be broken at this moment.
			//fprintf is possibly less error-prone.
			std::fprintf(stderr, "Error: Signal %d\n", number);

			CrashHandler::get().onCrash();

			std::abort();
		}

		void initializeExceptionHandler()
		{
			static const std::array<int, 3> HandledSignals =
			{{
					SIGSEGV, SIGFPE, SIGILL,
				}
			};

			for (const int sig : HandledSignals)
			{
				struct sigaction action;
				sigemptyset(&action.sa_mask);
				action.sa_flags = 0;
				action.sa_handler = handleSignal;
				sigaction(sig, &action, nullptr);
			}
		}
	}

	void printCallStack(std::ostream &out)
	{
		std::array<void *, 256> symbols;
		size_t const count = static_cast<size_t>(backtrace(symbols.data(), static_cast<int>(symbols.size())));
		char **const symbol_names = backtrace_symbols(symbols.data(), static_cast<int>(count));

		{
			out << "Built " << GitLastChange << "\n";
			out << "From " << GitCommit << " (" << Revisision << ")\n";

			//reuse the dynamic memory
			std::string function_name;

			for (size_t i = 0; i < count; ++i)
			{
				const char *const symbol_name = symbol_names[i];
				{
					const char *const symbol_name_end = (symbol_name + std::strlen(symbol_name));
					const char *function_begin = std::find(symbol_name, symbol_name_end, '(');
					if (function_begin != symbol_name_end)
					{
						++function_begin;
					}

					const char *const function_end = std::find_if(
					                                     function_begin,
					                                     symbol_name_end,
					                                     [](char c)
					{
						return (c == ')') || (c == '+');
					});

					function_name.assign(function_begin, function_end);
				}

				size_t size;
				int status;
				char *const cpp_name = abi::__cxa_demangle(
				                           function_name.c_str(),
				                           nullptr,
				                           &size,
				                           &status
				                       );

				const char *const printed_name = (cpp_name ? cpp_name : symbol_name);
				out << printed_name << "\n";

				std::free(cpp_name);
			}
		}
		std::free(symbol_names);
	}
}
#else
#error Looks like an unsupported operating system
#endif


namespace wowpp
{
	std::unique_ptr<CrashHandler> CrashHandler::m_instance;


	void CrashHandler::enableDumpFile(String path)
	{
		m_dumpFilePath = path;
	}

	const boost::optional<String> &CrashHandler::getDumpFilePath() const
	{
		return m_dumpFilePath;
	}

	CrashHandler &CrashHandler::get()
	{
		if (!m_instance)
		{
			m_instance.reset(new CrashHandler);
		}
		return *m_instance;
	}

	CrashHandler::CrashHandler()
	{
		initializeExceptionHandler();
	}
}
