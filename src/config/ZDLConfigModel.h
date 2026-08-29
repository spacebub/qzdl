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
#pragma once

#include <QMap>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QVector>

#include "config/ZDLProfile.h"

/** A named entry in the IWAD or source port list. */
struct ZDLNameEntry {
    QString name;
    QString file;
};

/** Remembered file dialog directories. */
struct ZDLLastDirs {
    QString general;
    QString wad;
    QString src;
    QString save;
    QString zdl;
    QString config;
};

/** Application wide settings, the old [zdl.general] section. */
struct ZDLGeneralSettings {
    QString alwaysAdd;
    bool autoClose{false};
    bool launchZdlImmediately{false};
    bool rememberFileList{true};
    bool showPaths{true};
    bool noUserConf{false};

    /* Bookkeeping for the "import this config to the user directory" flow. */
    bool isImported{false};
    bool doNotImportThis{false};
    QString importedFrom;
    QString importDate;

    bool hasWindowSize{false};
    QSize windowSize;
    bool hasWindowPos{false};
    QPoint windowPos;

    ZDLLastDirs lastDirs;

    /** IWAD name -> profile id, so a game reopens the profile last used with it. */
    QMap<QString, QString> lastProfileByIwad;
};

/**
 * The whole of zdl.json in memory.  Replaces ZDLConf as the object the widgets
 * read from and write to: rebuild() pushes widget state into here, newConfig()
 * pulls it back out.
 *
 * The model always holds at least one profile and activeProfileId always names
 * one of them, so activeProfile() is safe to call unconditionally.
 */
class ZDLConfigModel {
public:
    ZDLConfigModel();

    static constexpr int SCHEMA_VERSION = 1;

    bool load(const QString &path, QString *error = nullptr);

    bool save(const QString &path, QString *error = nullptr) const;

    /** Resets to a single empty profile, dropping everything else. */
    void clear();

    ZDLGeneralSettings general;
    QVector<ZDLNameEntry> iwads;
    QVector<ZDLNameEntry> ports;
    QVector<ZDLProfile> profiles;
    QString activeProfileId;

    /** The profile the launch tab is currently editing. */
    ZDLProfile &activeProfile();

    [[nodiscard]] int activeProfileIndex() const;

    [[nodiscard]] int indexOfProfile(const QString &id) const;

    /** Switches profiles.  Returns false if id names no profile. */
    bool setActiveProfile(const QString &id);

    /** Appends a new empty profile and returns its id. */
    QString addProfile(const QString &name);

    /** Copies the active profile under a new name and returns the copy's id. */
    QString duplicateActiveProfile(const QString &name);

    /** Removes a profile; the last remaining one is emptied rather than removed. */
    void removeProfile(const QString &id);

    /** Restores the "at least one profile, valid active id" invariant. */
    void ensureProfile();

    /** Appends " (2)", " (3)"... until the name is free. */
    [[nodiscard]] QString uniqueProfileName(const QString &base) const;

    /** Profile id to switch to when iwadName is selected, or empty for none. */
    [[nodiscard]] QString profileForIwad(const QString &iwadName) const;

    /** Records the active profile as the one last used with its bound IWAD. */
    void rememberProfileForIwad();

    [[nodiscard]] const ZDLNameEntry *findIwad(const QString &name) const;

    [[nodiscard]] const ZDLNameEntry *findPort(const QString &name) const;
};
