/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023  spacebub
 * 
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <QObject>
#include <QPushButton>
#include <QHBoxLayout>
#include "ui/ZDLWidget.h"
#include "ui/lists/ZDLListable.h"

class ZDLListWidget : public ZDLWidget {
Q_OBJECT

public:
    explicit ZDLListWidget(ZDLWidget *parent);

    virtual void insert(ZDLListable *item, int index);

    virtual int count();

    virtual void remove(int index);

    virtual ZDLListable *get(int index);

    void doDragDrop(int enabled);

    virtual void newDrop(const QStringList &fileList);

protected slots:

    virtual void addButton();

    virtual void removeButton();

    virtual void upButton();

    virtual void downButton();

    virtual void editButton();

    virtual void editButton(QListWidgetItem *item);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;

    void dragMoveEvent(QDragMoveEvent *event) override;

    void dragLeaveEvent(QDragLeaveEvent *event) override;

    void dropEvent(QDropEvent *event) override;

    QHBoxLayout *buttonRow;
    QPushButton *btnAdd;
    QPushButton *btnRem;
    QPushButton *btnEdt;
    QPushButton *btnUp;
    QPushButton *btnDn;
    QListWidget *pList;
    QList<ZDLListable *> list;
};
