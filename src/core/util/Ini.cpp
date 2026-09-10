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

#include <algorithm>
#include <fstream>

#include "core/util/Ini.h"
#include "core/util/Text.h"

bool Ini::Section::has(const std::string &key) const {
    return std::ranges::any_of(values, [&key](const auto &entry) {
        return Text::iequals(entry.first, key);
    });
}

std::string Ini::Section::get(const std::string &key) const {
    for (const auto &entry : values) {
        if (Text::iequals(entry.first, key)) {
            return entry.second;
        }
    }

    return {};
}

void Ini::Section::set(const std::string &key, const std::string &value) {
    for (auto &entry : values) {
        if (Text::iequals(entry.first, key)) {
            entry.second = value;

            return;
        }
    }

    values.emplace_back(key, value);
}

std::vector<const std::pair<std::string, std::string> *>
Ini::Section::startingWith(const std::string_view prefix) const {
    std::vector<const std::pair<std::string, std::string> *> found;

    for (const auto &entry : values) {
        if (entry.first.size() > prefix.size()
            && Text::iequals(std::string_view(entry.first).substr(0, prefix.size()), prefix)) {
            found.push_back(&entry);
        }
    }

    return found;
}

const Ini::Section *Ini::section(const std::string &name) const {
    for (const Section &section : _sections) {
        if (Text::iequals(section.name, name)) {
            return &section;
        }
    }

    return nullptr;
}

Ini::Section &Ini::ensure(const std::string &name) {
    for (Section &section : _sections) {
        if (Text::iequals(section.name, name)) {
            return section;
        }
    }

    return _sections.emplace_back(Section{.name = name, .values = {}});
}

bool Ini::read(const std::filesystem::path &path) {
    std::ifstream file(path);

    if (!file) {
        return false;
    }

    Section *current = nullptr;
    std::string line;

    while (std::getline(file, line)) {
        const std::string trimmed = Text::trim(line);

        if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';') {
            continue;
        }

        if (trimmed.front() == '[') {
            const size_t close = trimmed.find(']');

            if (close != std::string::npos) {
                current = &ensure(Text::trim(trimmed.substr(1, close - 1)));
            }

            continue;
        }

        const size_t equals = trimmed.find('=');

        if (equals == std::string::npos || current == nullptr) {
            continue;
        }

        current->set(Text::trim(trimmed.substr(0, equals)), Text::trim(trimmed.substr(equals + 1)));
    }

    return true;
}

bool Ini::write(const std::filesystem::path &path) const {
    std::error_code code;
    const std::filesystem::path directory = path.parent_path();

    if (!directory.empty() && !std::filesystem::is_directory(directory, code)) {
        std::filesystem::create_directories(directory, code);

        if (code) {
            return false;
        }
    }

    std::ofstream file(path, std::ios::trunc);

    if (!file) {
        return false;
    }

    for (const Section &section : _sections) {
        file << '[' << section.name << "]\n";

        for (const auto &[key, value] : section.values) {
            file << key << '=' << value << '\n';
        }

        file << '\n';
    }

    file.close();

    return static_cast<bool>(file);
}
