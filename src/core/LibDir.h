/*
 * This file is part of qZDL
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

#include "core/MapFile.h"

class LibDir : public MapFile {
public:
    explicit LibDir(std::filesystem::path file);

    std::string iwadinfoName() override;

    std::string lump(std::string_view name) override;

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

    std::filesystem::path _file;
};
