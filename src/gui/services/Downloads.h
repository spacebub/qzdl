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
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "ttk/notices/Notifier.h"
#include "ttk/system/Http.h"
#include "ttk/util/Clock.h"

#include "core/config/Config.h"
#include "core/ports/Catalog.h"
#include "core/ports/Detect.h"
#include "core/ports/Install.h"
#include "gui/state/State.h"

class Downloads {
public:
    Downloads(ttk::Clock *clock, ttk::Notifier *notifier);
    ~Downloads();

    Downloads(const Downloads &) = delete;
    Downloads &operator=(const Downloads &) = delete;
    Downloads(Downloads &&) = delete;
    Downloads &operator=(Downloads &&) = delete;

    // The config side of installing a port, wired by the window. The service says what an engine holds, and the
    // window puts it in the engine list.
    std::function<std::string(const Engine &engine)> addEngine;
    std::function<void(int at, const std::string &name, const std::string &file, bool dos)> updateEngine;
    std::function<void(int at)> removeEngine;
    std::function<void()> scheduleSave;

    // The releases that Flatpak installs of listed engines name, read at start-up, when a config opens and on every
    // find.
    std::function<void(std::span<const Detect::Installed> read)> storeVersions;

    // Puts every unpacked port back into the engine list, and repoints engines whose file is gone.
    void relist() const;

    // Another config is in place. Refreshes the versions of its engines and, with detect set, finds the engines on
    // this machine when its list is empty. A read still running for the config before is done again for this one.
    void opened(bool detect);

    // Finds the engines on this machine, adds the ones the list lacks and refreshes the versions of listed Flatpaks.
    // A click while a find runs joins it.
    void find();

    // Only rows with no recent answer, unless everything.
    void refresh(bool everything);

    // Asks nothing of the network. For the benchmark rig, which would otherwise
    // spend the hour's rate limit on a run.
    void setOffline(const bool value) { _offline = value; }

    void install(int row);
    void cancel(int row) const;

    // The files of the port go, and the engine ZDL4 keeps for it.
    void remove(int row);

    // An engine goes, and its files with it.
    void forget(int listed);

    void measure();
    void clearDownloads();

private:
    // No lock: the worker sets done last, and this side reads nothing before.
    struct Unpacking {
        std::thread worker;
        std::atomic<bool> done{false};

        Install::Placed answer;
        std::string name;

        // Just downloaded, rather than taken off the shelf.
        bool fresh{false};

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
        State::DownloadStatus state = State::DownloadStatus::Waiting;

        std::string version;
        std::string have;
        std::string url;
        std::string asset;
        std::string file;
        std::string error;
        long long size{0};

        // What the download is held to, with the size: from the release, or the catalogue.
        std::string sha256;

        double progress{0};

        State::DownloadStatus verdict = State::DownloadStatus::Waiting;
        std::string note;

        // Seconds. Kept across runs so the hour's allowance is not spent again.
        long long checked{0};

        std::unique_ptr<ttk::Http::Fetch> fetch;

        // The fetch is a release check rather than a download.
        bool asking{false};

        std::unique_ptr<Unpacking> unpacking;

        // Where a download is written, and where it lands.
        std::filesystem::path partial;
        std::filesystem::path into;

        // The release is asked after in order to fetch it.
        bool wanted{false};
    };

    [[nodiscard]] static const Catalog::Download &download(int row);

    // Initial state: unpacked, waiting, or not fetchable here.
    void settle(int row);

    void readCache();
    void writeCache() const;

    void check(int row);
    void fetch(int row);

    // Off the interface thread. A fresh download is held to what was promised and
    // moved onto the shelf first. A shelf copy is held to it again, and one that
    // no longer matches is fetched afresh.
    void unpack(int row, bool fresh);

    void unpacked(int row);

    void adopt(int row, const std::string &file);

    // The config row ZDL4 keeps for this download: managed, and of its port. -1 for none.
    [[nodiscard]] static int rowOf(int row);

    // Puts the port in the engine list when no engine speaks for it. True when it added one.
    [[nodiscard]] bool enlist(int row) const;

    // Points the engine of the port at what was just unpacked.
    void repoint(int row) const;

    // Points the engine of the port at the shelf copy when the file it names is gone.
    void repointMissing(int row) const;

    void erase(int row);

    void sweep();

    // Starts the queued questions that fit under ASKING.
    void pump();

    void give(int row, State::DownloadStatus state, const std::string &error = {});

    void push();

    // What a read off the interface thread found, for the engine list as it was when the read began.
    struct Looked {
        // How many configs were opened when the read began.
        int opened{0};
        bool walked{false};

        // Size of the downloads. Negative when not measured.
        long long held{-1};

        std::vector<Engine> compared;
        std::vector<Engine> fresh;
        std::vector<Detect::Installed> read;
    };

    // Reads the versions of the listed engines off the interface thread, and sizes the downloads with sizeShelf. Walks
    // the disk as well when a find was asked, or when the list is empty and an unasked find may fill it. One read runs
    // at a time.
    void look(bool sizeShelf = false);

    // An asked find adds to a list that is not empty and says when it found nothing.
    void looked(const Looked &result);

    static constexpr double TICK = 0.08;

    // How many releases are asked about at once.
    static constexpr size_t ASKING = 3;

    ttk::Clock *_clock;
    ttk::Notifier *_notifier;

    std::vector<Entry> _entries;
    std::string _trouble;

    // Size of the downloads when last measured.
    long long _cached{0};

    int _ticker{0};

    // Rows waiting for a turn at asking GitHub.
    std::vector<int> _queued;

    bool _offline{false};

    std::thread _reader;
    bool _reading{false};
    bool _walking{false};
    bool _findAsked{false};

    // False for a config emptied on purpose, which an unasked find leaves empty.
    bool _detect{true};

    // Counts the configs opened, so that a read for one before is known.
    int _opened{0};

    std::shared_ptr<std::atomic<bool>> _alive = std::make_shared<std::atomic<bool>>(true);
};
