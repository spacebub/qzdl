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

#include <initializer_list>
#include <optional>
#include <ranges>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "core/system/Env.h"
#include "core/system/Process.h"
#include "core/util/Text.h"

#ifdef _WIN32

#include <windows.h>

#else

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <sys/wait.h>
#include <system_error>
#include <termios.h>
#include <unistd.h>

#endif

namespace Process {

namespace {

#ifdef _WIN32
using Native = HANDLE;
#else
using Native = pid_t;
#endif

std::unordered_map<Id, Native> &children() {
    static std::unordered_map<Id, Native> table;

    return table;
}

Id remember(const Native child) {
    static Id next = 1;

    children()[next] = child;

    return next++;
}

#ifdef _WIN32

std::optional<std::wstring> environmentVariable(const std::wstring &name) {
    const DWORD room = GetEnvironmentVariableW(name.c_str(), nullptr, 0);

    if (room == 0) {
        return std::nullopt;
    }

    std::wstring value(room, L'\0');
    const DWORD written = GetEnvironmentVariableW(name.c_str(), value.data(), room);

    value.resize(written);

    return value;
}

#endif

#ifndef _WIN32

// strerror is not thread safe.
std::string describe(int number) {
    return std::generic_category().message(number);
}

void closeAll(std::initializer_list<int> ends) {
    for (const int end : ends) {
        if (end >= 0) {
            close(end);
        }
    }
}

// ends[0] stays with ZDL, ends[1] goes to the child.
bool openTerminal(int (&ends)[2]) {
    const int primary = posix_openpt(O_RDWR | O_NOCTTY);

    if (primary < 0 || grantpt(primary) != 0 || unlockpt(primary) != 0) {
        if (primary >= 0) {
            close(primary);
        }

        return false;
    }

    // NOLINTNEXTLINE(concurrency-mt-unsafe) -- only called from the launch thread.
    const char *name = ptsname(primary);

    if (name == nullptr) {
        close(primary);

        return false;
    }

    const int secondary = open(name, O_RDWR | O_NOCTTY);

    if (secondary < 0) {
        close(primary);

        return false;
    }

    // Echo off: nothing is ever written towards the child.
    if (termios settings = {}; tcgetattr(secondary, &settings) == 0) {
        settings.c_lflag &= ~static_cast<tcflag_t>(ECHO | ECHONL);
        tcsetattr(secondary, TCSANOW, &settings);
    }

    ends[0] = primary;
    ends[1] = secondary;

    return true;
}

// PATH is searched here rather than by execvp, since nothing may allocate after fork.
std::filesystem::path resolve(const std::filesystem::path &program) {
    if (program.has_parent_path()) {
        return program;
    }

    const std::string path = Env::get("PATH");

    for (const auto &part : std::views::split(std::string_view(path), ':')) {
        std::filesystem::path candidate(std::string_view(part.begin(), part.end()));

        if (candidate.empty()) {
            candidate = ".";
        }

        candidate /= program;

        if (access(candidate.c_str(), X_OK) == 0) {
            return candidate;
        }
    }

    return program;
}

std::vector<std::string> environmentWith(const std::map<std::string, std::string> &additions) {
    std::vector<std::string> out;

    for (char * const*each = environ; each != nullptr && *each != nullptr; ++each) {
        const std::string entry(*each);
        const size_t split = entry.find('=');

        if (split == std::string::npos || !additions.contains(entry.substr(0, split))) {
            out.push_back(entry);
        }
    }

    for (const auto &[name, value] : additions) {
        std::string entry = name;
        entry += '=';
        entry += value;
        out.push_back(std::move(entry));
    }

    return out;
}

#endif

}

#ifdef _WIN32

bool start(const std::filesystem::path &program,
           const std::vector<std::string> &arguments,
           const std::filesystem::path &workingDirectory,
           const std::map<std::string, std::string> &environment,
           Id *id,
           Stream *output,
           std::string *error) {
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

    HANDLE readEnd = nullptr;
    HANDLE writeEnd = nullptr;

    if (output != nullptr) {
        SECURITY_ATTRIBUTES inheritable = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};

        if (CreatePipe(&readEnd, &writeEnd, &inheritable, 0) == 0) {
            if (error != nullptr) {
                *error = "the system would not make a pipe (error "
                    + std::to_string(GetLastError()) + ")";
            }

            return false;
        }

        SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

        startup.dwFlags |= STARTF_USESTDHANDLES;
        startup.hStdOutput = writeEnd;
        startup.hStdError = writeEnd;
    }

