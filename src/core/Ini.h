/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
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

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

/**
 * The INI format ZDL used before JSON.  Two things still speak it: a zdl.ini
 * left behind by an older version, which is read once and migrated, and .zdl
 * launch configs, which stay in this format because other Doom tools exchange
 * them.
 *
 * Sections and the keys inside them keep the order they were written in, which
 * is what makes a numbered list like file0, file1, file2 come back as a list.
 */
class Ini {
public:
    struct Section {
        std::string name;
        std::vector<std::pair<std::string, std::string>> values;

        [[nodiscard]] bool has(const std::string &key) const;

        /** The value for a key, or an empty string when it is not there. */
        [[nodiscard]] std::string get(const std::string &key) const;

        /** Replaces the value if the key is already here, appends it if not. */
        void set(const std::string &key, const std::string &value);

        /**
         * Every key that starts with prefix, in the order they were written.
         * This is how the numbered lists (i0n, file3d) are read back.
         */
        [[nodiscard]] std::vector<const std::pair<std::string, std::string> *>
        startingWith(std::string_view prefix) const;
    };

    bool read(const std::filesystem::path &path);

    bool write(const std::filesystem::path &path) const;

    [[nodiscard]] const Section *section(const std::string &name) const;

    /** The named section, created empty at the end if it is not already here. */
    Section &ensure(const std::string &name);

private:
    std::vector<Section> _sections;
};
