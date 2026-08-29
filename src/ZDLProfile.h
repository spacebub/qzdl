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

#include <QString>
#include <QStringList>
#include <QVector>

#include "ZDLJson.h"

/**
 * One entry of the external file list.  Entries can be disabled, which keeps
 * them in the list (struck through in the UI) but off the command line; the INI
 * format encoded that as a "d" suffix on the fileN key.
 */
struct ZDLFileEntry {
    QString file;
    bool enabled{true};
};

/**
 * Multiplayer half of a launch profile.  The text fields are kept as strings
 * rather than numbers because an empty string is what "not set" means here, and
 * the source port command line distinguishes an unset limit from a zero one.
 */
struct ZDLMultiplayerSettings {
    int gameType{0};
    int players{0};
    int extratic{0};
    int netmode{-1};
    int dup{0};
    QString host;
    QString port;
    QString fragLimit;
    QString timeLimit;
    QString dmflags;
    QString dmflags2;
    QString savegame;
};

/**
 * A single named launch configuration.  This replaces the old [zdl.save]
 * section: where there used to be exactly one, a config now holds a list.
 *
 * iwad and port refer to entries in the config's IWAD and source port lists
 * *by name*, which is both how [zdl.save] always worked and what .zdl files
 * exchanged with other Doom tools expect.  An empty iwad means the profile is
 * not bound to any particular game.
 */
struct ZDLProfile {
    QString id;
    QString name;
    QString iwad;
    QString port;
    QVector<ZDLFileEntry> files;
    /* 0 means "not set" for both of these, matching the combo box index where
     * entry 0 is the empty default. */
    int skill{0};
    int monsters{0};
    QString warp;
    QString extra;
    bool dialogOpen{false};
    ZDLMultiplayerSettings multiplayer;

    /** Generates a fresh unique profile id. */
    static QString newId();

    static ZDLProfile fromJson(yyjson_val *obj);

    [[nodiscard]] yyjson_mut_val *toJson(ZDLJson::Builder &builder) const;

    /** Everything except id and name, used when switching or clearing. */
    void clearSettings();
};
