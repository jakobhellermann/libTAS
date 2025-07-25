/*
    Copyright 2015-2024 Clément Gallet <clement.gallet@ens-lyon.org>

    This file is part of libTAS.

    libTAS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libTAS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libTAS.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBTAS_RAMWATCHMODEL_H_INCLUDED
#define LIBTAS_RAMWATCHMODEL_H_INCLUDED

#include "ramsearch/RamWatchDetailed.h"

#include <QtCore/QAbstractTableModel>
#include <QtCore/QSettings>
#include <QtCore/QMimeData>
#include <vector>
#include <memory>

class RamWatchModel : public QAbstractTableModel {
    Q_OBJECT

public:
    RamWatchModel(QObject *parent = Q_NULLPTR);

    /* A reference to the vector of addresses to watch */
    std::vector<std::unique_ptr<RamWatchDetailed>> ramwatches;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    Qt::DropActions supportedDropActions() const override;
    Qt::DropActions supportedDragActions() const override;

    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent, int destinationChild) override;

    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;

    void addWatch(std::unique_ptr<RamWatchDetailed> ramwatch);
    void removeWatch(int row);

    void saveSettings(QSettings& watchSettings);
    void loadSettings(QSettings& watchSettings);

    void update();
    void update_frozen();
};

#endif
