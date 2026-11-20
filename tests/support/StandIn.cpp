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
#include <fstream>
#include <iterator>
#include <string>

#include <fcntl.h>
#include <io.h>
#include <windows.h>

// A stand-in port on Windows, where there is no sh to script one with. Spawn::port writes what
// it does into a file beside the copy that is run: the count, whether it holds, the exit code,
// then the bytes it says.
int main() {
    wchar_t self[MAX_PATH * 4];
    const DWORD length = GetModuleFileNameW(nullptr, self, MAX_PATH * 4);
    std::ifstream sidecar(std::wstring(self, length) + L".port", std::ios::binary);

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
