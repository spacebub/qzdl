/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
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

#include <QFile>
#include <QFileInfo>
#include <QMap>

#include "config/ZDLIniImport.h"

namespace {

/** Legacy flags were stored as the strings "1" and "0". */
bool legacyBool(ZDLConf &conf, const char *key, bool def) {
    if (!conf.hasValue("zdl.general", key)) {
        return def;
    }
    int ok = 0;
    return conf.getValue("zdl.general", key, &ok) == "1";
}

QString legacyString(ZDLConf &conf, const char *key) {
    if (!conf.hasValue("zdl.general", key)) {
        return {};
    }
    int ok = 0;
    return conf.getValue("zdl.general", key, &ok);
}

/** Parses the old "x,y" geometry encoding. */
bool parsePair(const QString &value, int *first, int *second) {
    if (!value.contains(",")) {
        return false;
    }
    QStringList parts = value.split(",");
    if (parts.size() < 2) {
        return false;
    }
    bool ok = false;
    int a = parts[0].toInt(&ok);
    if (!ok) {
        return false;
    }
    int b = parts[1].toInt(&ok);
    if (!ok) {
        return false;
    }
    *first = a;
    *second = b;
    return true;
}

/**
 * Reads a numbered name/file list, i.e. the i0n/i0f and p0n/p0f pairs used by
 * [zdl.iwads] and [zdl.ports], into a flat vector ordered by index.
 */
void readNumberedEntries(ZDLSection *section, QChar prefix, QVector<ZDLNameEntry> &out) {
    out.clear();
    if (!section) {
        return;
    }

    QMap<int, ZDLNameEntry> byIndex;
    QVector<ZDLLine *> lines;
    section->getRegex(QString("^%1[0-9]+[nf]$").arg(prefix), lines);

    for (ZDLLine *line: lines) {
        QString variable = line->getVariable();
        bool ok = false;
        int index = variable.mid(1, variable.length() - 2).toInt(&ok);
        if (ok) {
            if (variable.endsWith('n')) {
                byIndex[index].name = line->getValue();
            } else {
                byIndex[index].file = line->getValue();
            }
        }
        // getRegex hands out clones that belong to the caller.
        delete line;
    }

    for (auto it = byIndex.constBegin(); it != byIndex.constEnd(); ++it) {
        if (!it.value().file.isEmpty()) {
            out.append(it.value());
        }
    }
}

/**
 * Reads the file0..fileN keys of a [zdl.save] section, in numeric order.  A "d"
 * suffix on the key marks the entry as disabled.
 */
QVector<ZDLFileEntry> readNumberedFiles(ZDLSection *section) {
    QMap<int, ZDLFileEntry> byIndex;
    QVector<ZDLLine *> lines;
    section->getRegex("^file[0-9]+d?$", lines);

    for (ZDLLine *line: lines) {
        QString variable = line->getVariable();
        bool disabled = variable.endsWith('d', Qt::CaseInsensitive);
        QString digits = variable.mid(4, variable.length() - 4 - (disabled ? 1 : 0));
        bool ok = false;
        int index = digits.toInt(&ok);
        if (ok) {
            byIndex.insert(index, ZDLFileEntry{line->getValue(), !disabled});
        }
        delete line;
    }

    QVector<ZDLFileEntry> files;
    for (auto it = byIndex.constBegin(); it != byIndex.constEnd(); ++it) {
        files.append(it.value());
    }
    return files;
}

QString sectionString(ZDLSection *section, const char *key) {
    return section->hasVariable(key) ? section->findVariable(key) : QString();
}

int sectionInt(ZDLSection *section, const char *key, int def) {
    if (!section->hasVariable(key)) {
        return def;
    }
    bool ok = false;
    int value = section->findVariable(key).toInt(&ok);
    return ok ? value : def;
}

/** Only writes the key when the value carries meaning, matching the old code. */
void setIfSet(ZDLSection *section, const char *key, const QString &value) {
    if (!value.isEmpty()) {
        section->setValue(key, value);
    }
}

}

ZDLProfile ZDLIniImport::profileFromSection(ZDLSection *section) {
    ZDLProfile profile;
    if (!section) {
        return profile;
    }

    profile.iwad = sectionString(section, "iwad");
    profile.port = sectionString(section, "port");
    profile.skill = sectionInt(section, "skill", 0);
    profile.monsters = sectionInt(section, "monsters", 0);
    profile.warp = sectionString(section, "warp");
    profile.extra = sectionString(section, "extra");
    profile.dialogOpen = sectionString(section, "dlgmode").compare("open", Qt::CaseInsensitive) == 0;
    profile.files = readNumberedFiles(section);

    ZDLMultiplayerSettings &mp = profile.multiplayer;
    mp.gameType = sectionInt(section, "gametype", 0);
    mp.players = sectionInt(section, "players", 0);
    mp.extratic = sectionInt(section, "extratic", 0);
    mp.netmode = sectionInt(section, "netmode", -1);
    mp.dup = sectionInt(section, "dup", 0);
    mp.host = sectionString(section, "host");
    mp.port = sectionString(section, "mp_port");
    mp.fragLimit = sectionString(section, "fraglimit");
    mp.timeLimit = sectionString(section, "timelimit");
    mp.dmflags = sectionString(section, "dmflags");
    mp.dmflags2 = sectionString(section, "dmflags2");
    mp.savegame = sectionString(section, "savegame");

    return profile;
}

