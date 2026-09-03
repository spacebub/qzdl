/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "core/Process.h"
#include "core/Text.h"

#ifdef _WIN32

#include <windows.h>

#else

#include <cerrno>
#include <csignal>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

#endif

namespace Process {

#ifdef _WIN32

bool startDetached(const std::filesystem::path &program,
                   const std::vector<std::string> &arguments,
                   const std::filesystem::path &workingDirectory,
                   std::string *error) {
    /*
    Windows hands the child one string and lets it do its own splitting, so the
    quoting has to be right here. The program itself is quoted whole, since a
    path with a space in it is the normal case rather than the odd one.
    */
    std::wstring command = L"\"" + program.wstring() + L"\"";

    for (const std::string &argument : arguments) {
        const std::string quoted = Text::quoteArgument(argument);

        command.push_back(L' ');
        command.append(std::filesystem::path(quoted).wstring());
    }

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_SHOWNORMAL;

    PROCESS_INFORMATION information = {};
    const std::wstring directory = workingDirectory.wstring();

    // The command line is written into by CreateProcessW, so it cannot be const.
    std::vector<wchar_t> writable(command.begin(), command.end());
    writable.push_back(L'\0');

    const BOOL started = CreateProcessW(
        nullptr, writable.data(), nullptr, nullptr, FALSE,
        NORMAL_PRIORITY_CLASS | CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE,
        nullptr, directory.empty() ? nullptr : directory.c_str(), &startup, &information);

    if (started == 0) {
        if (error != nullptr) {
            *error = "the system refused to start it (error "
                + std::to_string(GetLastError()) + ")";
        }

        return false;
    }

    // Nothing here waits on it, so both handles go straight back.
    CloseHandle(information.hProcess);
    CloseHandle(information.hThread);

    return true;
}

#else

bool startDetached(const std::filesystem::path &program,
                   const std::vector<std::string> &arguments,
                   const std::filesystem::path &workingDirectory,
                   std::string *error) {
    /*
    Forked twice: the middle process exits at once and the game is adopted by
    init, so nothing is left for ZDL to reap and the game outlives it. The one
    thing that has to travel back is whether exec itself worked, which comes
    through a pipe that the exec closes on success.
    */
    int report[2] = {-1, -1};

    if (pipe(report) != 0) {
        if (error != nullptr) {
            *error = std::strerror(errno);
        }

        return false;
    }

    const pid_t middle = fork();

    if (middle < 0) {
        if (error != nullptr) {
            *error = std::strerror(errno);
        }

        close(report[0]);
        close(report[1]);

        return false;
    }

    if (middle == 0) {
        close(report[0]);

        if (fork() == 0) {
            setsid();

            if (!workingDirectory.empty()) {
                // Not being able to get there is not worth refusing to launch over.
                [[maybe_unused]] const int moved = chdir(workingDirectory.c_str());
            }

            std::vector<std::string> owned;
            owned.reserve(arguments.size() + 1);
            owned.push_back(program.string());
            owned.insert(owned.end(), arguments.begin(), arguments.end());

            std::vector<char *> argv;
            argv.reserve(owned.size() + 1);

            for (std::string &argument : owned) {
                argv.push_back(argument.data());
            }

            argv.push_back(nullptr);

            execv(program.c_str(), argv.data());

            // Only reached when exec failed, and then the number says why.
            const int failure = errno;
            [[maybe_unused]] const ssize_t told = write(report[1], &failure, sizeof(failure));

            _exit(127);
        }

        close(report[1]);
        _exit(0);
    }

    close(report[1]);

    int failure = 0;
    const ssize_t heard = read(report[0], &failure, sizeof(failure));

    close(report[0]);

    // The middle process is gone the moment it has forked, so it is reaped here.
    int status = 0;
    waitpid(middle, &status, 0);

    if (heard == sizeof(failure)) {
        if (error != nullptr) {
            *error = std::strerror(failure);
        }

        return false;
    }

    return true;
}

#endif

}
