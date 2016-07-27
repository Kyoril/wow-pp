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

#include "pch.h"
#include "editor_manager.h"
#include "editor.h"
#include "binary_io/string_sink.h"

namespace wowpp
{
	EditorManager::EditorManager(
	    size_t capacity)
		: m_capacity(capacity)
	{
	}

	EditorManager::~EditorManager()
	{
	}

	void EditorManager::editorDisconnected(Editor &editor)
	{
		const auto p = std::find_if(
		                   m_editors.begin(),
						   m_editors.end(),
		                   [&editor](const std::unique_ptr<Editor> &p)
		{
			return (&editor == p.get());
		});
		assert(p != m_editors.end());
		m_editors.erase(p);
	}

	const EditorManager::Editors &EditorManager::getEditors() const
	{
		return m_editors;
	}
	
	bool EditorManager::hasEditorCapacityBeenReached() const
	{
		return m_editors.size() >= m_capacity;
	}

	void EditorManager::addEditor(std::unique_ptr<Editor> added)
	{
		assert(added);
		m_editors.push_back(std::move(added));
	}
}