void ZDLIniImport::profileToSection(const ZDLProfile &profile, ZDLSection *section) {
    if (!section) {
        return;
    }

    setIfSet(section, "port", profile.port);
    setIfSet(section, "iwad", profile.iwad);
    if (profile.skill > 0) {
        section->setValue("skill", QString::number(profile.skill));
    }
    if (profile.monsters > 0) {
        section->setValue("monsters", QString::number(profile.monsters));
    }
    setIfSet(section, "warp", profile.warp);
    setIfSet(section, "extra", profile.extra);
    section->setValue("dlgmode", profile.dialogOpen ? "open" : "closed");

    for (int i = 0; i < profile.files.size(); i++) {
        const ZDLFileEntry &entry = profile.files[i];
        QString key = QString("file%1").arg(i);
        if (!entry.enabled) {
            key.append('d');
        }
        section->setValue(key, entry.file);
    }

    const ZDLMultiplayerSettings &mp = profile.multiplayer;
    setIfSet(section, "host", mp.host);
    setIfSet(section, "mp_port", mp.port);
    setIfSet(section, "fraglimit", mp.fragLimit);
    setIfSet(section, "timelimit", mp.timeLimit);
    setIfSet(section, "dmflags", mp.dmflags);
    setIfSet(section, "dmflags2", mp.dmflags2);
    setIfSet(section, "savegame", mp.savegame);
    section->setValue("gametype", QString::number(mp.gameType));
    section->setValue("players", QString::number(mp.players));
    section->setValue("extratic", QString::number(mp.extratic));
    section->setValue("netmode", QString::number(mp.netmode));
    section->setValue("dup", QString::number(mp.dup));
}

void ZDLIniImport::fromLegacyConf(ZDLConf &conf, ZDLConfigModel &model) {
    model.clear();

    ZDLGeneralSettings &general = model.general;
    general.alwaysAdd = legacyString(conf, "alwaysadd");
    general.autoClose = legacyBool(conf, "autoclose", false);
    general.launchZdlImmediately = legacyBool(conf, "zdllaunch", false);
    general.rememberFileList = legacyBool(conf, "rememberFilelist", true);
    general.showPaths = legacyBool(conf, "showpaths", true);
    general.noUserConf = legacyBool(conf, "nouserconf", false);
    general.isImported = legacyBool(conf, "isimported", false);
    general.doNotImportThis = legacyBool(conf, "donotimportthis", false);
    general.importedFrom = legacyString(conf, "importedfrom");
    general.importDate = legacyString(conf, "importdate");

    general.lastDirs.general = legacyString(conf, "lastDir");
    general.lastDirs.wad = legacyString(conf, "wadLastDir");
    general.lastDirs.src = legacyString(conf, "srcLastDir");
    general.lastDirs.save = legacyString(conf, "saveLastDir");
    general.lastDirs.zdl = legacyString(conf, "zdlLastDir");
    general.lastDirs.config = legacyString(conf, "iniLastDir");

    int first = 0;
    int second = 0;
    if (parsePair(legacyString(conf, "windowsize"), &first, &second)) {
        general.hasWindowSize = true;
        general.windowSize = QSize(first, second);
    }
    if (parsePair(legacyString(conf, "windowpos"), &first, &second)) {
        general.hasWindowPos = true;
        general.windowPos = QPoint(first, second);
    }

    readNumberedEntries(conf.getSection("zdl.iwads"), 'i', model.iwads);
    readNumberedEntries(conf.getSection("zdl.ports"), 'p', model.ports);

    // The single [zdl.save] becomes the one and only profile.
    ZDLSection *save = conf.getSection("zdl.save");
    if (save) {
        ZDLProfile profile = profileFromSection(save);
        profile.id = model.profiles.first().id;
        profile.name = model.profiles.first().name;
        model.profiles[0] = profile;
        model.activeProfileId = profile.id;
    }

    model.ensureProfile();
    model.rememberProfileForIwad();

    LOGDATA() << "Imported legacy config: " << model.iwads.size() << " iwads, "
              << model.ports.size() << " ports" << Qt::endl;
}

bool ZDLIniImport::loadLegacyFile(const QString &path, ZDLConfigModel &model) {
    if (!QFile::exists(path)) {
        return false;
    }
    ZDLConf conf;
    if (conf.readINI(path) != 0) {
        LOGDATA() << "Failed to read legacy config " << path << Qt::endl;
        return false;
    }
    fromLegacyConf(conf, model);
    return true;
}

bool ZDLIniImport::loadZdlFile(const QString &path, ZDLProfile &profile) {
    ZDLConf conf;
    if (conf.readINI(path) != 0) {
        return false;
    }
    ZDLSection *section = conf.getSection("zdl.save");
    if (!section) {
        LOGDATA() << "No zdl.save section in " << path << Qt::endl;
        return false;
    }

    profile = profileFromSection(section);
    profile.id = ZDLProfile::newId();
    profile.name = QFileInfo(path).completeBaseName();
    return true;
}

bool ZDLIniImport::saveZdlFile(const QString &path, const ZDLProfile &profile) {
    ZDLConf conf;
    auto *section = new ZDLSection("zdl.save");
    profileToSection(profile, section);
    // ZDLConf takes ownership of the section.
    conf.addSection(section);
    return conf.writeINI(path) == 0;
}
