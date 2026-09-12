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

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <blend2d/blend2d.h>

#include "gui/app/Shell.h"

// A card's title screen: the last add-on carrying one, else the game's. Read on
// its own thread, cached on disk by source file, and held in memory to a budget.
class IwadArt {
public:
    explicit IwadArt(Shell *shell) : _shell(shell) {}

    ~IwadArt();

    IwadArt(const IwadArt &) = delete;
    IwadArt &operator=(const IwadArt &) = delete;
    IwadArt(IwadArt &&) = delete;
    IwadArt &operator=(IwadArt &&) = delete;

    // Call once before anything is drawn.
    static void prune();

    // The key is the game then its enabled add-ons, newline separated. Empty when
    // nothing has been read yet; the card draws its gradient until one arrives.
    [[nodiscard]] BLImage of(const std::string &key);

    // Bumped whenever a card's picture changes.
    [[nodiscard]] int revision() const { return _revision; }

    // Called on the interface thread when revision moved.
    std::function<void()> arrived;

private:
    struct Read {
        std::string key;

        // The entry it came from; cards from the same file share it. Empty for none.
        std::string name;

        // An image file put aside on disk, decoded here rather than by the reader.
        std::filesystem::path kept;

        int width{0};
        int height{0};
        std::vector<std::uint8_t> pixels;
    };

    void want(const std::string &key);
    void work();
    void deliver(const Read &done);
    void evict();
    void take(const std::string &name);
    void drop(const std::string &name);

    [[nodiscard]] BLImage imageOf(const std::string &name) const;
    [[nodiscard]] static Read read(const std::string &key);

    // Shared by the cards drawn from it.
    struct Picture {
        BLImage image;
        size_t bytes{0};
        size_t cards{0};
    };

    struct Card {
        std::string name;
        int seen{0};
        bool pending{false};
        std::list<std::string>::iterator at;
    };

    Shell *_shell;

    std::unordered_map<std::string, Picture> _pictures;
    std::unordered_map<std::string, Card> _cards;

    // Most recently used first.
    std::list<std::string> _order;
    size_t _bytes{0};
    int _revision{0};

    // Cards asked for under this revision are on screen and are not evicted.
    int _seen{0};

    std::mutex _guard;
    std::condition_variable _wake;
    std::deque<std::string> _wanted;
    bool _stopping{false};
    std::thread _reader;
};
