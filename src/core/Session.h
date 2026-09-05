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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "core/Config.h"

class Session {
public:
    enum class Source : std::uint8_t {
        Unknown,
        UserSpecified,
        User,
        Portable,
        Fallback,
    };

    [[nodiscard]] static Session &get();

    // Reads the command line, settles on a config and loads it. What comes
    // back is whatever was left over: files to put in the list.
    std::vector<std::string> start(const std::vector<std::string> &arguments);

    [[nodiscard]] Config &config();

    [[nodiscard]] const Config &config() const;

    [[nodiscard]] const std::filesystem::path &path() const;

    [[nodiscard]] Source source() const;

     // True when a .zdl was named on the command line, which is the one case
     // where ZDL can be asked to launch without ever showing itself.
    [[nodiscard]] bool openedZdlFile() const;

    bool load(const std::filesystem::path &path, std::string *error = nullptr);

    bool save(std::string *error = nullptr) const;

    bool saveAs(const std::filesystem::path &path, std::string *error = nullptr);

    /*
    Copies the config to the per user location and works on it there. This
    is the "import current config" action: a portable config becomes the one
    this machine opens by default.
    */
    bool adoptAsUserConfig(std::string *error = nullptr);

    /*
    Whether the per user config has been told to stay out of the way, so that a
    config sitting next to the executable is opened in its place. The flag lives
    in the user config itself, which is why it is read and written here rather
    than off whichever config happens to be open.
    */
    [[nodiscard]] bool userConfigIgnored() const;

    bool setUserConfigIgnored(bool value, std::string *error = nullptr);

private:
    Session() = default;

    // Reads a config, migrating a legacy one only when it is being kept.
    static bool read(const std::filesystem::path &jsonPath,
                     const std::filesystem::path &iniPath,
                     Config &into,
                     bool migrate);

    // The per user config and whatever legacy file it would be migrated from.
    static std::pair<std::filesystem::path, std::filesystem::path> userPaths();

    Config _config;
    std::filesystem::path _path;
    std::filesystem::path _legacy;
    Source _source{Source::Unknown};
    bool _openedZdl{false};
};
