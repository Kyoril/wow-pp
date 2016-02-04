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

#include "event_dialog.h"
#include "ui_event_dialog.h"
#include "data/trigger_entry.h"
#include "data/project.h"
#include "templates/basic_template.h"
#include "editor_application.h"
#include "trigger_helper.h"

namespace wowpp
{
	namespace editor
	{
		EventDialog::EventDialog(EditorApplication &app, UInt32 e/* = 0*/)
			: QDialog()
			, m_ui(new Ui::EventDialog)
			, m_app(app)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);
			m_ui->eventBox->setCurrentIndex(e);

			connect(m_ui->eventBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_eventBox_currentIndexChanged(int)));
			on_eventBox_currentIndexChanged(e);
		}

		void EventDialog::on_buttonBox_accepted()
		{
			// TODO
		}

		void EventDialog::on_eventBox_currentIndexChanged(int index)
		{
			m_ui->eventTextLabel->setText(getTriggerEventText(index));
		}

		UInt32 EventDialog::getEvent() const
		{
			return m_ui->eventBox->currentIndex();
		}

	}
}
