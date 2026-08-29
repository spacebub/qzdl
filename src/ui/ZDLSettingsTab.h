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
#include <QCheckBox>
#include "ui/ZDLWidget.h"
#include "ui/lists/ZDLSourcePortList.h"
#include "ui/lists/ZDLIWadList.h"

class ZDLSettingsTab : public ZDLWidget {
Q_OBJECT

public:
    explicit ZDLSettingsTab(QWidget *parent);

    void rebuild() override;

    void newConfig() override;

    void startRead();

    void writeConfig();

protected slots:

    void fileAssociations();

    void reloadConfig();

    void pathToggled(int state) const;

private:
    QLineEdit *alwaysArgs;
    QCheckBox *launchClose;
    ZDLIWadList *iwadList;
    ZDLSourcePortList *sourceList;
    QCheckBox *showPaths;
    QCheckBox *launchZDL;
    QCheckBox *savePaths;
};
