/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
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

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "ttk/system/Paths.h"
#include "ttk/system/Process.h"

#include "core/config/Config.h"
#include "core/config/Schema.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace launch {

namespace fs = std::filesystem;

using Strings = std::vector<std::string>;

inline void write(const fs::path &path, const std::string_view bytes) {
    fs::create_directories(path.parent_path());

    std::ofstream out(path, std::ios::binary | std::ios::trunc);

    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

inline std::string slurp(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);

    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

inline Strings lines(const fs::path &path) {
    std::ifstream in(path);
    Strings out;

    for (std::string line; std::getline(in, line);) {
        out.push_back(line);
    }

    return out;
}

inline fs::path wad(const fs::path &path, const char *kind, const Strings &maps) {
    const auto put32 = [](std::string &out, const std::uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            out.push_back(static_cast<char>((value >> shift) & 0xFFU));
        }
    };

    std::string directory;

    for (const std::string &map : maps) {
        for (const std::string &name : {map, std::string("THINGS")}) {
            put32(directory, 12);
            put32(directory, 0);

            directory += name;
            directory.append(8 - name.size(), '\0');
        }
    }

    std::string bytes(kind);

    put32(bytes, static_cast<std::uint32_t>(maps.size() * 2));
    put32(bytes, 12);

    write(path, bytes + directory);

    return path;
}

inline fs::path profiles() {
    return ttk::Paths::data_directory() / ConfigFile::PROFILES_DIR;
}

inline Config single(const fs::path &port, const bool dosbox = false) {
    Config made;

    made.iwads.push_back({.name = "Game", .file = tests::Corpus::iwad().string()});
    made.ports.push_back({.name = "Port", .file = port.string(), .dosbox = dosbox});

    Profile profile;

    profile.id = "only";
    profile.name = "Only";
    profile.iwad = "Game";
    profile.port = "Port";

    made.profiles.push_back(std::move(profile));
    made.activeProfileId = "only";

    return made;
}

// A port that writes what it was run with beside record: .args one per line, .cwd, and .env.
inline fs::path recorder(const fs::path &path, const fs::path &record) {
    const std::string at = record.string();

    write(path,
          "#!/bin/sh\n"
          "pwd -P > '"
                  + at
                  + ".cwd'\n"
                    "for each in \"$@\"; do printf '%s\\n' \"$each\"; done > '"
                  + at
                  + ".args'\n"
                    "printf '%s\\n' \"$DOOMWADDIR\" \"$DOOMWADPATH\" > '"
                  + at + ".env'\n");

    fs::permissions(path, fs::perms::owner_all);

    return path;
}

inline ttk::Process::State finish(const ttk::Process::Id id) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    ttk::Process::State state = ttk::Process::State::Running;

    while (state == ttk::Process::State::Running && std::chrono::steady_clock::now() < deadline) {
        state = ttk::Process::poll(id);

        std::this_thread::yield();
    }

    return state;
}

}
