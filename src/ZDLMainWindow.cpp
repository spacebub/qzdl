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

#include <QRegularExpression>
#include <QMainWindow>
#include <QAction>
#include <QMessageBox>

#include "ZDLInterface.h"
#include "ZDLMainWindow.h"
#include "ZDLConfigurationManager.h"
#include "ZDLImportDialog.h"
#include "ZDLMapFile.h"
#include "ZDLIniImport.h"

#ifdef _WIN32
#include <windows.h>
#else

#include <wordexp.h>

#endif

ZDLMainWindow::~ZDLMainWindow() {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config) {
        config->general.hasWindowSize = true;
        config->general.windowSize = this->size();
        config->general.hasWindowPos = true;
        config->general.windowPos = this->pos();
    }
    LOGDATAO() << "Closing main window" << Qt::endl;
}

QString ZDLMainWindow::getWindowTitle() {
    QString windowTitle = "ZDL";
    windowTitle += " " ZDL_VERSION_STRING;
    ZDLConfiguration *conf = ZDLConfigurationManager::getConfiguration();
    if (conf) {
        QString userConfPath = conf->getPath(ZDLConfiguration::CONF_USER);
        QString currentConf = ZDLConfigurationManager::getConfigFileName();
        if (userConfPath != currentConf) {
            windowTitle += " [" + ZDLConfigurationManager::getConfigFileName() + "]";
        }
    } else {
        windowTitle += ZDLConfigurationManager::getConfigFileName();
    }
    LOGDATAO() << "Returning main window title " << windowTitle << Qt::endl;
    return windowTitle;

}

ZDLMainWindow::ZDLMainWindow(QWidget *parent) :
        QMainWindow(parent) {
    LOGDATAO() << "New main window " << DPTR(this) << Qt::endl;
    QString windowTitle = getWindowTitle();
    setWindowTitle(windowTitle);

    setWindowIcon(ZDLConfigurationManager::getIcon());

    setContentsMargins(0, 2, 0, 0);
    layout()->setContentsMargins(0, 0, 0, 0);
    auto *widget = new QTabWidget(this);

    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config) {
        if (config->general.hasWindowSize) {
            LOGDATAO() << "Resizing to " << config->general.windowSize << Qt::endl;
            this->resize(config->general.windowSize);
        }
        if (config->general.hasWindowPos) {
            LOGDATAO() << "Moving to " << config->general.windowPos << Qt::endl;
            this->move(config->general.windowPos);
        }
    }

    intr = new ZDLInterface(this);
    settings = new ZDLSettingsTab(this);

    widget->setDocumentMode(true);
    widget->addTab(intr, "Launch config");
    widget->addTab(settings, "General settings");
    setCentralWidget(widget);

    auto *qact = new QAction(widget);
    qact->setShortcut(Qt::Key_Return);
    widget->addAction(qact);
    connect(qact, SIGNAL(triggered()), this, SLOT(launch()));

    qact2 = new QAction(widget);
    qact2->setShortcut(Qt::Key_Escape);
    widget->addAction(qact2);

    connect(qact2, SIGNAL(triggered()), this, SLOT(quit()));

    connect(widget, SIGNAL(currentChanged(int)), this, SLOT(tabChange(int)));
    LOGDATAO() << "Main window created." << Qt::endl;
}

