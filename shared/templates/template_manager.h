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
#include <boost/noncopyable.hpp>
#include <unordered_map>
#include <vector>

namespace wowpp
{
	template <class TTemplate, class Ptr>
	struct TemplateManager : boost::noncopyable
	{
		typedef TTemplate Template;
		typedef typename Template::Identifier Identifier;
		typedef Ptr TemplatePtr;
		typedef std::vector<TemplatePtr> Templates;
		typedef std::unordered_map<Identifier, Template *> TemplatesById;


		void add(TemplatePtr temp)
		{
			Template &ref = *temp;
			m_templates.push_back(std::move(temp));
			m_byId.insert(std::make_pair(ref.id, &ref));
		}

		void remove(Identifier templateId)
		{
			const auto it = m_byId.find(templateId);
			if (it != m_byId.end())
			{
				m_byId.erase(it);
			}

			for (auto it2 = m_templates.begin(); it2 != m_templates.end(); ++it2)
			{
				if ((*it2)->id == templateId)
				{
					it2 = m_templates.erase(it2);
					break;
				}
			}
		}

		void clear()
		{
			m_byId.clear();
			m_templates.clear();
		}

		const Template *getById(Identifier id) const
		{
			const auto i = m_byId.find(id);
			return (i != m_byId.end()) ? i->second : nullptr;
		}

		Template *getEditableById(Identifier id)
		{
			const auto i = m_byId.find(id);
			return (i != m_byId.end()) ? i->second : nullptr;
		}

		const Template *getAny() const
		{
			return m_templates.empty() ? nullptr : &m_templates.front();
		}

		const Templates &getTemplates() const
		{
			return m_templates;
		}

		Templates &getTemplates()
		{
			return m_templates;
		}

		const TemplatesById &getTemplatesById() const
		{
			return m_byId;
		}

	private:

		Templates m_templates;
		TemplatesById m_byId;
	};
}
