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

#include <utility>
#include "config/ZDLConfigurationManager.h"

#include <QFileInfo>
#include <QProcessEnvironment>

#include "ico_icon.xpm"

void ZDLConfigurationManager::init() {
    activeConfig = nullptr;
    cdir = "";
    conf = new ZDLConfiguration();
}

ZDLConfigModel *ZDLConfigurationManager::activeConfig;
QString ZDLConfigurationManager::cdir;
ZDLWidget *ZDLConfigurationManager::zinterface;
QString ZDLConfigurationManager::filename;
ZDLConfiguration *ZDLConfigurationManager::conf;
ZDLConfigurationManager::WhyConfig ZDLConfigurationManager::why;
QStringList ZDLConfigurationManager::argv;
QString ZDLConfigurationManager::exec;

void ZDLConfigurationManager::setExec(QString execu) {
    ZDLConfigurationManager::exec = std::move(execu);
}

QString ZDLConfigurationManager::getExec() {
    return ZDLConfigurationManager::exec;
}

QStringList ZDLConfigurationManager::getArgv() {
    return ZDLConfigurationManager::argv;
}

void ZDLConfigurationManager::setArgv(QStringList args) {
    ZDLConfigurationManager::argv = std::move(args);
}

void ZDLConfigurationManager::setWhy(ZDLConfigurationManager::WhyConfig whyConfig) {
    ZDLConfigurationManager::why = whyConfig;
}

ZDLConfigurationManager::WhyConfig ZDLConfigurationManager::getWhy() {
    return ZDLConfigurationManager::why;
}

void ZDLConfigurationManager::setInterface(ZDLWidget *widget) {
    zinterface = widget;
}

ZDLWidget *ZDLConfigurationManager::getInterface() {
    return zinterface;
}

void ZDLConfigurationManager::setConfig(ZDLConfigModel *model) {
    ZDLConfigurationManager::activeConfig = model;
}

ZDLConfigModel *ZDLConfigurationManager::getConfig() {
    return ZDLConfigurationManager::activeConfig;
}

void ZDLConfigurationManager::setCurrentDirectory(const QString &dir) {
    cdir = dir;
}

QPixmap ZDLConfigurationManager::getIcon() {
    return QPixmap(zdlicon);
}

QString ZDLConfigurationManager::getCurrentDirectory() {
    return cdir;
}

QString ZDLConfigurationManager::getConfigFileName() {
    return filename;
}

void ZDLConfigurationManager::setConfigFileName(QString name) {
    filename = std::move(name);
}

ZDLConfiguration *ZDLConfigurationManager::getConfiguration() {
    return conf;
}

namespace {

/** Every getter degrades to the general last directory, then to nothing. */
const ZDLLastDirs *lastDirs() {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    return (config != nullptr) ? &config->general.lastDirs : nullptr;
}

ZDLLastDirs *mutableLastDirs() {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    return (config != nullptr) ? &config->general.lastDirs : nullptr;
}

QString orGeneral(const QString &specific, const ZDLLastDirs *dirs) {
    return specific.isEmpty() ? dirs->general : specific;
}

/** Stores the containing directory of fileName under both keys. */
void remember(QString ZDLLastDirs::*field, const QString &fileName, bool is_dir = false) {
    ZDLLastDirs *dirs = mutableLastDirs();
    if (dirs == nullptr) {
        return;
    }
    QString const dir = is_dir ? fileName : QFileInfo(fileName).absolutePath();
    dirs->*field = dir;
    dirs->general = dir;
}

}

QString getWadLastDir(bool dwd_first) {
    const ZDLLastDirs *dirs = lastDirs();
    if (dirs == nullptr) {
        return {};
    }

    QProcessEnvironment const env = QProcessEnvironment::systemEnvironment();
    if (dwd_first && env.contains("DOOMWADDIR")) {
        return env.value("DOOMWADDIR");
    }
    if (!dirs->wad.isEmpty()) {
        return dirs->wad;
    }
    if (env.contains("DOOMWADDIR")) {
        return env.value("DOOMWADDIR");
    }
    return dirs->general;
}

QString getSrcLastDir() {
    const ZDLLastDirs *dirs = lastDirs();
    return (dirs != nullptr) ? orGeneral(dirs->src, dirs) : QString();
}

QString getSaveLastDir() {
    const ZDLLastDirs *dirs = lastDirs();
    return (dirs != nullptr) ? orGeneral(dirs->save, dirs) : QString();
}

QString getZdlLastDir() {
    const ZDLLastDirs *dirs = lastDirs();
    return (dirs != nullptr) ? orGeneral(dirs->zdl, dirs) : QString();
}

QString getConfigLastDir() {
    const ZDLLastDirs *dirs = lastDirs();
    return (dirs != nullptr) ? orGeneral(dirs->config, dirs) : QString();
}

QString getLastDir() {
    const ZDLLastDirs *dirs = lastDirs();
    return (dirs != nullptr) ? dirs->general : QString();
}

void saveWadLastDir(const QString &fileName, bool is_dir) {
    remember(&ZDLLastDirs::wad, fileName, is_dir);
}

void saveSrcLastDir(const QString &fileName) {
    remember(&ZDLLastDirs::src, fileName);
}

void saveSaveLastDir(const QString &fileName) {
    remember(&ZDLLastDirs::save, fileName);
}

void saveZdlLastDir(const QString &fileName) {
    remember(&ZDLLastDirs::zdl, fileName);
}

void saveConfigLastDir(const QString &fileName) {
    remember(&ZDLLastDirs::config, fileName);
}

void saveLastDir(const QString &fileName) {
    ZDLLastDirs *dirs = mutableLastDirs();
    if (dirs != nullptr) {
        dirs->general = QFileInfo(fileName).absolutePath();
    }
}
