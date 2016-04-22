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
#include <QDialog>
#include <QItemSelection>

// Forwards
namespace Ui
{
	class SpawnDialog;
}

namespace wowpp
{
	namespace proto
	{
		class UnitSpawnEntry;
	}

	namespace editor
	{
		class EditorApplication;

		/// 
		class SpawnDialog : public QDialog
		{
			Q_OBJECT

		public:

			/// 
			explicit SpawnDialog(EditorApplication &app, proto::UnitSpawnEntry &spawn);

		private slots:

			void on_buttonBox_accepted();

		private:

			Ui::SpawnDialog *m_ui;
			EditorApplication &m_app;
			proto::UnitSpawnEntry &m_spawn;
		};
	}
}
