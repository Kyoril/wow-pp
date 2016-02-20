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
#include <boost/noncopyable.hpp>
#include <boost/signals2.hpp>

namespace wowpp
{
	class CrashHandler : public boost::noncopyable
	{
	public:

		typedef boost::signals2::signal<void ()> CrashSignal;

		CrashSignal onCrash;

		static const String NativeCrashDumpEnding;


		void enableDumpFile(String path);
		const boost::optional<String> &getDumpFilePath() const;

		static CrashHandler &get();

	private:

		boost::optional<String> m_dumpFilePath;

		static std::unique_ptr<CrashHandler> m_instance;

		CrashHandler();
	};

	void printCallStack(std::ostream &out);
}
