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

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/config/Config.h"

namespace Command {

enum class Placeholder : std::uint8_t {
    SourcePort,
    Game,
    Addon,
    Profile,
    CfgDir,
    ExtraCfg,
    SaveDir,
    SaveFile,
    ReplayDir,
};

struct Token {
    Placeholder kind;

    // As written in a command. {addon_n} stands for {addon_1} upwards.
    std::string_view text;

    std::string_view about;
};

// Every placeholder a custom command can hold, in the order they are offered.
[[nodiscard]] std::span<const Token> tokens();

[[nodiscard]] std::string_view text(Placeholder kind);

[[nodiscard]] bool uses(const std::string &command, Placeholder kind);

// A whole {source_port} word stands for every word that starts the port, such as flatpak run and the app.
[[nodiscard]] bool startsPort(const std::string &command);

// A short custom command to show in an empty field.
[[nodiscard]] std::string example();

// The custom command as tokens with placeholders filled in as the generated launch fills them. Empty when it cannot
// run.
[[nodiscard]] std::vector<std::string> custom(const Config &config, std::string *error = nullptr);

// The generated launch with placeholders put back, as a starting point for a custom command.
[[nodiscard]] std::string pattern(const Config &config);

// The whole launch as one shell-quoted line, led by the variables it sets.
[[nodiscard]] std::string line(const Config &config);

struct Shown {
    std::string line;

    // Why the launch cannot run: a typed command that cannot be filled in, or a Flatpak port that is gone, starts no
    // game or finds no flatpak.
    std::string trouble;

    // The -c commands this launch spends of the eleven DOSBox honours, whether the launch was generated or typed by
    // hand. Zero for one that does not run DOSBox.
    int dosCommands{0};
};

// The line, the trouble and the DOSBox commands from one build of the launch.
[[nodiscard]] Shown shown(const Config &config);

}
