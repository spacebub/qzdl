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
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <sys/stat.h>
#endif

#include "ttk/system/Process.h"

#include "support/Shell.h"

// Stand-in ports. Outside Windows they are shell scripts that use only what sh has built in,
// since PATH points at an empty directory. On Windows each is a copy of the stand-in program.
namespace tests::Spawn {

struct Child {
    ttk::Process::Id id{0};
    ttk::Process::Stream output{ttk::Process::NOTHING};
};

// What a stand-in does once started, in this order.
struct Port {
    // The lines 0, 1, 2 and on, below this count.
    int counts{0};

    // Written as it is, with no newline added.
    std::string says{};

    // Until it is killed, rather than leaving with `exit`.
    bool holds{false};

    int exit{0};

    // TERM is a signal outside Windows only, so this is ignored there.
    bool ignoresTerm{false};
};

#ifndef _WIN32

// Opening it for reading blocks until a writer comes, so a script reading it waits for good.
inline std::filesystem::path fifo(const std::filesystem::path &directory) {
    const std::filesystem::path at = directory / "hold";

    std::filesystem::create_directories(directory);
    ::mkfifo(at.c_str(), 0600);

    return at;
}

// Every byte but the plainest as \0ooo, which printf %b writes back as it was.
inline std::string octal(const std::string &bytes) {
    std::string out;

    for (const char each : bytes) {
        const auto byte = static_cast<unsigned char>(each);

        if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9')
            || byte == ' ') {
            out.push_back(each);

            continue;
        }

        out += "\\0";
        out.push_back(static_cast<char>('0' + ((byte >> 6U) & 7U)));
        out.push_back(static_cast<char>('0' + ((byte >> 3U) & 7U)));
        out.push_back(static_cast<char>('0' + (byte & 7U)));
    }

    return out;
}

#endif

// A Windows program has it once CMake marks it with qzdl_test_spawns().
#if !defined(_WIN32) || defined(QZDL_STAND_IN)
inline std::filesystem::path port(const std::filesystem::path &directory, const std::string &name, const Port &what) {
    std::filesystem::create_directories(directory);

#ifdef _WIN32
    const std::filesystem::path at = directory / (name + ".exe");
    std::filesystem::path sidecar = at;

    sidecar += ".port";

    std::filesystem::copy_file(QZDL_STAND_IN, at, std::filesystem::copy_options::overwrite_existing);
    std::ofstream(sidecar, std::ios::binary) << what.counts << ' ' << (what.holds ? 1 : 0) << ' ' << what.exit << '\n'
                                             << what.says;
#else
    const std::filesystem::path at = directory / name;
    std::ofstream script(at);

    script << "#!/bin/sh\n";

    if (what.ignoresTerm) {
        script << "trap '' TERM\n";
    }

    if (what.counts > 0) {
        script << "i=0; while [ $i -lt " << what.counts << " ]; do echo $i; i=$((i+1)); done\n";
    }

    if (!what.says.empty()) {
        script << "printf '%b' '" << octal(what.says) << "'\n";
    }

    if (what.holds) {
        script << "read x < \"" << fifo(directory).string() << "\"\n";
    }

    script << "exit " << what.exit << "\n";
    script.close();

    std::filesystem::permissions(at, std::filesystem::perms::owner_all);
#endif

    return at;
}
#endif

inline Child start(const std::filesystem::path &file, const bool capture) {
    Child child;

    ttk::Process::start(file, {}, {}, {}, &child.id, capture ? &child.output : nullptr);

    return child;
}

inline bool until(Shell &shell, const double step, const std::function<bool()> &done, const double seconds = 10.0) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);

    while (!done()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }

        shell.advance(step);

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    return true;
}

inline void end(const std::vector<ttk::Process::Id> &ids) {
    for (const ttk::Process::Id id : ids) {
        ttk::Process::force(id);

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

        while (ttk::Process::poll(id) == ttk::Process::State::Running && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

}