void ZDLMainWindow::handleImport() {
#if !defined(NO_IMPORT)
    ZDLConfiguration *conf = ZDLConfigurationManager::getConfiguration();
    if (!conf) {
        return;
    }

    QString userConfPath = conf->getPath(ZDLConfiguration::CONF_USER);
    QString currentConf = ZDLConfigurationManager::getConfigFileName();
    if (userConfPath == currentConf) {
        return;
    }

    LOGDATAO() << "Not currently using user conf" << Qt::endl;
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (!config || config->general.doNotImportThis) {
        LOGDATAO() << "Don't import current config" << Qt::endl;
        return;
    }

    ZDLConfigModel userConfig;
    QFileInfo userFile(userConfPath);
    if (userFile.exists()) {
        LOGDATAO() << "Reading user conf" << Qt::endl;
        userConfig.load(userConfPath);
    }
    if (userConfig.general.noUserConf) {
        LOGDATAO() << "Do not use user conf" << Qt::endl;
        return;
    }

    if (ZDLConfigurationManager::getWhy() == ZDLConfigurationManager::USER_SPECIFIED) {
        LOGDATA() << "The user asked for this config, don't go forward" << Qt::endl;
        return;
    }

    if (userFile.exists() && userFile.size() >= 10) {
        return;
    }
    LOGDATA() << "User conf is small, assuming empty" << Qt::endl;

    ZDLImportDialog importd(this);
    importd.exec();
    if (importd.result() != QDialog::Accepted) {
        return;
    }

    switch (importd.getImportAction()) {
        case ZDLImportDialog::IMPORTNOW: {
            LOGDATAO() << "Importing now" << Qt::endl;
            config->general.importedFrom = currentConf;
            config->general.isImported = true;
            config->general.importDate = QDateTime::currentDateTime().toString(Qt::ISODate);

            QString error;
            if (!config->save(userConfPath, &error)) {
                LOGDATAO() << "Import failed: " << error << Qt::endl;
                QMessageBox::critical(this, "ZDL",
                                      QString("Unable to write the configuration file at %1:\n%2")
                                              .arg(userConfPath, error));
                break;
            }
            ZDLConfigurationManager::setConfigFileName(userConfPath);
            break;
        }
        case ZDLImportDialog::DONOTIMPORTTHIS:
            LOGDATAO() << "Tagging this config as not importable" << Qt::endl;
            config->general.doNotImportThis = true;
            break;
        case ZDLImportDialog::NEVERIMPORT:
            LOGDATAO() << "Setting NEVER IMPORT" << Qt::endl;
            userConfig.general.noUserConf = true;
            userConfig.save(userConfPath);
            break;
        case ZDLImportDialog::ASKLATER:
        case ZDLImportDialog::UNKNOWN:
        default:
            LOGDATAO() << "Not setting anything" << Qt::endl;
            break;
    }
#endif
}

void ZDLMainWindow::tabChange(int newTab) {
    LOGDATAO() << "Tab changed to " << newTab << Qt::endl;
    if (newTab == 0) {
        settings->notifyFromParent(nullptr);
        intr->readFromParent(nullptr);
    } else if (newTab == 1) {
        intr->notifyFromParent(nullptr);
        settings->readFromParent(nullptr);
    }
}

void ZDLMainWindow::quit() {
    LOGDATAO() << "quitting" << Qt::endl;
    writeConfig();
    close();
}

