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

#include <QAbstractTableModel>
#include <QVariant>

namespace wowpp
{
	namespace proto
	{
		class Project;
	}

	namespace editor
	{
		class VariableViewModel final : public QAbstractTableModel
		{
			//Q_OBJECT

		public:

			/// 
			VariableViewModel(proto::Project &project, QObject *parent = nullptr);

			/// @copydoc QAbstractTableModel::headerData()
			QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
			/// @copydoc QAbstractTableModel::rowCount()
			int rowCount(const QModelIndex &parent = QModelIndex()) const override;
			/// @copydoc QAbstractTableModel::columnCount()
			int columnCount(const QModelIndex &parent = QModelIndex()) const override;
			/// @copydoc QAbstractTableModel::data()
			QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
			/// @copydoc QAbstractTableModel::setData()
			bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
			/// @copydoc QAbstractTableModel::flags()
			Qt::ItemFlags flags(const QModelIndex &index) const override;

		private:
			
			proto::Project &m_project;
		};
	}
}
