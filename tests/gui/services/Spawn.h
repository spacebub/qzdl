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

#include <sys/stat.h>

#include "ttk/system/Process.h"

#include "support/Shell.h"

// Stand-in ports: shell scripts that use only what sh has built in, since PATH
// points at an empty directory.
namespace tests::Spawn {

struct Child {
    ttk::Process::Id id{0};
    ttk::Process::Stream output{ttk::Process::NOTHING};
};

inline std::filesystem::path
script(const std::filesystem::path &directory, const std::string &name, const std::string &body) {
    const std::filesystem::path at = directory / name;

    std::filesystem::create_directories(directory);
    std::ofstream(at) << "#!/bin/sh\n" << body << "\n";
    std::filesystem::permissions(at, std::filesystem::perms::owner_all);

    return at;
}

// Opening it for reading blocks until a writer comes, so a script reading it waits for good.
inline std::filesystem::path fifo(const std::filesystem::path &directory) {
    const std::filesystem::path at = directory / "hold";

    std::filesystem::create_directories(directory);
    ::mkfifo(at.c_str(), 0600);

    return at;
}

inline Child start(const std::filesystem::path &program, const bool capture) {
    Child child;

    ttk::Process::start(program, {}, {}, {}, &child.id, capture ? &child.output : nullptr);

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
