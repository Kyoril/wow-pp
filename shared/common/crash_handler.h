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

#include "typedefs.h"

namespace wowpp
{
	/// This method watches for application crashes and fires a signal once
	/// a crash occurs. This can be used to finish some work (like saving all characters or flush
	/// logs) before the application will exit. This class also supports creating a dump
	/// file and call stack log. This class uses the singleton pattern.
	class CrashHandler : public boost::noncopyable
	{
	public:

		typedef boost::signals2::signal<void ()> CrashSignal;

		/// Fired if an application crash occurs.
		CrashSignal onCrash;

		/// File extension to use, depending on the current platform.
		static const String NativeCrashDumpEnding;

		/// Enables the creation of a dump file (if the platform supports this).
		/// @param path The path where the dump file will be created.
		void enableDumpFile(String path);
		/// Gets the current path of the dump file (if any).
		const boost::optional<String> &getDumpFilePath() const;

		/// Singleton getter.
		static CrashHandler &get();

	private:

		/// Path of the dump file (if any).
		boost::optional<String> m_dumpFilePath;

	private:

		/// The one and only instance (static, auto-deleted).
		static std::unique_ptr<CrashHandler> m_instance;

	private:

		/// Private constructor since this is a singleton class.
		CrashHandler();
	};

	/// Writes the call stack into an c++ output stream. This method is implemented depending
	/// on the underlying platform and might do just nothing. Also note, that debug informations
	/// have to be enabled in order to produce a usable call stack.
	void printCallStack(std::ostream &out);
}
