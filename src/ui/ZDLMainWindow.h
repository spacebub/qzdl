/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2019  Lcferrum
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

#include <QApplication>
#include <QMainWindow>
#include "ui/ZDLInterface.h"
#include "ui/ZDLSettingsTab.h"

class ZDLMainWindow : public QMainWindow {
Q_OBJECT

public:
    explicit ZDLMainWindow(QWidget *parent = nullptr);

    ~ZDLMainWindow() override;

    ZDLMainWindow(const ZDLMainWindow &) = delete;

    ZDLMainWindow &operator=(const ZDLMainWindow &) = delete;

    ZDLMainWindow(ZDLMainWindow &&) = delete;

    ZDLMainWindow &operator=(ZDLMainWindow &&) = delete;

    void startRead();

    void writeConfig() const;

    static QString getArgumentsString(bool native_sep = false);

    static QStringList getArgumentsList();

    static QString getExecutable();

    void handleImport();

    static QString getWindowTitle();

protected:
    ZDLInterface *intr;
    ZDLSettingsTab *settings;
    QAction *qact2;
public slots:

    void launch();

    void quit();

    void tabChange(int newTab) const;
};

/** The one main window, owned by main(). */
extern ZDLMainWindow *mw;
