/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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
#include <string>
#include <vector>

#include "core/config/Session.h"
#include "core/launch/Launcher.h"
#include "core/system/Paths.h"
#include "gui/Http.h"
#include "gui/app/App.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

// No window and, on Windows, no console: the box is the only way to say so.
void reportFailure(const std::string &text) {
    // NOLINTNEXTLINE(cert-err33-c,modernize-use-std-print) -- std::println can throw before the box is shown.
    std::fprintf(stderr, "Nothing was launched: %s\n", text.c_str());

#ifdef _WIN32
    const std::string message = "Nothing was launched.\n\n" + text;
    const int wide = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, nullptr, 0);
    std::wstring said(static_cast<size_t>(wide), L'\0');

    MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, said.data(), wide);

    MessageBoxW(nullptr, said.c_str(), L"ZDL4", MB_OK | MB_ICONERROR);
#endif
}

}

int main(int argc, char *argv[]) {
    Paths::setExecutable(argc > 0 ? argv[0] : "");

    std::vector<std::string> arguments;

    for (int index = 1; index < argc; index++) {
        arguments.emplace_back(argv[index]);
    }

    Session &session = Session::get();

    session.start(arguments);

    if (session.openedZdlFile() && session.config().general.launchZdlImmediately) {
        std::string error;

        if (Launcher::launch(session.config(), nullptr, nullptr, &error)) {
            return 0;
        }

        reportFailure(error);

        return 1;
    }

    Http::start();

    App().run();

    Http::stop();

    return 0;
}
