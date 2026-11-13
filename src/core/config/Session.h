/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
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

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "core/config/Config.h"

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

    // Returns the leftover arguments: files to add to the list.
    std::vector<std::string> start(const std::vector<std::string> &arguments);

    [[nodiscard]] Config &config();

    [[nodiscard]] const Config &config() const;

    [[nodiscard]] const std::filesystem::path &path() const;

    [[nodiscard]] Source source() const;

    [[nodiscard]] bool openedZdlFile() const;

    // Set when the config would not read at start. Empty otherwise.
    [[nodiscard]] const std::string &warning() const;

    bool load(const std::filesystem::path &path, std::string *error = nullptr);

    bool save(std::string *error = nullptr) const;

    bool saveAs(const std::filesystem::path &path, std::string *error = nullptr);

    // Copies the open config to the per-user location and switches to it.
    bool adoptAsUserConfig(std::string *error = nullptr);

    // Read from the user config itself, not whichever config is open.
    [[nodiscard]] bool userConfigIgnored() const;

    bool setUserConfigIgnored(bool value, std::string *error = nullptr);

private:
    Session() = default;

    static bool read(const std::filesystem::path &jsonPath,
                     const std::filesystem::path &iniPath,
                     Config &into,
                     bool migrate,
                     std::string *error = nullptr);

    // {json, legacy ini}
    static std::pair<std::filesystem::path, std::filesystem::path> userPaths();

    // Moves an unreadable config out of the way of the next save, or holds saves off it.
    void setAside(const std::string &why);

    Config _config;
    std::filesystem::path _path;
    std::filesystem::path _legacy;
    Source _source{Source::Unknown};
    bool _openedZdl{false};

    // The config could not be read or moved, so saving would destroy it.
    bool _held{false};

    std::string _warning;
};
