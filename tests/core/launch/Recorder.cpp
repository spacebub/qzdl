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
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <windows.h>

// The stand-in port on Windows, where there is no sh to script one with. The path of the
// record it writes is in a file beside the copy that was run.
namespace {

std::string env(const char *name) {
    const char *value = std::getenv(name);

    return value != nullptr ? value : "";
}

std::filesystem::path self() {
    wchar_t buffer[MAX_PATH * 4];
    const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));

    return {std::wstring(buffer, length)};
}

}

int main(int argc, char **argv) {
    const std::filesystem::path me = self();
    std::ifstream sidecar(me.string() + ".record");
    std::string at;

    if (!std::getline(sidecar, at)) {
        return 1;
    }

    std::ofstream(at + ".cwd") << std::filesystem::current_path().string() << "\n";

    std::ofstream args(at + ".args");

    for (int i = 1; i < argc; ++i) {
        args << argv[i] << "\n";
    }

    std::ofstream(at + ".env") << env("DOOMWADDIR") << "\n" << env("DOOMWADPATH") << "\n" << env("QZDL_PROBE") << "\n";

    return 0;
}
