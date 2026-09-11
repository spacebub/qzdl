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

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>

#include "core/config/Session.h"
#include "core/launch/Launcher.h"
#include "core/system/Env.h"
#include "core/system/Paths.h"
#include "gui/Http.h"
#include "gui/app/App.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <io.h>
#include <share.h>
#else
#include <unistd.h>
#endif

namespace {

// No stdio: the thread on its way down may be holding the stream's lock.
void writeRaw(const char *text) {
    const size_t length = std::strlen(text);

#ifdef _WIN32
    // NOLINTNEXTLINE(cert-err33-c) -- the process is leaving either way.
    (void) _write(_fileno(stderr), text, static_cast<unsigned int>(length));
#else
    // NOLINTNEXTLINE(cert-err33-c) -- the process is leaving either way.
    (void) ::write(STDERR_FILENO, text, length);
#endif
}

// Windows issues __fastfail for a Rust panic, and the kernel takes that past every
// handler here. Only last-run.log carries those.
extern "C" void onAbort(int) {
    writeRaw("ZDL4 stopped: an error it could not recover from.\n");

    // Returning hands back to abort(), which reports a crash.
    std::_Exit(1);
}

#ifdef _WIN32

// A windowed build has no console. Unbuffered: nothing is flushed on the way down.
void keepMessages() {
    const std::filesystem::path directory = Paths::get().configPath(Paths::USER).parent_path();

    if (directory.empty()) {
        return;
    }

    std::error_code code;

    std::filesystem::create_directories(directory, code);

    if (code) {
        return;
    }

    // A windowed build starts with no stderr for _dup2 to land on.
    FILE *nowhere = nullptr;

    if (_wfreopen_s(&nowhere, L"NUL", L"w", stderr) != 0 || nowhere == nullptr) {
        return;
    }

    // _SH_DENYWR keeps the log readable while ZDL4 is up, and shuts any other ZDL4 out of
    // the name. That one keeps its own beside it.
    FILE *log = _wfsopen((directory / "last-run.log").c_str(), L"w", _SH_DENYWR);

    if (log == nullptr) {
        const std::wstring named =
            L"last-run-" + std::to_wstring(GetCurrentProcessId()) + L".log";

        log = _wfsopen((directory / named).c_str(), L"w", _SH_DENYWR);
    }

    if (log == nullptr) {
        return;
    }

    const int moved = _dup2(_fileno(log), _fileno(stderr));

    // stderr owns the file from here.
    std::fclose(log);

    if (moved != 0) {
        return;
    }

    std::setvbuf(stderr, nullptr, _IONBF, 0);

    // Rust writes to STD_ERROR_HANDLE, which the C stream does not touch.
    if (const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stderr)));
        handle != INVALID_HANDLE_VALUE) {
        SetStdHandle(STD_ERROR_HANDLE, handle);
    }

    // NOLINTNEXTLINE(cert-err33-c,modernize-use-std-print) -- std::println can throw.
    std::fprintf(stderr, "ZDL4 " QZDL_VERSION "\n");
}

#endif

// Windows has no console here; every caller writes to stderr first.
void say([[maybe_unused]] const std::string &message) {
#ifdef _WIN32
    const int wide = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, nullptr, 0);
    std::wstring said(static_cast<size_t>(wide), L'\0');

    MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, said.data(), wide);

    MessageBoxW(nullptr, said.c_str(), L"ZDL4", MB_OK | MB_ICONERROR);
#endif
}

void reportFailure(const std::string &text) {
    // NOLINTNEXTLINE(cert-err33-c,modernize-use-std-print) -- std::println can throw before the box is shown.
    std::fprintf(stderr, "Nothing was launched: %s\n", text.c_str());

    say("Nothing was launched.\n\n" + text);
}

int run(const int argc, char *argv[]) {
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

    // Slint reads this when it first builds a window, so it has to be set before one
    // exists. A value already in the environment wins, and is the way back in when the
    // graphics card cannot be used.
    if (Env::get("SLINT_BACKEND").empty()) {
        Env::set("SLINT_BACKEND",
                 session.config().general.hardwareRendering ? "winit-femtovg" : "winit-software");
    }

    Http::start();

    App().run();

    Http::stop();

    return 0;
}

void reportCrash(const std::string &text) {
    // NOLINTNEXTLINE(cert-err33-c,modernize-use-std-print) -- std::println can throw here of all places.
    std::fprintf(stderr, "ZDL4 stopped: %s\n", text.c_str());

    say("ZDL4 stopped.\n\n" + text);
}

}

int main(int argc, char *argv[]) {
    Paths::setExecutable(argc > 0 ? argv[0] : "");

#ifdef _WIN32
    keepMessages();
#endif

    // After the log is open, so what it has to say lands in it.
    std::signal(SIGABRT, onAbort);

    // What a Slint callback throws unwinds into Rust, which aborts before this frame.
    try {
        return run(argc, argv);
    } catch (const std::exception &trouble) {
        reportCrash(trouble.what());
    } catch (...) {
        reportCrash("something that gave no account of itself");
    }

    return 1;
}
