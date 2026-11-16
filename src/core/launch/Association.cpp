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

#include <cwchar>
#include <filesystem>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <shlobj.h>

#include "ttk/system/Text.h"

#include "core/config/Session.h"
#include "core/launch/Association.h"
#include "core/launch/Shortcut.h"

using namespace ttk;

namespace {

// IDI_SAVE in src/resources/Win32/resource.h, which is UTF-16, and the installer's -109.
constexpr int ZDL_ICON = 109;

constexpr const wchar_t *PROG_ID = L"ZDL4.zdl";
constexpr const wchar_t *EXTENSION = L"Software\\Classes\\.zdl";
constexpr const wchar_t *OPEN_WITH = L"Software\\Classes\\.zdl\\OpenWithProgids";
constexpr const wchar_t *CLASS = L"Software\\Classes\\ZDL4.zdl";
constexpr const wchar_t *COMMAND = L"Software\\Classes\\ZDL4.zdl\\shell\\open\\command";

// What Open with > Always picked. It outranks Classes, and only Windows may write it.
constexpr const wchar_t *CHOSEN = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.zdl\\UserChoice";

// The form the installer writes, so either one reads as held.
std::wstring commandOf(const std::filesystem::path &exe) {
    return L"\"" + exe.wstring() + L"\" " + Text::widen(Session::LAUNCH) + L" \"%1\"";
}

std::wstring read(const wchar_t *key, const wchar_t *name = nullptr) {
    DWORD size = 0;

    if (RegGetValueW(HKEY_CURRENT_USER, key, name, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS
        || size == 0) {
        return {};
    }

    std::wstring value(size / sizeof(wchar_t), L'\0');

    if (RegGetValueW(HKEY_CURRENT_USER, key, name, RRF_RT_REG_SZ, nullptr, value.data(), &size) != ERROR_SUCCESS) {
        return {};
    }

    value.resize(std::wcslen(value.c_str()));

    return value;
}

bool write(const wchar_t *key, const wchar_t *name, const std::wstring &value) {
    const auto bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));

    return RegSetKeyValueW(HKEY_CURRENT_USER, key, name, REG_SZ, value.c_str(), bytes) == ERROR_SUCCESS;
}

bool same(const std::wstring &one, const wchar_t *other) {
    return CompareStringOrdinal(one.c_str(), -1, other, -1, TRUE) == CSTR_EQUAL;
}

void dropIfEmpty(const wchar_t *key) {
    HKEY opened = nullptr;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_READ, &opened) != ERROR_SUCCESS) {
        return;
    }

    DWORD keys = 0;
    DWORD values = 0;
    const bool empty = RegQueryInfoKeyW(opened,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        &keys,
                                        nullptr,
                                        nullptr,
                                        &values,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        nullptr)
                               == ERROR_SUCCESS
                       && keys == 0 && values == 0;

    RegCloseKey(opened);

    if (empty) {
        RegDeleteKeyW(HKEY_CURRENT_USER, key);
    }
}

bool remove() {
    const LSTATUS tree = RegDeleteTreeW(HKEY_CURRENT_USER, CLASS);

    RegDeleteKeyValueW(HKEY_CURRENT_USER, OPEN_WITH, PROG_ID);

    // The extension may since have been given to another program.
    if (same(read(EXTENSION), PROG_ID)) {
        RegDeleteKeyValueW(HKEY_CURRENT_USER, EXTENSION, nullptr);
    }

    dropIfEmpty(OPEN_WITH);
    dropIfEmpty(EXTENSION);

    return tree == ERROR_SUCCESS || tree == ERROR_FILE_NOT_FOUND;
}

bool chosenElsewhere() {
    const std::wstring chosen = read(CHOSEN, L"ProgId");

    return !chosen.empty() && !same(chosen, PROG_ID);
}

}

namespace Association {

bool held() {
    const std::filesystem::path exe = Shortcut::program();

    return !exe.empty() && !chosenElsewhere() && same(read(EXTENSION), PROG_ID)
           && same(read(COMMAND), commandOf(exe).c_str());
}

bool set(const bool on, std::string *error) {
    const std::filesystem::path exe = Shortcut::program();
    std::string why;

    if (!on) {
        if (!remove()) {
            why = "Windows would not remove ZDL4 as the program for .zdl files";
        }
    } else if (exe.empty()
               || !(write(EXTENSION, nullptr, PROG_ID) && write(OPEN_WITH, PROG_ID, L"")
                    && write(CLASS, nullptr, L"ZDL4 launch config")
                    && write(std::wstring(CLASS).append(L"\\DefaultIcon").c_str(),
                             nullptr,
                             L"\"" + exe.wstring() + L"\",-" + std::to_wstring(ZDL_ICON))
                    && write(COMMAND, nullptr, commandOf(exe)))) {
        remove();

        why = "Windows would not record ZDL4 as the program for .zdl files";
    } else if (chosenElsewhere()) {
        why = "Windows opens .zdl files with another program picked in Open with. Pick ZDL4 there to change it";
    }

    // Explorer caches icons and handlers until told.
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    if (!why.empty() && error != nullptr) {
        *error = why;
    }

    return why.empty();
}

}
