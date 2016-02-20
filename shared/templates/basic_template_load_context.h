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

#include "common/typedefs.h"
#include "simple_file_format/sff_read_tree.h"
#include "template_manager.h"
#include <boost/format.hpp>
#include <functional>
#include <vector>
#include <array>

namespace wowpp
{
	typedef String::const_iterator DataFileIterator;

	struct BasicTemplateLoadContext
	{
		typedef std::function<void (const String &)> OnError;
		typedef std::function<bool ()> LoadLaterFunction;
		typedef std::vector<LoadLaterFunction> LoadLater;

		OnError onError;
		OnError onWarning;
		LoadLater loadLater;
		UInt32 version;


		virtual ~BasicTemplateLoadContext();
		bool executeLoadLater();
	};


	struct ReadTableWrapper
	{
		typedef sff::read::tree::Table<DataFileIterator> SffTemplateTable;


		const SffTemplateTable &table;


		explicit ReadTableWrapper(const SffTemplateTable &table);
	};

	namespace detail
	{
		template <class T, class F, class Id>
		bool loadTemplatePointerLaterFunction(
		    BasicTemplateLoadContext &context,
		    const T *&ptr,
		    const F &loadFunction,
		    Id id,
		    const String &locationString
		)
		{
			ptr = loadFunction(id);
			if (!ptr)
			{
				context.onError((boost::format("%1%: Could not load a %2% template with id %3%")
				                 % locationString
				                 % (typeid(T).name())
				                 % id
				                ).str());
				return false;
			}

			return true;
		}
	}

	template <class T, class F, class Id>
	void loadTemplatePointerLater(
	    BasicTemplateLoadContext &context,
	    const T *&ptr,
	    const F &loadFunction,
	    Id id,
	    const String &locationString
	)
	{
		context.loadLater.push_back(std::bind(&detail::loadTemplatePointerLaterFunction<T, F, Id>,
		                                      std::ref(context),
		                                      std::ref(ptr),
		                                      loadFunction,
		                                      id,
		                                      locationString
		                                     ));
	}

	template <class T>
	String makeLocationString(const T &object)
	{
		return (boost::format("%1% (%2%)")
		        % typeid(object).name()
		        % object.id
		       ).str();
	}

	bool loadPosition(
	    std::array<float, 3> &position,
	    const sff::read::tree::Array<DataFileIterator> &array);
}
