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

#include "ui/ZDLWidget.h"
#include "config/ZDLConfiguration.h"
#include "config/ZDLConfigModel.h"
#include "core/zdlcommon.h"

class ZDLConfigurationManager {
public:
    enum WhyConfig {
        UNKNOWN, USER_SPECIFIED, USER_CONF, IN_EXEC_DIR, IN_CWD
    };

    static void init();

    static ZDLWidget *getInterface();

    static void setInterface(ZDLWidget *widget);

    static void setWhy(WhyConfig whyConfig);

    static WhyConfig getWhy();

    /** The config model every widget reads from and writes to. */
    static void setConfig(ZDLConfigModel *model);

    static ZDLConfigModel *getConfig();

    static ZDLConfiguration *getConfiguration();

    static void setCurrentDirectory(const QString &dir);

    static QString getCurrentDirectory();

    static QPixmap getIcon();

    static QString getConfigFileName();

    static void setConfigFileName(QString name);

    static QStringList getArgv();

    static void setArgv(QStringList args);

    static QString getExec();

    static void setExec(QString execu);

protected:
    static QString exec;
    static QStringList argv;
    static QString filename;
    static ZDLWidget *zinterface;
    static ZDLConfigModel *activeConfig;
    static QString cdir;
    static ZDLConfiguration *conf;
    static WhyConfig why;
};

/* Remembered file dialog directories.  Each getter falls back on the general
 * "last directory" when its own kind has never been used. */

QString getLastDir();

void saveLastDir(const QString &fileName);

QString getWadLastDir(bool dwd_first = false);

void saveWadLastDir(const QString &fileName, bool is_dir = false);

QString getSrcLastDir();

void saveSrcLastDir(const QString &fileName);

QString getSaveLastDir();

void saveSaveLastDir(const QString &fileName);

QString getZdlLastDir();

void saveZdlLastDir(const QString &fileName);

QString getConfigLastDir();

void saveConfigLastDir(const QString &fileName);
