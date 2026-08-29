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

#include <QDir>
#include <QFileInfo>

#include "core/ZDLNullDevice.h"
#include "config/ZDLConfigurationManager.h"
#include "config/ZDLIniImport.h"
#include "ui/ZDLMainWindow.h"

#if defined(_WIN32)
#include "windows.h"
#endif

ZDLMainWindow *mw;

namespace {

/** Path of the .json that sits beside a legacy .ini. */
QString jsonSiblingOf(const QString &iniPath) {
    QFileInfo const info(iniPath);
    return info.dir().filePath(info.completeBaseName() + ".json");
}

/** True when a config file exists and holds more than a stub. */
bool hasContent(const QString &path) {
    QFileInfo const info(path);
    return info.exists() && info.size() > 20;
}

/** First of the candidate paths that actually holds a config, or empty. */
QString firstExisting(const QStringList &paths) {
    for (const QString &path: paths) {
        if (hasContent(path)) {
            return path;
        }
    }
    return {};
}

/**
 * Reads jsonPath, falling back to migrating the legacy iniPath beside it when
 * the JSON isn't there yet.  A migrated config is written straight back out so
 * the .json exists from then on; the .ini is left untouched.
 */
bool loadConfig(const QString &jsonPath, const QString &iniPath, ZDLConfigModel &model) {
    QString error;
    if (QFile::exists(jsonPath)) {
        if (model.load(jsonPath, &error)) {
            return true;
        }
        LOGDATA() << "Failed to read " << jsonPath << ": " << error << Qt::endl;
        return false;
    }

    if (!iniPath.isEmpty() && ZDLIniImport::loadLegacyFile(iniPath, model)) {
        LOGDATA() << "Migrating " << iniPath << " to " << jsonPath << Qt::endl;
        if (!model.save(jsonPath, &error)) {
            LOGDATA() << "Could not write migrated config: " << error << Qt::endl;
        }
        return true;
    }

    return false;
}

}

QDebug *zdlDebug;

#if defined(_WIN32)
extern Q_CORE_EXPORT int qt_ntfs_permission_lookup;
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

