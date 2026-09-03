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

#include <filesystem>
#include <string>
#include <vector>

#include "core/Config.h"

/**
 * The one config the application is working on, and where it came from.
 *
 * Which file that is takes some deciding: one named on the command line wins,
 * then the per user one, then a config sitting beside a portable executable,
 * and a config still in the old INI format is migrated on the way in.  All of
 * that happens once, in start(), and everything afterwards just asks here.
 */
class Session {
public:
    /** Why the config being used is the one being used. */
    enum class Source : std::uint8_t {
        Unknown,
        UserSpecified,
        User,
        Portable,
        Fallback,
    };

    [[nodiscard]] static Session &get();

    /**
     * Reads the command line, settles on a config and loads it.  What comes
     * back is whatever was left over: files to put in the list.
     */
    std::vector<std::string> start(const std::vector<std::string> &arguments);

    [[nodiscard]] Config &config();

    [[nodiscard]] const Config &config() const;

    [[nodiscard]] const std::filesystem::path &path() const;

    [[nodiscard]] Source source() const;

    /**
     * True when a .zdl was named on the command line, which is the one case
     * where ZDL can be asked to launch without ever showing itself.
     */
    [[nodiscard]] bool openedZdlFile() const;

    /** Points the session at another file and reads it. */
    bool load(const std::filesystem::path &path, std::string *error = nullptr);

    /** Writes the config back to wherever it came from. */
    bool save(std::string *error = nullptr) const;

    /** Writes it somewhere else and keeps working on it there. */
    bool saveAs(const std::filesystem::path &path, std::string *error = nullptr);

    /**
     * Copies the config to the per user location and works on it there.  This
     * is the "import current config" action: a portable config becomes the one
     * this machine opens by default.
     */
    bool adoptAsUserConfig(std::string *error = nullptr);

private:
    Session() = default;

    /**
     * Reads jsonPath, falling back to migrating the legacy iniPath beside it
     * when the JSON isn't there yet.  A migrated config is written straight
     * back out so the .json exists from then on; the .ini is left untouched.
     */
    static bool read(const std::filesystem::path &jsonPath,
                     const std::filesystem::path &iniPath,
                     Config &into);

    Config _config;
    std::filesystem::path _path;
    std::filesystem::path _legacy;
    Source _source{Source::Unknown};
    bool _openedZdl{false};
};
