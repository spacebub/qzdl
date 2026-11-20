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
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <fcntl.h>
#include <io.h>
#include <windows.h>

// A stand-in port on Windows, where there is no sh to script one with. What it does is in a file
// beside the copy that is run: .record from launch::recorder, or .port from Spawn::port.
namespace {

std::string env(const char *name) {
    const char *value = std::getenv(name);

    return value != nullptr ? value : "";
}

int record(const std::string &at, const int argc, char **argv) {
    std::ofstream(at + ".cwd") << std::filesystem::current_path().string() << "\n";

    std::ofstream args(at + ".args");

    for (int i = 1; i < argc; ++i) {
        args << argv[i] << "\n";
    }

    std::ofstream(at + ".env") << env("DOOMWADDIR") << "\n" << env("DOOMWADPATH") << "\n" << env("QZDL_PROBE") << "\n";

    return 0;
}

// The count, whether it holds, the exit code, then the bytes it says.
int play(std::ifstream &sidecar) {
    int counts = 0;
    int holds = 0;
    int code = 0;

    if (!(sidecar >> counts >> holds >> code)) {
        return 120;
    }

    sidecar.get();

    const std::string says{std::istreambuf_iterator<char>(sidecar), std::istreambuf_iterator<char>()};

    // Bytes as given: text mode would turn each \n into \r\n.
    _setmode(_fileno(stdout), _O_BINARY);

    for (int at = 0; at < counts; ++at) {
        std::printf("%d\n", at);
    }

    std::fwrite(says.data(), 1, says.size(), stdout);
    std::fflush(stdout);

    if (holds != 0) {
        Sleep(INFINITE);
    }

    return code;
}

}

int main(int argc, char **argv) {
    wchar_t buffer[MAX_PATH * 4];
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH * 4);
    const std::wstring self(buffer, length);

    std::ifstream recorded(self + L".record");
    std::string at;

    if (std::getline(recorded, at)) {
        return record(at, argc, argv);
    }

    std::ifstream sidecar(self + L".port", std::ios::binary);

    return play(sidecar);
}
