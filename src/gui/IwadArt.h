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
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <slint.h>

// A game's title screen, read out of its IWAD. Reading walks a WAD, so it happens
// on a thread of its own: a card gets an empty picture now and is told when the
// real one arrives. Kept, since the shelf asks for every card again on any change.
class IwadArt {
public:
    IwadArt() = default;

    ~IwadArt();

    IwadArt(const IwadArt &) = delete;
    IwadArt &operator=(const IwadArt &) = delete;
    IwadArt(IwadArt &&) = delete;
    IwadArt &operator=(IwadArt &&) = delete;

    // Called once before anything is drawn; whatever is still in use is re-read.
    static void prune();

    // The picture for a file, or an empty one while it is still being read.
    [[nodiscard]] slint::Image of(const std::string &file);

    // Moves whenever a picture arrives, so bindings on it are worked out again.
    [[nodiscard]] int revision() const { return _revision; }

    // Called on the interface's thread when the revision has moved.
    std::function<void()> arrived;

private:
    // What the reading thread found: a Slint picture is made on the drawing thread.
    struct Read {
        std::string file;

        // A paletted picture, spilled to disk: the only way Slint will load one.
        std::filesystem::path kept;

        int width{0};
        int height{0};
        std::vector<std::uint8_t> pixels;
    };

    void want(const std::string &file);
    void work();
    void deliver(const Read &done);

    [[nodiscard]] static Read read(const std::string &file);

    std::unordered_map<std::string, slint::Image> _kept;
    int _revision{0};

    // What the thread has been asked for, and what tells it to stop.
    std::mutex _guard;
    std::condition_variable _wake;
    std::deque<std::string> _wanted;
    bool _stopping{false};
    std::thread _reader;
};
