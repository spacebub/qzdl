/*
 * This file is part of qZDL
 * Copyright (C) 2007-2012  Cody Harris
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

#include "core/MapFile.h"

class Wad : public MapFile {
public:
    explicit Wad(std::filesystem::path file);

    std::string iwadinfoName() override;

    std::string lump(std::string_view name) override;

    std::vector<std::string> lumpNames() override;

    bool isGame() override;

    std::vector<std::string> mapNames() override;

    bool isMapXX() override;

    ~Wad() override = default;

    Wad(const Wad &) = delete;

    Wad &operator=(const Wad &) = delete;

    Wad(Wad &&) = delete;

    Wad &operator=(Wad &&) = delete;

private:
    struct Header {
        char type[4];
        std::int32_t lumps;
        std::int32_t directory;
    };

    struct Lump {
        std::int32_t offset;
        std::int32_t length;
        char name[8];

        // Points into this lump, so it lives exactly as long as the lump does.
        [[nodiscard]] std::string_view nameView() const;
    };

    static std::vector<Lump> readDirectory(std::ifstream &stream);

    std::filesystem::path _file;
};