    PROCESS_INFORMATION information = {};
    const std::wstring directory = workingDirectory.wstring();

    // Set in this process for the child to inherit, then restored.
    std::vector<std::pair<std::wstring, std::optional<std::wstring>>> restore;
    restore.reserve(environment.size());

    for (const auto &[name, value] : environment) {
        std::wstring wide = std::filesystem::path(name).wstring();

        restore.emplace_back(wide, environmentVariable(wide));
        SetEnvironmentVariableW(wide.c_str(), std::filesystem::path(value).wstring().c_str());
    }

    // CreateProcessW may modify the command line.
    std::vector<wchar_t> writable(command.begin(), command.end());
    writable.push_back(L'\0');

    const DWORD creation = NORMAL_PRIORITY_CLASS | CREATE_UNICODE_ENVIRONMENT
        | (output != nullptr ? CREATE_NO_WINDOW : CREATE_NEW_CONSOLE);

    const BOOL started = CreateProcessW(
        nullptr, writable.data(), nullptr, nullptr, output != nullptr ? TRUE : FALSE,
        creation, nullptr, directory.empty() ? nullptr : directory.c_str(),
        &startup, &information);

    const DWORD refused = GetLastError();

    for (const auto &[name, value] : restore) {
        SetEnvironmentVariableW(name.c_str(), value ? value->c_str() : nullptr);
    }

    // The parent's write end must close or EOF never arrives.
    if (writeEnd != nullptr) {
        CloseHandle(writeEnd);
    }

    if (started == 0) {
        if (readEnd != nullptr) {
            CloseHandle(readEnd);
        }

        if (error != nullptr) {
            *error = "the system refused to start it (error " + std::to_string(refused) + ")";
        }

        return false;
    }

    CloseHandle(information.hThread);

    if (id == nullptr) {
        CloseHandle(information.hProcess);
    } else {
        *id = remember(information.hProcess);
    }

    if (output != nullptr) {
        *output = reinterpret_cast<Stream>(readEnd);
    }

    return true;
}

bool read(const Stream output, std::string &into) {
    into.clear();

    if (output == NOTHING) {
        return false;
    }

    const auto pipe = reinterpret_cast<HANDLE>(output);
    DWORD waiting = 0;

    if (PeekNamedPipe(pipe, nullptr, 0, nullptr, &waiting, nullptr) == 0) {
        return false;
    }

    if (waiting == 0) {
        return true;
    }

    into.resize(waiting);

    DWORD got = 0;

    if (ReadFile(pipe, into.data(), waiting, &got, nullptr) == 0) {
        into.clear();

        return false;
    }

    into.resize(got);

    return true;
}

void closeStream(const Stream output) {
    if (output != NOTHING) {
        CloseHandle(reinterpret_cast<HANDLE>(output));
    }
}

void stop(const Id id) {
    const auto found = children().find(id);

    if (found != children().end()) {
        TerminateProcess(found->second, 1);
    }
}

void force(const Id id) {
    stop(id);
}

State poll(const Id id, int *code) {
    const auto found = children().find(id);

    if (found == children().end()) {
        return State::Unknown;
    }

    // STILL_ACTIVE (259) is a valid exit code, so wait instead.
    if (WaitForSingleObject(found->second, 0) == WAIT_TIMEOUT) {
        return State::Running;
    }

    DWORD status = 0;
    const BOOL known = GetExitCodeProcess(found->second, &status);

    CloseHandle(found->second);
    children().erase(found);

    if (known == 0) {
        return State::Unknown;
    }

    if (code != nullptr) {
        *code = static_cast<int>(status);
    }

    return status == 0 ? State::Finished : State::Failed;
}

#else

