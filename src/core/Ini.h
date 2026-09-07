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

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

// Legacy for the old pre-json config format.
class Ini {
public:
    struct Section {
        std::string name;
        std::vector<std::pair<std::string, std::string>> values;

        [[nodiscard]] bool has(const std::string &key) const;

        [[nodiscard]] std::string get(const std::string &key) const;
        void set(const std::string &key, const std::string &value);

        [[nodiscard]] std::vector<const std::pair<std::string, std::string> *> startingWith(std::string_view prefix) const;
    };

    bool read(const std::filesystem::path &path);

    [[nodiscard]] bool write(const std::filesystem::path &path) const;

    [[nodiscard]] const Section *section(const std::string &name) const;

    Section &ensure(const std::string &name);

private:
    std::vector<Section> _sections;
};
