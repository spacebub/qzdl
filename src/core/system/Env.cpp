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
#include <string>

#include "core/system/Env.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

std::string Env::get(const char *name) {
#ifdef _MSC_VER
    char *value = nullptr;
    size_t length = 0;

    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) {
        return {};
    }

    std::string held(value);

    std::free(value);

    return held;
#else
    // NOLINTNEXTLINE(concurrency-mt-unsafe) -- set() runs before any thread starts.
    const char *value = std::getenv(name);

    return value != nullptr ? std::string(value) : std::string();
#endif
}

void Env::set(const char *name, const char *value) {
#ifdef _WIN32
    const int wide = MultiByteToWideChar(CP_UTF8, 0, value, -1, nullptr, 0);
    std::wstring held(static_cast<size_t>(wide), L'\0');

    MultiByteToWideChar(CP_UTF8, 0, value, -1, held.data(), wide);

    const int narrow = MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
    std::wstring called(static_cast<size_t>(narrow), L'\0');

    MultiByteToWideChar(CP_UTF8, 0, name, -1, called.data(), narrow);

    SetEnvironmentVariableW(called.c_str(), held.c_str());

    // Windows keeps the C library's environment separate, and get() reads that one.
    // NOLINTNEXTLINE(concurrency-mt-unsafe) -- runs before any thread starts.
    _wputenv_s(called.c_str(), held.c_str());
#else
    // NOLINTNEXTLINE(concurrency-mt-unsafe) -- runs before any thread starts.
    setenv(name, value, 1);
#endif
}
