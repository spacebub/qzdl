/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2019  Lcferrum
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

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

/**
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
class Paths {
public:
    // NUM_CONFS *MUST* be last!
    enum Scope : std::uint8_t {
        SYSTEM,
        USER,
        FILE,
        NUM_CONFS,
    };

    /**
     * Where the running executable sits.  Nothing in core can work this out on
     * its own, so whoever owns main() hands it over before anything is asked.
     */
    static void setExecutable(const std::filesystem::path &path);

    [[nodiscard]] static const std::filesystem::path &executable();

    [[nodiscard]] static std::filesystem::path executableDirectory();

    /** The one instance, built the first time it is asked for. */
    [[nodiscard]] static const Paths &get();

    /** Path to the zdl.json for the given scope. */
    [[nodiscard]] std::filesystem::path path(Scope scope) const;

    /** Candidate pre-JSON zdl.ini paths for the given scope, best first. */
    [[nodiscard]] std::vector<std::filesystem::path> legacy(Scope scope) const;

    /** The user's home directory, or empty when the environment hides it. */
    [[nodiscard]] static std::filesystem::path home();

private:
    Paths();

    std::filesystem::path _paths[NUM_CONFS];
    std::vector<std::filesystem::path> _legacy[NUM_CONFS];
};
