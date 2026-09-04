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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

class Paths {
public:
    // NUM_CONFS *MUST* be last!
    enum Scope : std::uint8_t {
        SYSTEM,
        USER,
        FILE,
        NUM_CONFS,
    };

    static void setExecutable(const std::filesystem::path &path);

    [[nodiscard]] static const std::filesystem::path &executable();

    [[nodiscard]] static std::filesystem::path executableDirectory();

    [[nodiscard]] static const Paths &get();

    [[nodiscard]] std::filesystem::path configPath(Scope scope) const;

    [[nodiscard]] std::vector<std::filesystem::path> legacyConfigPath(Scope scope) const;

    [[nodiscard]] static std::filesystem::path homeDirectory();

    [[nodiscard]] static std::filesystem::path dataDirectory();

private:
    Paths();

    std::filesystem::path _paths[NUM_CONFS];
    std::vector<std::filesystem::path> _legacy[NUM_CONFS];
};
