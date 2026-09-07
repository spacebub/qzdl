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

#include "core/Launcher.h"
#include "core/Paths.h"
#include "core/Session.h"
#include "gui/App.h"
#include "gui/Http.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

// This run opens no window, and the Windows GUI subsystem leaves no console
// behind either, so without the box the failure is a silent exit.
void reportFailure(const std::string &text) {
    // std::println can throw, and an exception out of main would terminate the
    // process before the box below.
    // NOLINTNEXTLINE(cert-err33-c,modernize-use-std-print)
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

    // A .zdl on the command line with the setting on is the one path that never
    // shows a window: the file says what to launch, and that is the whole run.
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
