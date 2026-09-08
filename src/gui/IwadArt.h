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

#include <slint.h>

/*
The title screen a card is drawn from: the last add-on carrying one, and failing
that the game itself, in the order the port is handed them. A mod whose title is
a map rather than a picture has nothing to take, and leaves the game's showing.

Reading walks a WAD or a zip's directory, so it happens on a thread of its own:
a card gets what is already known now and is told when the real one arrives.

What is found is put aside on disk and keyed by what it was read out of, so a
file is only ever looked through once, however many profiles load it. What is
drawn from is held in memory to a budget, since a picture costs its whole size
in pixels there and a config can name any number of them.
*/
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

    /*
    The picture for a card, or the nearest thing to one while it is still being
    read. The key is the game and then its enabled add-ons in the order the port
    is handed them, a newline between each; a game on its own is its own key.
    */
    [[nodiscard]] slint::Image of(const std::string &key);

    // Moves whenever a card's picture changes, so bindings on it are worked
    // out again.
    [[nodiscard]] int revision() const { return _revision; }

    // Called on the interface's thread when the revision has moved.
    std::function<void()> arrived;

private:
    // What the reading thread found: a Slint picture is made on the drawing thread.
    struct Read {
        std::string key;

        // The entry it came out of, which is what two cards drawn from the
        // same file share. Empty for no picture.
        std::string name;

        // A picture that names its own colours, put aside as it lay: the only
        // way Slint will read one is off a file.
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

    [[nodiscard]] slint::Image imageOf(const std::string &name) const;
    [[nodiscard]] static Read read(const std::string &key);

    // A picture and how many cards are drawn from it: held once between them.
    struct Picture {
        slint::Image image;
        size_t bytes{0};
        size_t cards{0};
    };

    // What a card is drawn from, and where it sits in the order last asked for.
    struct Card {
        std::string name;
        int seen{0};
        bool pending{false};
        std::list<std::string>::iterator at;
    };

    std::unordered_map<std::string, Picture> _pictures;
    std::unordered_map<std::string, Card> _cards;

    // Most recently asked for first, so the far end is what goes.
    std::list<std::string> _order;
    size_t _bytes{0};
    int _revision{0};

    // The revision anything was last asked under: a card asked then is on
    // screen, and letting it go would only have it asked for again.
    int _seen{0};

    // What the thread has been asked for, and what tells it to stop.
    std::mutex _guard;
    std::condition_variable _wake;
    std::deque<std::string> _wanted;
    bool _stopping{false};
    std::thread _reader;
};
