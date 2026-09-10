/*
 * This file is part of qZDL
 * Copyright (C) 2019  Lcferrum
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

#include "core/wad/MapFile.h"

class LibDir : public MapFile {
public:
    explicit LibDir(std::filesystem::path file);

    std::string iwadinfoName() override;

    std::string lump(std::string_view name) override;

    std::string picture(std::span<const std::string_view> names) override;

    std::vector<std::string> lumpNames() override;

    bool isGame() override;

    std::vector<std::string> mapNames() override;

    bool isMapXX() override;

    ~LibDir() override = default;

    LibDir(const LibDir &) = delete;

    LibDir &operator=(const LibDir &) = delete;

    LibDir(LibDir &&) = delete;

    LibDir &operator=(LibDir &&) = delete;

private:
    [[nodiscard]] std::filesystem::path mapsDirectory() const;

    // under is the directory's name, which drawable() checks.
    static void bestIn(const std::filesystem::path &directory, std::string_view under,
                       std::span<const std::string_view> names,
                       std::filesystem::path &best, size_t &rank);

    std::filesystem::path _file;
};
