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
#include "properties.h"
#include <QDialog>

// Forwards
namespace Ui
{
	class StringEditor;
}

namespace wowpp
{
	namespace editor
	{
		/// Represents the main window of the editor application.
		class StringEditor : public QDialog
		{
			Q_OBJECT

		public:

			/// Initializes a new instance of the main window of the editor application.
			explicit StringEditor(StringProperty &prop);

		private slots:

			void on_buttonBox_accepted();

		private:

			Ui::StringEditor *m_ui;
			StringProperty &m_property;
		};
	}
}
