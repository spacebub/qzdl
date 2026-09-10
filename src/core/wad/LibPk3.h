/*
 * This file is part of qZDL
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

#include <memory>

#include "core/wad/MapFile.h"

class LibPk3 : public MapFile {
public:
    struct Zip;

    explicit LibPk3(std::filesystem::path file);

    ~LibPk3() override;

    std::string iwadinfoName() override;

    std::string lump(std::string_view name) override;

    std::string picture(std::span<const std::string_view> names) override;

    std::vector<std::string> lumpNames() override;

    bool isGame() override;

    std::vector<std::string> mapNames() override;

    bool isMapXX() override;

    LibPk3(const LibPk3 &) = delete;

    LibPk3 &operator=(const LibPk3 &) = delete;

    LibPk3(LibPk3 &&) = delete;

    LibPk3 &operator=(LibPk3 &&) = delete;

private:
    // Opened once; reading the central directory is expensive.
    Zip *zip();

    std::filesystem::path _file;
    std::unique_ptr<Zip> _zip;
};
