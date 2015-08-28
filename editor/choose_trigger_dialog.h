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

#pragma once

#include "common/typedefs.h"
#include "data/project.h"
#include <QDialog>
#include <QItemSelection>
#include <memory>

// Forwards
namespace Ui
{
	class ChooseTriggerDialog;
}

namespace wowpp
{
	namespace editor
	{
		class EditorApplication;

		/// 
		class ChooseTriggerDialog : public QDialog
		{
			Q_OBJECT

		public:

			/// 
			explicit ChooseTriggerDialog(EditorApplication &app);

			/// 
			const TriggerEntry * getSelectedTrigger() const { return m_selected; }

		private slots:

			void on_buttonBox_accepted();
			void onTriggerSelectionChanged(const QItemSelection& selection, const QItemSelection& old);

		private:

			Ui::ChooseTriggerDialog *m_ui;
			EditorApplication &m_app;
			const TriggerEntry *m_selected;
		};
	}
}
