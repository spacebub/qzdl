/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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
#include <QDialog>
#include <QLineEdit>
#include "ui/lists/ZDLNameListable.h"
#include "wad/ZDLFileInfo.h"

class ZDLNameInput : public QDialog {
Q_OBJECT

public:
    ZDLNameInput(QWidget *parent, QString last_used_dir, ZDLFileInfo *zdl_fi, bool alllow_dirs, bool allow_params);

    [[nodiscard]] QString getName() const;

    [[nodiscard]] QString getParams() const;

    [[nodiscard]] QString getFile() const;

    void setFilter(const QString &inFilters);

    void basedOff(ZDLNameListable *listable) const;

    void fromUrl(const QUrl &url) const;

public slots:

    void browse();

    void okClick();

protected:
    ZDLFileInfo *zdl_fi;
    QString last_used_dir;
    bool alllow_dirs;
    int params_offset;
    QLineEdit *lname;
    QLineEdit *lparams;
    QLineEdit *lfile;
    QString filters;
    QPushButton *btnBrowse;
};
