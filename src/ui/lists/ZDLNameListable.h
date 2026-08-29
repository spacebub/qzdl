/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
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

#include "ui/lists/ZDLListable.h"
#include <QString>

class ZDLNameListable : public ZDLListable {
Q_OBJECT

public:
    ZDLNameListable(QListWidget *parent, int type, QString qfile, QString name);

    ~ZDLNameListable() override;

    virtual QString getFile();

    QString getName() override;

    void setDisplayName(QString name);

    void setFile(QString file);

protected:
    QString generateName();


    QString fileName;
    QString displayName;
};