int main(int argc, char **argv) {
    QStringList eatenArgs;
    for (int i = 1; i < argc; i++) {
        eatenArgs << argv[i];
    }
    ZDLNullDevice nullDev;
#if defined(ZDL_BLACKBOX)
    QFile *loggingFile = nullptr;
    zdlDebug = nullptr;
    int logger = eatenArgs.indexOf("--enable-logger");
    
    if(logger >= 0){
        eatenArgs.removeAt(logger);
        loggingFile = new QFile("zdl.log");
        if(loggingFile->exists()){
            loggingFile->remove();
        }
        loggingFile->open(QIODevice::ReadWrite);
        zdlDebug = new QDebug(loggingFile);
        qDebug() << "Logger is enabled";
    }else{
        zdlDebug = new QDebug(&nullDev);
    }
#else
    zdlDebug = new QDebug(&nullDev);
#endif
    LOGDATA() << "ZDL" << " booting at " << QDateTime::currentDateTime().toString() << Qt::endl;

#if defined(Q_WS_MAC)
    QFont::insertSubstitution(".Lucida Grande UI", "Lucida Grande");
#endif

    QApplication a(argc, argv);

    ZDLConfigurationManager::setArgv(eatenArgs);
    {
        QFileInfo const fullPath(argv[0]);
        LOGDATA() << "Executable path: " << fullPath.canonicalFilePath() << Qt::endl;
        ZDLConfigurationManager::setExec(fullPath.canonicalFilePath());
    }

    LOGDATA() << "ZDL Version: " << ZDL_PRIVATE_VERSION_STRING << Qt::endl;
    LOGDATA() << "Built on " << __DATE__ << " at " << __TIME__ << Qt::endl;
#if ZDL_DEV_BUILD == 1
    LOGDATA() << "This is development build" << Qt::endl;
#endif

    QDir const cwd = QDir::current();
    ZDLConfigurationManager::init();
    ZDLConfigurationManager::setCurrentDirectory(cwd.absolutePath());

    const ZDLConfiguration *conf = ZDLConfigurationManager::getConfiguration();
    auto *config = new ZDLConfigModel();
    ZDLConfigurationManager::setConfigFileName("");
    ZDLConfigurationManager::setWhy(ZDLConfigurationManager::UNKNOWN);

    // Set whenever the config we settle on still lives in the old INI format.
    QString legacySource;

    // A config named on the command line wins over everything else.
    for (int i = 0; i < eatenArgs.size(); i++) {
        QString const arg = eatenArgs[i];
        if (arg.endsWith(".json", Qt::CaseInsensitive)) {
            ZDLConfigurationManager::setConfigFileName(arg);
        } else if (arg.endsWith(".ini", Qt::CaseInsensitive)) {
            // Legacy configs still work; they migrate to a .json beside them.
            legacySource = arg;
            ZDLConfigurationManager::setConfigFileName(jsonSiblingOf(arg));
        } else {
            continue;
        }
        LOGDATA() << "Loading command line configuration " << arg << Qt::endl;
        eatenArgs.removeAt(i);
        ZDLConfigurationManager::setWhy(ZDLConfigurationManager::USER_SPECIFIED);
        break;
    }

    if (ZDLConfigurationManager::getConfigFileName().isEmpty() && (conf != nullptr)) {
        QString const userJson = conf->getPath(ZDLConfiguration::CONF_USER);
        QString const userIni = firstExisting(conf->getLegacyPaths(ZDLConfiguration::CONF_USER));

        if (hasContent(userJson) || !userIni.isEmpty()) {
            // Migration happens here if needed, so the check below sees the
            // setting whichever format it came from.
            ZDLConfigModel probe;
            if (loadConfig(userJson, userIni, probe) && !probe.general.noUserConf) {
                ZDLConfigurationManager::setConfigFileName(userJson);
                legacySource = userIni;
                LOGDATA() << "Using user-level config file at " << userJson << Qt::endl;
            } else {
                LOGDATA() << "Config file specified noUserConf, or could not be read" << Qt::endl;
            }
        } else {
            LOGDATA() << "No user config file with any content" << Qt::endl;
        }
    }

    // Portable mode: a config sitting next to the executable.  On platforms
    // where that is already the per user location, this step has nothing to
    // add, and skipping it keeps the noUserConf check above authoritative.
    if (ZDLConfigurationManager::getConfigFileName().isEmpty() && (conf != nullptr)
        && QFileInfo(conf->getPath(ZDLConfiguration::CONF_USER)).absolutePath()
           != QFileInfo(ZDLConfigurationManager::getExec()).absolutePath()) {
        QDir const exe_dir(QFileInfo(ZDLConfigurationManager::getExec()).dir());
        if (exe_dir.exists("zdl.json")) {
            ZDLConfigurationManager::setConfigFileName(exe_dir.filePath("zdl.json"));
        } else if (exe_dir.exists("zdl.ini")) {
            legacySource = exe_dir.filePath("zdl.ini");
            ZDLConfigurationManager::setConfigFileName(exe_dir.filePath("zdl.json"));
        }
        if (!ZDLConfigurationManager::getConfigFileName().isEmpty()) {
            LOGDATA() << "Using config next to the executable at "
                      << ZDLConfigurationManager::getConfigFileName() << Qt::endl;
        }
    }

    if (ZDLConfigurationManager::getConfigFileName().isEmpty()) {
        if (conf != nullptr) {
            ZDLConfigurationManager::setConfigFileName(conf->getPath(ZDLConfiguration::CONF_USER));
            legacySource = firstExisting(conf->getLegacyPaths(ZDLConfiguration::CONF_USER));
        } else {
            ZDLConfigurationManager::setConfigFileName("zdl.json");
        }
        LOGDATA() << "Falling back on " << ZDLConfigurationManager::getConfigFileName() << Qt::endl;
    }

    loadConfig(ZDLConfigurationManager::getConfigFileName(), legacySource, *config);
    ZDLConfigurationManager::setConfig(config);

    bool clear_on_args = true;
    bool hasZDLFile = false;

    for (const QString &item: eatenArgs) {
        if (item.endsWith(".zdl", Qt::CaseInsensitive)) {
            LOGDATA() << "Found a .zdl on the command line, adding it as a profile" << Qt::endl;
            ZDLProfile profile;
            if (ZDLIniImport::loadZdlFile(item, profile)) {
                profile.name = config->uniqueProfileName(profile.name);
                config->profiles.append(profile);
                config->setActiveProfile(profile.id);
                hasZDLFile = true;
                clear_on_args = false;
            }
            break;
        }
    }

    for (const QString &item: eatenArgs) {
        if (item.endsWith(".zdl", Qt::CaseInsensitive) || item.endsWith(".json", Qt::CaseInsensitive)
            || item.endsWith(".ini", Qt::CaseInsensitive) || item.startsWith("-"))
            continue;

        // Files given on the command line replace the remembered list, unless a
        // .zdl already provided one.
        if (clear_on_args) {
            config->activeProfile().files.clear();
            clear_on_args = false;
        }

        config->activeProfile().files.append(ZDLFileEntry{item, true});
    }

    mw = new ZDLMainWindow();
    mw->show();
    QObject::connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));
    mw->startRead();

    if (hasZDLFile && config->general.launchZdlImmediately) {
        LOGDATA() << "A .zdl file was passed as a command line option, launching NOW" << Qt::endl;
        mw->launch();
        LOGDATA() << "ZDL QUIT" << Qt::endl;
        return 0;
    }

    mw->handleImport();
    LOGDATA() << "-----------------------------------" << Qt::endl;
    int const ret = QApplication::exec();
    LOGDATA() << "-----------------------------------" << Qt::endl;
    LOGDATA() << "Starting shutdown" << Qt::endl;
    if (ret != 0) {
        LOGDATA() << "ZDL QUIT, ERROR CONDITION" << Qt::endl;
        return ret;
    }
    mw->writeConfig();
    QString const qscwd = ZDLConfigurationManager::getCurrentDirectory();
    config = ZDLConfigurationManager::getConfig();
    QDir::setCurrent(qscwd);
    delete mw;

    if (config != nullptr) {
        if (!config->general.rememberFileList) {
            for (ZDLProfile &profile: config->profiles) {
                profile.files.clear();
            }
        }

        QString error;
        if (!config->save(ZDLConfigurationManager::getConfigFileName(), &error)) {
            LOGDATA() << "Failed to save configuration: " << error << Qt::endl;
        }
    }

    LOGDATA() << "ZDL QUIT" << Qt::endl;
    return ret;
}