bool start(const std::filesystem::path &program,
           const std::vector<std::string> &arguments,
           const std::filesystem::path &workingDirectory,
           const std::map<std::string, std::string> &environment,
           Id *id,
           Stream *output,
           std::string *error) {
    // Everything is built before the fork; nothing may allocate between fork and exec.
    std::vector<std::string> variables = environmentWith(environment);

    const std::filesystem::path found = resolve(program);

    std::vector<char *> envp;
    envp.reserve(variables.size() + 1);

    for (std::string &variable : variables) {
        envp.push_back(variable.data());
    }

    envp.push_back(nullptr);

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

    // Exec failure comes back through report; close-on-exec ends it on success.
    int report[2] = {-1, -1};

    if (pipe(report) != 0) {
        if (error != nullptr) {
            *error = describe(errno);
        }

        return false;
    }

    // A pty rather than a pipe keeps the child's stdio line buffered.
    int pty[2] = {-1, -1};

    if (output != nullptr && !openTerminal(pty)) {
        if (error != nullptr) {
            *error = describe(errno);
        }

        close(report[0]);
        close(report[1]);

        return false;
    }

    for (const int end : {report[0], report[1], pty[0], pty[1]}) {
        if (end >= 0 && fcntl(end, F_SETFD, FD_CLOEXEC) != 0) {
            if (error != nullptr) {
                *error = describe(errno);
            }

            closeAll({report[0], report[1], pty[0], pty[1]});

            return false;
        }
    }

    const pid_t child = fork();

    if (child < 0) {
        if (error != nullptr) {
            *error = describe(errno);
        }

        closeAll({report[0], report[1], pty[0], pty[1]});

        return false;
    }

    if (child == 0) {
        close(report[0]);
        setsid();

        // Double fork when untracked, so init reaps the child.
        if (id == nullptr) {
            const pid_t handed = fork();

            if (handed > 0) {
                _exit(0);
            }

            if (handed < 0) {
                const int failure = errno;
                [[maybe_unused]] const ssize_t told = write(report[1], &failure, sizeof(failure));

                _exit(127);
            }
        }

        if (pty[1] >= 0) {
            dup2(pty[1], STDOUT_FILENO);
            dup2(pty[1], STDERR_FILENO);
        }

        if (!workingDirectory.empty()) {
            [[maybe_unused]] const int moved = chdir(workingDirectory.c_str());
        }

        execve(found.c_str(), argv.data(), envp.data());

        const int failure = errno;
        [[maybe_unused]] const ssize_t told = write(report[1], &failure, sizeof(failure));

        _exit(127);
    }

    close(report[1]);

    if (pty[1] >= 0) {
        close(pty[1]);
    }

    int failure = 0;
    const ssize_t heard = ::read(report[0], &failure, sizeof(failure));

    close(report[0]);

    if (heard == sizeof(failure)) {
        int status = 0;
        waitpid(child, &status, 0);

        if (pty[0] >= 0) {
            close(pty[0]);
        }

        if (error != nullptr) {
            *error = describe(failure);
        }

        return false;
    }

    if (output != nullptr) {
        fcntl(pty[0], F_SETFL, fcntl(pty[0], F_GETFL, 0) | O_NONBLOCK);

        *output = pty[0];
    }

    if (id != nullptr) {
        *id = remember(child);
    } else {
        // Reaps the intermediate of the double fork.
        int status = 0;

        waitpid(child, &status, 0);
    }

    return true;
}

bool read(const Stream output, std::string &into) {
    into.clear();

    if (output == NOTHING) {
        return false;
    }

    char buffer[8192];
    const ssize_t got = ::read(static_cast<int>(output), buffer, sizeof(buffer));

    if (got > 0) {
        into.assign(buffer, static_cast<size_t>(got));

        return true;
    }

    return got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR);
}

void closeStream(const Stream output) {
    if (output != NOTHING) {
        close(static_cast<int>(output));
    }
}

void stop(const Id id) {
    const auto found = children().find(id);

    if (found != children().end()) {
        ::kill(found->second, SIGTERM);
    }
}

void force(const Id id) {
    const auto found = children().find(id);

    if (found != children().end()) {
        ::kill(found->second, SIGKILL);
    }
}

State poll(const Id id, int *code) {
    const auto found = children().find(id);

    if (found == children().end()) {
        return State::Unknown;
    }

    int status = 0;
    const pid_t done = waitpid(found->second, &status, WNOHANG);

    if (done == 0) {
        return State::Running;
    }

    // Stopped, not ended.
    if (done > 0 && !WIFEXITED(status) && !WIFSIGNALED(status)) {
        return State::Running;
    }

    children().erase(found);

    if (done < 0) {
        return State::Unknown;
    }

    if (WIFSIGNALED(status)) {
        if (code != nullptr) {
            *code = -WTERMSIG(status);
        }

        return State::Failed;
    }

    if (code != nullptr) {
        *code = WEXITSTATUS(status);
    }

    return WEXITSTATUS(status) == 0 ? State::Finished : State::Failed;
}

#endif

}
