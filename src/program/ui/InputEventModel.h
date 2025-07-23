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

#ifndef LIBTAS_INPUTEVENTMODEL_H_INCLUDED
#define LIBTAS_INPUTEVENTMODEL_H_INCLUDED

#include <QtCore/QAbstractTableModel>
#include <vector>
#include <map>
#include <memory>
#include <sys/types.h>
#include <stdint.h>

#include "shared/inputs/InputEvent.h"

/* Forward declaration */
struct Context;

class InputEventModel : public QAbstractTableModel {
    Q_OBJECT

public:
    InputEventModel(Context* c, QObject *parent = Q_NULLPTR);

    void setEvents(const std::vector<InputEvent>& events);
    void append();
    void duplicate(int min_row, int max_row);
    void remove(int min_row, int max_row);
    void clear();

    std::vector<InputEvent> events;

private:
    Context *context;
    
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

};

#endif
