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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

// Fetching one thing over HTTPS, on a thread of its own. Polled rather than
// reporting in, so the drawing thread needs no locking around it.
namespace Http {

// One-time setup, done while there is still one thread: libcurl's implicit
// version of it is not safe from several fetches starting at once.
void start();

void stop();

class Fetch {
public:
    // Into a file, or held in memory when `into` is empty. `json` is the Accept header.
    Fetch(std::string url, bool json, std::filesystem::path into);

    ~Fetch();

    Fetch(const Fetch &) = delete;
    Fetch &operator=(const Fetch &) = delete;
    Fetch(Fetch &&) = delete;
    Fetch &operator=(Fetch &&) = delete;

    [[nodiscard]] bool done() const { return _done.load(); }

    // Asks it to stop; done() still says when it has.
    void cancel() { _cancelled.store(true); }

    [[nodiscard]] bool cancelled() const { return _cancelled.load(); }

    // 0 to 1, or zero where the other end gave no size.
    [[nodiscard]] double progress() const { return _progress.load(); }

    [[nodiscard]] int status() const { return _status.load(); }

    // Empty when it worked.
    [[nodiscard]] std::string error() const;

    [[nodiscard]] std::string body() const;

private:
    void work();

    // Long enough for a slow mirror, short enough that a dead one gives up.
    static constexpr long STALL_SECONDS = 30;

    std::string _url;
    bool _json;
    std::filesystem::path _into;

    std::atomic<bool> _done{false};
    std::atomic<bool> _cancelled{false};
    std::atomic<double> _progress{0};
    std::atomic<int> _status{0};

    mutable std::mutex _guard;
    std::string _error;
    std::string _body;

    std::thread _thread;
};

}
