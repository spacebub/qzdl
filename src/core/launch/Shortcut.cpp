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
#include <string>
#include <system_error>
#include <utility>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include "ttk/system/Text.h"

#include "core/config/Session.h"
#include "core/launch/Shortcut.h"

using namespace ttk;
using Microsoft::WRL::ComPtr;

namespace {

std::filesystem::path desktop() {
    PWSTR raw = nullptr;
    std::filesystem::path folder;

    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, KF_FLAG_DEFAULT, nullptr, &raw)) && raw != nullptr) {
        folder = raw;
    }

    CoTaskMemFree(raw);

    return folder;
}

// argv[0] may be relative to a folder the shortcut is never started from.
std::filesystem::path program() {
    std::wstring path(MAX_PATH, L'\0');

    for (;;) {
        const DWORD got = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));

        if (got == 0) {
            return {};
        }

        if (got < path.size()) {
            path.resize(got);

            return path;
        }

        path.resize(path.size() * 2);
    }
}

bool launches(const std::filesystem::path &path, const std::wstring &arguments) {
    ComPtr<IShellLinkW> link;
    ComPtr<IPersistFile> file;
    std::wstring held(32768, L'\0');

    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))
        || FAILED(link.As(&file)) || FAILED(file->Load(path.c_str(), STGM_READ))
        || FAILED(link->GetArguments(held.data(), static_cast<int>(held.size())))) {
        return false;
    }

    held.resize(std::wcslen(held.c_str()));

    return CompareStringOrdinal(held.c_str(), -1, arguments.c_str(), -1, TRUE) == CSTR_EQUAL;
}

// An icon renamed since, by the user or with its profile, is still this one.
std::filesystem::path place(const std::filesystem::path &folder, const std::string &stem, const std::wstring &arguments) {
    std::error_code code;

    for (std::filesystem::directory_iterator it(folder, code), end; !code && it != end; it.increment(code)) {
        if (CompareStringOrdinal(it->path().extension().c_str(), -1, L".lnk", -1, TRUE) == CSTR_EQUAL
            && launches(it->path(), arguments)) {
            return it->path();
        }
    }

    for (int suffix = 1; suffix < 100; suffix++) {
        const std::string named = suffix == 1 ? stem : stem + " (" + std::to_string(suffix) + ")";
        const std::filesystem::path path = folder / Text::widen(named + ".lnk");

        if (!std::filesystem::exists(path, code) && !code) {
            return path;
        }
    }

    return {};
}

std::wstring argumentsOf(const std::filesystem::path &zdl, const std::filesystem::path &config) {
    std::string line = std::string(Session::LAUNCH) + " " + Text::quote_argument(Text::narrow(zdl.wstring()));

    // After the .zdl, since a start reads nothing past its config.
    if (!config.empty()) {
        line += " " + Text::quote_argument(Text::narrow(config.wstring()));
    }

    return Text::widen(line);
}

std::filesystem::path write(const std::string &stem,
                            const std::filesystem::path &zdl,
                            const std::filesystem::path &config,
                            std::string &why) {
    const std::filesystem::path folder = desktop();
    const std::filesystem::path exe = program();

    if (folder.empty()) {
        why = "Windows did not say where the desktop is";

        return {};
    }

    if (exe.empty()) {
        why = "Windows did not say where ZDL4 is";

        return {};
    }

    const std::wstring arguments = argumentsOf(zdl, config);
    const std::wstring description = Text::widen("Launch " + stem + " with ZDL4");
    std::filesystem::path path = place(folder, stem, arguments);

    if (path.empty()) {
        why = "the desktop already holds too many shortcuts named " + stem;

        return {};
    }

    ComPtr<IShellLinkW> link;
    ComPtr<IPersistFile> file;

    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))
        || FAILED(link->SetPath(exe.c_str())) || FAILED(link->SetArguments(arguments.c_str()))
        || FAILED(link->SetWorkingDirectory(exe.parent_path().c_str()))
        || FAILED(link->SetIconLocation(exe.c_str(), 0)) || FAILED(link->SetDescription(description.c_str()))
        || FAILED(link.As(&file)) || FAILED(file->Save(path.c_str(), TRUE))) {
        why = "Windows would not write " + Text::narrow(path.wstring());

        return {};
    }

    return path;
}

}

namespace Shortcut {

std::filesystem::path toDesktop(const std::string &stem,
                                const std::filesystem::path &zdl,
                                const std::filesystem::path &config,
                                std::string *error) {
    // Changed mode means COM is already up on this thread in the other apartment, which still serves.
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (FAILED(com) && com != RPC_E_CHANGED_MODE) {
        if (error != nullptr) {
            *error = "Windows would not start COM";
        }

        return {};
    }

    std::string why;
    std::filesystem::path made = write(stem, zdl, config, why);

    if (SUCCEEDED(com)) {
        CoUninitialize();
    }

    if (made.empty() && error != nullptr) {
        *error = std::move(why);
    }

    return made;
}

}
