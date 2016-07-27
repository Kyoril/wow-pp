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

namespace wowpp
{
	class Editor;

	/// Manages all connected editors.
	class EditorManager : public boost::noncopyable
	{
	public:

		typedef std::vector<std::unique_ptr<Editor>> Editors;

	public:

		/// Initializes a new instance of the editor manager class.
		/// @param capacity The maximum number of connections that can be connected at the same time.
		explicit EditorManager(
		    size_t capacity
		);
		~EditorManager();

		/// Notifies the manager that an editor has been disconnected which will
		/// delete the editor instance.
		void editorDisconnected(Editor &editor);
		/// Gets a dynamic array containing all connected editor instances.
		const Editors &getEditors() const;
		/// Determines whether the editor capacity limit has been reached.
		bool hasEditorCapacityBeenReached() const;
		/// Adds a new player instance to the manager.
		void addEditor(std::unique_ptr<Editor> added);

	private:

		Editors m_editors;
		size_t m_capacity;
	};
}