void ZDLMainWindow::launch() {
    LOGDATAO() << "Launching" << Qt::endl;
    writeConfig();
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();

    QString exec = getExecutable();
    if (exec.length() < 1) {
        QMessageBox::warning(this, "ZDL", "Please select a source port.");
        return;
    }
    QFileInfo exec_fi(exec);
    bool no_err = true;

#ifdef _WIN32
    PROCESS_INFORMATION pi={};
    STARTUPINFO si={sizeof(STARTUPINFO), nullptr, nullptr, nullptr, 0, 0, 0, 0, 0, 0, 0, STARTF_USESHOWWINDOW, SW_SHOWNORMAL};

    QString cmdline="\""+QDir::toNativeSeparators(exec_fi.absoluteFilePath())+"\" "+getArgumentsString(true);
    QString cwd=QDir::toNativeSeparators(exec_fi.absolutePath());

    if (CreateProcess(nullptr, (LPWSTR)cmdline.toStdWString().c_str(), nullptr, nullptr, FALSE, NORMAL_PRIORITY_CLASS | CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE, nullptr, cwd.toStdWString().c_str(), &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        QMessageBox::warning(this, "ZDL", "Failed to launch the application executable.");
        no_err=false;
    }
#else
    if (!QProcess::startDetached(exec_fi.absoluteFilePath(), getArgumentsList(), exec_fi.absolutePath())) {
        QMessageBox::warning(this, "ZDL", "Failed to launch the application executable.");
        no_err = false;
    }
#endif
    if (no_err && config && config->general.autoClose) {
        LOGDATAO() << "Asked to exit... closing" << Qt::endl;
        close();
    }
}

QStringList WarpBackwardCompat(const QString &iwad_path, const QString &map_name) {
    if (iwad_path.length()) {
        bool iwad_mapxx = false;

        if (ZDLMapFile *mapfile = ZDLMapFile::getMapFile(iwad_path)) {
            iwad_mapxx = mapfile->isMAPXX();
            delete mapfile;
        }

        QRegularExpressionMatch match;
        if (iwad_mapxx) {
            QRegularExpression mapxx_re("^MAP(\\d\\d)$");
            match = mapxx_re.match(map_name, Qt::CaseInsensitive);
            if (match.hasPartialMatch())
                return QStringList() << "-warp" << match.captured(1);
        } else {
            QRegularExpression exmy_re("^E(\\d)M([1-9])$");
            match = exmy_re.match(map_name, Qt::CaseInsensitive);
            if (match.hasPartialMatch())
                return QStringList() << "-warp" << match.captured(1) << match.captured(2);
        }
    }

    return {};
}

namespace {

/** External files split by how the source port wants them passed. */
struct ClassifiedFiles {
    QStringList pwads;
    QStringList dehs;
    QStringList bexs;
    QStringList autoexecs;
    QStringList lumps;
    /* Which of -deh and -bex goes last, decided by whichever kind appeared
     * last in the list.  The source port applies the later one on top. */
    char dehLast{1};
};

ClassifiedFiles classifyFiles(const QVector<ZDLFileEntry> &files) {
    ClassifiedFiles out;
    for (const ZDLFileEntry &entry: files) {
        // Disabled entries stay in the list but off the command line.
        if (!entry.enabled) {
            continue;
        }
        if (entry.file.endsWith(".bex", Qt::CaseInsensitive)) {
            out.dehLast = 0;
            out.bexs << entry.file;
        } else if (entry.file.endsWith(".deh", Qt::CaseInsensitive)) {
            out.dehLast = 1;
            out.dehs << entry.file;
        } else if (entry.file.endsWith(".cfg", Qt::CaseInsensitive)) {
            out.autoexecs << entry.file;
        } else if (entry.file.endsWith(".lmp", Qt::CaseInsensitive)) {
            out.lumps << entry.file;
        } else {
            out.pwads << entry.file;
        }
    }
    return out;
}

/** Full path of the profile's IWAD, or empty when it names none. */
QString resolveIwadPath(const ZDLConfigModel *config, const ZDLProfile &profile) {
    if (const ZDLNameEntry *iwad = config->findIwad(profile.iwad)) {
        return iwad->file;
    }
    return {};
}

}

#ifdef _WIN32

QString QuoteParam(const QString& param)
{
    //Based on "Everyone quotes command line arguments the wrong way" by Daniel Colascione
    //http://blogs.msdn.com/b/twistylittlepassagesallalike/archive/2011/04/23/everyone-quotes-arguments-the-wrong-way.aspx

    if (!param.isEmpty()&&param.indexOf(QRegularExpression("[\\s\"]"))<0) {
        return param;
    } else {
        QString qparam('"');

        for (QString::const_iterator it=param.constBegin();; it++) {
            int backslash_count=0;

            while (it!=param.constEnd()&&*it=='\\') {
                it++;
                backslash_count++;
            }

            if (it==param.constEnd()) {
                qparam.append(QString(backslash_count*2, '\\'));
                break;
            } else if (*it==L'"') {
                qparam.append(QString(backslash_count*2+1, '\\'));
                qparam.append(*it);
            } else {
                qparam.append(QString(backslash_count, '\\'));
                qparam.append(*it);
            }
        }

        qparam.append('"');

        return qparam;
    }
}

QString ExpandEnvironmentStringsWrapper(QString args)
{
    wchar_t dummy_buf;

    //Documentation says that lpDst parameter is optional but Win 95 version of this function actually fails if lpDst is nullptr
    //So using dummy buffer to get needed buffer length (function returns length in characters including terminating nullptr)
    //If returned length is 0 - it is an error
    if (DWORD buf_len=ExpandEnvironmentStrings(args.toStdWString().c_str(), &dummy_buf, 0)) {
        wchar_t* expanded_buf=new wchar_t[buf_len];

        //Ensuring that returned length is expected length
        if (ExpandEnvironmentStrings(args.toStdWString().c_str(), expanded_buf, buf_len)<=buf_len)
            args.setUtf16(reinterpret_cast<ushort*>(expanded_buf), static_cast<qsizetype>(buf_len) - 1);

        delete[] expanded_buf;
    }
    
    return args;
}

#define IF_NATIVE_SEP(p)	(native_sep?QDir::toNativeSeparators(p):p)

QString ZDLMainWindow::getArgumentsString(bool native_sep)
{
    LOGDATAO() << "Getting arguments" << Qt::endl;
    QString args;
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (!config) {
        return args;
    }
    const ZDLProfile &profile = config->activeProfile();

    QString iwadPath = resolveIwadPath(config, profile);
    if (!iwadPath.isEmpty()) {
        args.append("-iwad ");
        args.append(QuoteParam(IF_NATIVE_SEP(iwadPath)));
    }

    if (profile.monsters > 0) {
        if (profile.monsters == 1) {
            args.append(" -nomonsters");
        } else {
            if (profile.monsters % 2 == 0) {
                args.append(" -fast");
            }
            if (profile.monsters >= 3) {
                args.append(" -respawn");
            }
        }
    }

    if (profile.skill > 0) {
        args.append(" -skill ");
        args.append(QString::number(profile.skill));
    }

    if (!profile.warp.isEmpty()) {
        QStringList warp_args=WarpBackwardCompat(iwadPath, profile.warp);

        if (warp_args.length()) {
            args.append(' ');
            args.append(warp_args.join(" "));
        } else {
            args.append(" +map ");
            args.append(QuoteParam(profile.warp));
        }
    }

    ClassifiedFiles files = classifyFiles(profile.files);

    if (!files.pwads.isEmpty()) {
        args.append(" -file");
        for (const QString& str: files.pwads) {
            args.append(' ');
            args.append(QuoteParam(IF_NATIVE_SEP(str)));
        }
    }

    char deh_last = files.dehLast;
    do {
        if (deh_last%2) {
            for (const QString& str: files.bexs) {
                args.append(" -bex ");
                args.append(QuoteParam(IF_NATIVE_SEP(str)));
            }
        } else {
            for (const QString& str: files.dehs) {
                args.append(" -deh ");
                args.append(QuoteParam(IF_NATIVE_SEP(str)));
            }
        }
        deh_last+=3;
    } while (deh_last<=4);

    for (const QString& str: files.autoexecs) {
        args.append(" +exec ");
        args.append(QuoteParam(IF_NATIVE_SEP(str)));
    }

    for (const QString& str: files.lumps) {
        args.append(" -playdemo ");
        args.append(QuoteParam(IF_NATIVE_SEP(str)));
    }

    const ZDLMultiplayerSettings &mp = profile.multiplayer;
    if (mp.gameType != 0) {
        if (!mp.dmflags.isEmpty()) {
            args.append(" +set dmflags ");
            args.append(mp.dmflags);
        }

        if (!mp.dmflags2.isEmpty()) {
            args.append(" +set dmflags2 ");
            args.append(mp.dmflags2);
        }

        if (mp.gameType == 2) {
            args.append(" -deathmatch");
        } else if (mp.gameType == 3) {
            args.append(" -altdeath");
        }

        if (mp.players > 0) {
            args.append(" -host ");
            args.append(QString::number(mp.players));
            if (!mp.port.isEmpty()) {
                args.append(" -port ");
                args.append(mp.port);
            }
        } else if (mp.players == 0 && !mp.host.isEmpty()) {
            args.append(" -join ");
            if (!mp.port.isEmpty()) {
                QRegularExpression trailing_port(":\\d*\\s*$");
                args.append(QString(mp.host).remove(trailing_port)+":"+mp.port);
            } else {
                args.append(mp.host);
            }
        }

        if (!mp.fragLimit.isEmpty()) {
            args.append(" +set fraglimit ");
            args.append(mp.fragLimit);
        }
        if (!mp.timeLimit.isEmpty()) {
            args.append(" +set timelimit ");
            args.append(mp.timeLimit);
        }
        if (mp.extratic == 1) {
            args.append(" -extratic");
        }
        if (mp.netmode != -1) {
            args.append(" -netmode ");
            args.append(QString::number(mp.netmode));
        }
        if (mp.dup != 0) {
            args.append(" -dup ");
            args.append(QString::number(mp.dup));
        }
        if (!mp.savegame.isEmpty()) {
            args.append(" -loadgame ");
            args.append(QuoteParam(IF_NATIVE_SEP(mp.savegame)));
        }
    }

    if (!config->general.alwaysAdd.isEmpty()) {
        args.append(' ');
        args.append(ExpandEnvironmentStringsWrapper(config->general.alwaysAdd));
    }

    if (!profile.extra.isEmpty()) {
        args.append(' ');
        args.append(ExpandEnvironmentStringsWrapper(profile.extra));
    }

    LOGDATAO() << "args: " << args << Qt::endl;
    return args.trimmed();
}

QStringList ZDLMainWindow::getArgumentsList()
{
    return QStringList();
}

#else

QStringList ParseParams(const QString &params) {
    QStringList plist;

    wordexp_t result;

    switch (wordexp(qPrintable(params), &result, 0)) {
        case 0:
            for (size_t i = 0; i < result.we_wordc; i++) {
                plist << result.we_wordv[i];
            }
            [[fallthrough]];
        case WRDE_NOSPACE:    //If error is WRDE_NOSPACE - there is a possibilty that at least some part of wordexp_t.we_wordv was allocated
            wordfree(&result);
    }

    return plist;
}

QStringList ZDLMainWindow::getArgumentsList() {
    LOGDATAO() << "Getting arguments" << Qt::endl;
    QStringList args;
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (!config) {
        return args;
    }
    const ZDLProfile &profile = config->activeProfile();

    QString iwadPath = resolveIwadPath(config, profile);
    if (!iwadPath.isEmpty()) {
        args << "-iwad" << iwadPath;
    }

    if (profile.monsters > 0) {
        if (profile.monsters == 1) {
            args << "-nomonsters";
        } else {
            if (profile.monsters % 2 == 0) args << "-fast";
            if (profile.monsters >= 3) args << "-respawn";
        }
    }

    if (profile.skill > 0) {
        args << "-skill" << QString::number(profile.skill);
    }

    if (!profile.warp.isEmpty()) {
        QStringList warp_args = WarpBackwardCompat(iwadPath, profile.warp);

        if (warp_args.length()) {
            args << warp_args;
        } else {
            args << "+map" << profile.warp;
        }
    }

    ClassifiedFiles files = classifyFiles(profile.files);

    if (!files.pwads.empty()) {
        args << "-file";
        for (const QString &str: files.pwads) {
            args << str;
        }
    }

    char deh_last = files.dehLast;
    do {
        if (deh_last % 2) {
            for (const QString &str: files.bexs) {
                args << "-bex" << str;
            }
        } else {
            for (const QString &str: files.dehs) {
                args << "-deh" << str;
            }
        }
        deh_last += 3;
    } while (deh_last <= 4);

    for (const QString &str: files.autoexecs) {
        args << "+exec" << str;
    }

    for (const QString &str: files.lumps) {
        args << "-playdemo" << str;
    }

    const ZDLMultiplayerSettings &mp = profile.multiplayer;
    if (mp.gameType != 0) {
        if (!mp.dmflags.isEmpty()) {
            args << "+set" << "dmflags" << mp.dmflags;
        }

        if (!mp.dmflags2.isEmpty()) {
            args << "+set" << "dmflags2" << mp.dmflags2;
        }

        if (mp.gameType == 2) {
            args << "-deathmatch";
        } else if (mp.gameType == 3) {
            args << "-altdeath";
        }

        if (mp.players > 0) {
            args << "-host" << QString::number(mp.players);
            if (!mp.port.isEmpty()) {
                args << "-port" << mp.port;
            }
        } else if (mp.players == 0 && !mp.host.isEmpty()) {
            args << "-join";
            if (!mp.port.isEmpty()) {
                QRegularExpression trailing_port(":\\d*\\s*$");
                args << QString(mp.host).remove(trailing_port) + ":" + mp.port;
            } else {
                args << mp.host;
            }
        }

        if (!mp.fragLimit.isEmpty()) {
            args << "+set" << "fraglimit" << mp.fragLimit;
        }
        if (!mp.timeLimit.isEmpty()) {
            args << "+set" << "timelimit" << mp.timeLimit;
        }
        if (mp.extratic == 1) {
            args << "-extratic";
        }
        if (mp.netmode != -1) {
            args << "-netmode" << QString::number(mp.netmode);
        }
        if (mp.dup != 0) {
            args << "-dup" << QString::number(mp.dup);
        }
        if (!mp.savegame.isEmpty()) {
            args << "-loadgame" << mp.savegame;
        }
    }

    if (!config->general.alwaysAdd.isEmpty()) {
        args << ParseParams(config->general.alwaysAdd);
    }

    if (!profile.extra.isEmpty()) {
        args << ParseParams(profile.extra);
    }

    LOGDATAO() << "args: " << args << Qt::endl;
    return args;
}

QString ZDLMainWindow::getArgumentsString([[maybe_unused]] bool native_sep) {
    QString args;

    for (const QString &str: getArgumentsList()) {
        if (str.indexOf(QRegularExpression("\\s")) != -1) {
            args.append('"');
            args.append(str);
            args.append('"');
        } else {
            args.append(str);
        }
        args.append(' ');
    }

    return args;
}

#endif

QString ZDLMainWindow::getExecutable() {
    LOGDATAO() << "Getting exec" << Qt::endl;
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config) {
        if (const ZDLNameEntry *port = config->findPort(config->activeProfile().port)) {
            LOGDATAO() << "Executable: " << port->file << Qt::endl;
            return port->file;
        }
    }
    LOGDATAO() << "No executable" << Qt::endl;
    return {""};
}

//Pass through functions.
void ZDLMainWindow::startRead() {
    LOGDATAO() << "Starting to read configuration" << Qt::endl;
    intr->startRead();
    settings->startRead();
    QString windowTitle = getWindowTitle();
    setWindowTitle(windowTitle);
}

void ZDLMainWindow::writeConfig() {
    LOGDATAO() << "Writing configuration" << Qt::endl;
    intr->writeConfig();
    settings->writeConfig();
}
