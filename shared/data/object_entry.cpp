//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
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

#include "object_entry.h"
#include "templates/basic_template_load_context.h"
#include "templates/basic_template_save_context.h"

namespace wowpp
{
	ObjectEntry::ObjectEntry()
		: minGold(0)
		, maxGold(0)
		, type(0)
		, displayID(0)
		, factionID(0)
		, scale(1.0f)
		, flags(0)
	{
		data.fill(0);
	}

	bool ObjectEntry::load(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		wrapper.table.tryGetString("name", name);
		wrapper.table.tryGetInteger("type", type);
		wrapper.table.tryGetInteger("display", displayID);
		wrapper.table.tryGetString("caption", caption);
		wrapper.table.tryGetInteger("faction", factionID);
		wrapper.table.tryGetInteger("flags", flags);
		wrapper.table.tryGetInteger("size", scale);
		
		// Data
		data.fill(0);
		const sff::read::tree::Array<DataFileIterator> *dataArray = wrapper.table.getArray("data");
		if (dataArray)
		{
			if (dataArray->getSize() != data.size())
			{
				context.onWarning("Objects data array size mismatch!");
			}

			for (size_t j = 0, d = std::min(data.size(), dataArray->getSize()); j < d; ++j)
			{
				data[j] = dataArray->getInteger<UInt32>(j, 0);
			}
		}

#define MIN_MAX_CHECK(minval, maxval) if (maxval < minval) maxval = minval; else if(minval > maxval) minval = maxval;
		wrapper.table.tryGetInteger("min_gold", minGold);
		wrapper.table.tryGetInteger("max_gold", maxGold);
		MIN_MAX_CHECK(minGold, maxGold);
#undef MIN_MAX_CHECK

		return true;
	}

	void ObjectEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (!name.empty()) context.table.addKey("name", name);
		if (type != 0) context.table.addKey("type", type);
		if (displayID != 0) context.table.addKey("display", displayID);
		if (!caption.empty()) context.table.addKey("caption", caption);
		if (factionID != 0) context.table.addKey("faction", factionID);
		if (flags != 0) context.table.addKey("flags", flags);
		if (scale != 1.0f) context.table.addKey("size", scale);
		if (minGold != 0) context.table.addKey("min_gold", minGold);
		if (maxGold != 0) context.table.addKey("max_gold", maxGold);

		// Write data
		sff::write::Array<char> dataArray(context.table, "data", sff::write::Comma);
		for (const auto &entry : data)
		{
			dataArray.addElement(entry);
		}
		dataArray.finish();
	}
}
