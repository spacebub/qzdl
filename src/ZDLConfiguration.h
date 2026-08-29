/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2019  Lcferrum
 * Copyright (C) 2023  spacebub
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

/**
 * ZDLConfiguration
 * Resolves where configuration lives.  Two scopes are used in practice:
 * System level: a machine wide config
 * User level:   the per user config, which is where ZDL saves by default
 *
 * The user level config is zdl.json beside the executable on Windows, where
 * ZDL ships as a portable exe, and $XDG_CONFIG_HOME/qzdl/zdl.json (that is,
 * ~/.config/qzdl) everywhere else.  Each scope also lists legacy INI paths,
 * where pre-JSON versions of ZDL stored things; those are only ever read, and
 * only to migrate an existing zdl.ini into the location above.
 */
class ZDLConfiguration {
public:
    ZDLConfiguration();

    // NUM_CONFS *MUST* be last!
    enum ConfScope {
        CONF_SYSTEM, CONF_USER, CONF_FILE, NUM_CONFS
    };

    /** Path to the zdl.json for the given scope. */
    [[nodiscard]] QString getPath(ConfScope scope) const;

    /** Candidate pre-JSON zdl.ini paths for the given scope, best first. */
    [[nodiscard]] QStringList getLegacyPaths(ConfScope scope) const;

private:
    QString paths[NUM_CONFS];
    QStringList legacyPaths[NUM_CONFS];
};
