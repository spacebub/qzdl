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
#include <vector>

#include "core/config/Config.h"

namespace DosFiles {

// DOSBox mangles names longer than 8.3, so the port would not find them.
[[nodiscard]] bool spellable(const std::string &name);

struct Copy {
    std::filesystem::path from;
    std::filesystem::path to;
};

class Staging {
public:
    explicit Staging(std::filesystem::path directory);

    [[nodiscard]] std::filesystem::path spellableName(const std::filesystem::path &file);

    // The IWAD under the name the port expects, plus the port's own wads beside it.
    [[nodiscard]] std::filesystem::path game(const std::filesystem::path &iwad,
                                             const std::filesystem::path &portDirectory);

    [[nodiscard]] const std::filesystem::path &directory() const;

    [[nodiscard]] const std::vector<Copy> &planned() const;

private:
    std::filesystem::path keep(const std::filesystem::path &file, const std::string &name);

    std::string shorten(const std::filesystem::path &file);

    std::filesystem::path _directory;
    std::vector<Copy> _planned;
};

[[nodiscard]] std::filesystem::path directory(const Config &config,
                                              const std::filesystem::path &port);

}
