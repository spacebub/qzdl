/*
* This file is part of qZDL
 * Copyright (C) 2026  spacebub
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

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>

#include "external/chocobo1/Md5.h"

inline std::string md5File(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        throw std::runtime_error("Failed to open file");
    }

    Chocobo1::MD5 md5;

    std::array<std::byte, static_cast<std::size_t>(64 * 1024)> buffer{};

    while (file.read(
        reinterpret_cast<char*>(buffer.data()),
        buffer.size()) || file.gcount() > 0)
    {
        const auto bytes_read = static_cast<std::size_t>(file.gcount());

        md5.addData(std::span{
            buffer.data(),
            bytes_read
        });
    }

    return md5.finalize().toString();
}
