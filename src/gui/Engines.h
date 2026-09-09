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
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/Catalog.h"
#include "main.h"
#include "gui/ConfigBridge.h"
#include "gui/Models.h"
#include "gui/Http.h"
#include "gui/Notifier.h"

class Engines {
public:
    // Out here because the putting itself happens off the interface's thread.
    struct Placed {
        std::filesystem::path program;
        std::string trouble;

        // Worth a message of its own, or empty where the row saying so is enough.
        std::string headline;
    };

    Engines(const ui::Zdl *window, Notifier *notifier, ConfigBridge *config);

    // Every port already unpacked put back into the config's own list, for a
    // config that was replaced or never knew about them.
    void relist() const;

    // Every catalog port this machine already has, put into the list once each.
    // One taken out again is remembered rather than offered a second time.
    void discover();

private:
    // One archive unpacked on a thread of its own: seconds of work that would
    // otherwise stop the window painting. No lock needed -- the thread writes the
    // answer and sets `done` last, and this side reads neither until it is set.
    struct Unpacking {
        std::thread worker;
        std::atomic<bool> done{false};

        Placed answer;
        std::string name;

        Unpacking() = default;

        ~Unpacking() {
            if (worker.joinable()) {
                worker.join();
            }
        }

        Unpacking(const Unpacking &) = delete;
        Unpacking &operator=(const Unpacking &) = delete;
        Unpacking(Unpacking &&) = delete;
        Unpacking &operator=(Unpacking &&) = delete;
    };

    struct Entry {
        // waiting | checking | ready | elsewhere | unavailable | fetching
        // | unpacking | installed | failed
        std::string state{"waiting"};

        std::string version;
        std::string have;
        std::string url;
        std::string asset;
        std::string file;
        std::string error;
        long long size{0};
        double progress{0};

        std::string verdict{"waiting"};
        std::string note;

        // When GitHub last answered about this one, in seconds, or nothing where
        // it never has. Kept across runs, so closing ZDL does not spend the
        // hour's allowance over again.
        long long checked{0};

        // In flight for this row, and which of the two questions it is asking.
        std::unique_ptr<Http::Fetch> fetch;
        bool asking{false};

        std::unique_ptr<Unpacking> unpacking;

        // Where a download is being written, and where it lands.
        std::filesystem::path partial;
        std::filesystem::path into;

        // The release is only being asked after so that it can be fetched.
        bool wanted{false};
    };

    [[nodiscard]] static const Catalog::Port &port(int row);

    // Where a row stands before anything is asked: unpacked, waiting, or not
    // fetchable here at all.
    void settle(int row);

    // Only the ports nothing recent was heard about, unless `everything`.
    void refresh(bool everything);

    // The rows and what Releases keeps of them, each put into the other.
    // Read once at startup, written whenever an answer arrives.
    void readCache();
    void writeCache() const;

    void check(int row);
    void fetch(int row);
    void install(int row);
    void cancel(int row) const;

    void unpack(int row, const std::filesystem::path &archive);

    // The other end of it, once the thread has finished.
    void unpacked(int row);

    void adopt(int row, const std::string &file);

    // The config's list brought in line with what is on disk: an entry pointing
    // inside this port's directory is moved to the build that is there now, and
    // a port with no entry at all is given one. True when it added one.
    bool enlist(int row, const std::string &before = {}) const;

    void erase(int row);

    void remove(int row);

    // The same from the other end: a port list row goes, and its files with it.
    void forget(int listed);

    void measure();
    void clearDownloads();

    void sweep();

    void give(int row, const std::string &state, const std::string &error = {});

    void push();

    static constexpr std::chrono::milliseconds TICK{80};

    const ui::Zdl *_window;
    Notifier *_notifier;
    ConfigBridge *_config;

    // Handed over once and changed in place: a bar moving twelve times a second
    // is one row of the grid, not the grid.
    std::shared_ptr<slint::VectorModel<ui::EngineBrowseRow>> _rows
        = std::make_shared<slint::VectorModel<ui::EngineBrowseRow>>();

    std::vector<Entry> _entries;
    std::string _trouble;

    // What the downloads came to when they were last measured.
    long long _cached{0};

    slint::Timer _clock;
};
