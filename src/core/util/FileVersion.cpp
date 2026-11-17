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
#include <cstdint>
#include <format>
#include <string>

#ifdef _WIN32
#include <array>
#include <cstddef>
#include <cstring>
#include <string_view>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <winver.h>

#include "ttk/system/Text.h"
#endif

#include "core/util/FileVersion.h"

namespace FileVersion {

std::string key(const std::uint16_t language, const std::uint16_t codepage) {
    return std::format(R"(\StringFileInfo\{:04x}{:04x}\FileVersion)", language, codepage);
}

// NOLINTNEXTLINE(modernize-use-string-view): off Windows it only answers empty, on Windows it builds the string.
std::string of(const std::filesystem::path &program) {
#ifdef _WIN32
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(program.c_str(), &handle);

    if (size == 0) {
        return {};
    }

    std::vector<std::byte> block(size);

    if (GetFileVersionInfoW(program.c_str(), 0, size, block.data()) == 0) {
        return {};
    }

    void *found = nullptr;
    UINT length = 0;
    std::array<WORD, 2> translation{};

    if (VerQueryValueW(block.data(), LR"(\VarFileInfo\Translation)", &found, &length) == 0
        || length < sizeof(translation)) {
        return {};
    }

    // The first language and code page pair, copied out since the block promises no alignment.
    std::memcpy(translation.data(), found, sizeof(translation));

    const std::wstring where = ttk::Text::widen(key(translation[0], translation[1]));

    if (VerQueryValueW(block.data(), where.c_str(), &found, &length) == 0 || length == 0) {
        return {};
    }

    // The length counts characters, with the terminator.
    const std::wstring_view value(static_cast<const wchar_t *>(found), length);

    return ttk::Text::trim(ttk::Text::narrow(value.substr(0, value.find(L'\0'))));
#else
    (void)program;

    return {};
#endif
}

}
