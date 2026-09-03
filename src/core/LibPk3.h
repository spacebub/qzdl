/*
 * This file is part of qZDL
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

#include "core/MapFile.h"

/** A PK3, PK7 or plain zip: a PK3 is a zip with Doom data laid out inside it. */
class LibPk3 : public MapFile {
public:
    explicit LibPk3(std::filesystem::path file);

    std::string iwadinfoName() override;

    std::vector<std::string> mapNames() override;

    bool isMapXX() override;

    ~LibPk3() override = default;

    LibPk3(const LibPk3 &) = delete;

    LibPk3 &operator=(const LibPk3 &) = delete;

    LibPk3(LibPk3 &&) = delete;

    LibPk3 &operator=(LibPk3 &&) = delete;

private:
    std::filesystem::path _file;
};
