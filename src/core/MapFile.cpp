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

#include <algorithm>
#include <array>
#include <fstream>
#include <regex>

#include "core/LibDir.h"
#include "core/LibPk3.h"
#include "core/MapFile.h"
#include "core/Text.h"
#include "core/Wad.h"

namespace {

// Obvious non-map files, skipped before anything is opened.
constexpr std::array BANNED_EXTENSIONS = {
    ".lmp", ".txt", ".cfg", ".ini", ".deh", ".bex", ".zdl", ".zds", ".dsg", ".esg",
};

bool banned(const std::filesystem::path &file) {
    const std::string extension = Text::lower(file.extension().string());

    return std::ranges::any_of(BANNED_EXTENSIONS, [&extension](const char *candidate) {
        return extension == candidate;
    });
}

}

std::string MapFile::nameFromIwadinfo(const std::string_view text) {
    static const std::regex name(R"re(\s+Name\s*=\s*"(.+)"\s+)re", std::regex::icase);
    std::match_results<std::string_view::const_iterator> match;

    return std::regex_search(text.begin(), text.end(), match, name) ? match[1].str() : std::string();
}

std::unique_ptr<MapFile> MapFile::open(const std::filesystem::path &file) {
    std::error_code code;

    if (std::filesystem::is_directory(file, code)) {
        return std::make_unique<LibDir>(file);
    }

    // Only files with a present, non-blacklisted extension are worth opening.
    if (file.extension().empty() || banned(file) || !std::filesystem::is_regular_file(file, code)) {
        return nullptr;
    }

    std::ifstream stream(file, std::ios::binary);

    if (!stream) {
        return nullptr;
    }

    std::array<char, 4> magic{};

    if (!stream.read(magic.data(), magic.size())) {
        return nullptr;
    }

    if (magic == std::array{'I', 'W', 'A', 'D'} || magic == std::array{'P', 'W', 'A', 'D'}) {
        return std::make_unique<Wad>(file);
    }

    if (magic == std::array{'P', 'K', '\x03', '\x04'}) {
        return std::make_unique<LibPk3>(file);
    }

    return nullptr;
}
