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
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/ports/Catalog.h"
#include "main.h"
#include "gui/Http.h"
#include "gui/bridge/ConfigBridge.h"
#include "gui/components/Notifier.h"

class Engines {
public:
    struct Placed {
        std::filesystem::path program;
        std::string trouble;

        // A message of its own; empty when the row suffices.
        std::string headline;
    };

    Engines(const ui::Zdl *window, Notifier *notifier, ConfigBridge *config);

    // Puts every unpacked port back into the config's list.
    void relist() const;

    // Adds every detected port once; removed ones are not offered again.
    void discover();

private:
    // No lock: the worker sets done last, and this side reads nothing before.
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

        // Seconds; kept across runs so the hour's allowance is not spent again.
        long long checked{0};

        std::unique_ptr<Http::Fetch> fetch;

        // The fetch is a release check rather than a download.
        bool asking{false};

        std::unique_ptr<Unpacking> unpacking;

        // Where a download is written, and where it lands.
        std::filesystem::path partial;
        std::filesystem::path into;

        // The release is asked after in order to fetch it.
        bool wanted{false};
    };

    [[nodiscard]] static const Catalog::Port &port(int row);

    // Initial state: unpacked, waiting, or not fetchable here.
    void settle(int row);

    // Only rows with no recent answer, unless everything.
    void refresh(bool everything);

    void readCache();
    void writeCache() const;

    void check(int row);
    void fetch(int row);
    void install(int row);
    void cancel(int row) const;

    void unpack(int row, const std::filesystem::path &archive);

    void unpacked(int row);

    void adopt(int row, const std::string &file);

    // Brings the config's entry in line with what is on disk. True when it added one.
    bool enlist(int row, const std::string &before = {}) const;

    void erase(int row);

    void remove(int row);

    // A port list row goes, and its files with it.
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

    // Changed in place, so a moving bar updates one row.
    std::shared_ptr<slint::VectorModel<ui::EngineBrowseRow>> _rows
        = std::make_shared<slint::VectorModel<ui::EngineBrowseRow>>();

    std::vector<Entry> _entries;
    std::string _trouble;

    // Size of the downloads when last measured.
    long long _cached{0};

    slint::Timer _clock;
};
