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
#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace Http {

// curl_global_init is not thread safe, so it runs before any fetch.
void start();

void stop();

// Runs on its own thread; poll done().
class Fetch {
public:
    // Into a file, or in memory when into is empty. json sets the Accept header.
    Fetch(std::string url, bool json, std::filesystem::path into);

    ~Fetch();

    Fetch(const Fetch &) = delete;
    Fetch &operator=(const Fetch &) = delete;
    Fetch(Fetch &&) = delete;
    Fetch &operator=(Fetch &&) = delete;

    [[nodiscard]] bool done() const { return _done.load(); }

    void cancel() { _cancelled.store(true); }

    [[nodiscard]] bool cancelled() const { return _cancelled.load(); }

    // 0 to 1; stays 0 without a Content-Length.
    [[nodiscard]] double progress() const { return _progress.load(); }

    [[nodiscard]] int status() const { return _status.load(); }

    // Empty when it worked.
    [[nodiscard]] std::string error() const;

    [[nodiscard]] std::string body() const;

private:
    void work();

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
