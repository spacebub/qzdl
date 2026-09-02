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
#include <QVBoxLayout>
#include "ui/ZDLWidget.h"
#include "ui/ZDLMultiPane.h"

class ZDLInterface : public ZDLWidget {
Q_OBJECT

public:
    explicit ZDLInterface(QWidget *parent = nullptr);

    void startRead();

    void writeConfig();

    void newConfig() override;

    void rebuild() override;

public slots:

    void loadZdlFile();

    void saveZdlFile();

private slots:

    void sendSignals();

    void mclick();

    static void launch();

    void saveConfigFile();

    void loadConfigFile();

    void aboutClick();

    void showCommandline();

    static void exitzdl();

    static void clearAllFields();

    static void clearAllPWads();

    void clearEverything();

    void importCurrentConfig();

    void importLegacyIni();

    /* Profile selector */

    void profileSelected(int index);

    static void newProfile();

    static void duplicateProfile();

    void renameProfile();

    void deleteProfile();

private:
    QLayout *getProfilePane();

    /** Flushes the current widgets, makes id active, and reloads the UI. */
    static void switchToProfile(const QString &id);

    void refreshProfileCombo() const;

    QLayout *getBottomPane();

    QLayout *getButtonPane();

    QLayout *getTopPane();

    void buttonPaneNewConfig() const;

    void bottomPaneNewConfig() const;

    QPushButton *btnEpr{};
    QPushButton *btnZDL{};
    QPushButton *btnLaunch{};
    QVBoxLayout *box;
    ZDLMultiPane *mpane{nullptr};
    QLineEdit *extraArgs{};
    QComboBox *profileCombo{};
};
