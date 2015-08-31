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
#include <QDialog>
#include <memory>

// Forwards
namespace Ui
{
	class TargetDialog;
}

namespace wowpp
{
	namespace editor
	{
		class EditorApplication;

		/// 
		class TargetDialog : public QDialog
		{
			Q_OBJECT

		public:

			/// 
			explicit TargetDialog(UInt32 target, const QString &text);

			UInt32 getTarget() const;
			QString getTargetName() const;

		private slots:

			void on_buttonBox_accepted();
			void on_targetTypeBox_currentIndexChanged(int);

		private:

			Ui::TargetDialog *m_ui;
		};
	}
}
