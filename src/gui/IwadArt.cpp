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

#include <chrono>
#include <cstdio>
#include <fstream>
#include <utility>

#include "core/Artwork.h"
#include "core/Md5.h"
#include "core/Paths.h"
#include "gui/Convert.h"
#include "gui/IwadArt.h"

namespace {

// Drawn for 4:3 out of non-square pixels, so it is stored a fifth short.
constexpr int SQUASHED = 200;
constexpr int STRETCHED = SQUASHED * 6 / 5;

slint::SharedPixelBuffer<slint::Rgb8Pixel> stretch(
    const slint::SharedPixelBuffer<slint::Rgb8Pixel> &from, const uint32_t height) {
    slint::SharedPixelBuffer<slint::Rgb8Pixel> to(from.width(), height);
    const slint::Rgb8Pixel *source = from.begin();
    slint::Rgb8Pixel *target = to.begin();

    for (uint32_t row = 0; row < height; row++) {
        const float at = ((static_cast<float>(row) + 0.5F)
                          * static_cast<float>(from.height()) / static_cast<float>(height))
            - 0.5F;
        const float clamped = at < 0 ? 0 : at;
        const auto above = static_cast<uint32_t>(clamped);
        const uint32_t below = std::min(above + 1, from.height() - 1);
        const float mix = clamped - static_cast<float>(above);

        for (uint32_t column = 0; column < from.width(); column++) {
            const slint::Rgb8Pixel &one = source[(above * from.width()) + column];
            const slint::Rgb8Pixel &other = source[(below * from.width()) + column];

            target[(row * from.width()) + column] = {
                .r = static_cast<uint8_t>((static_cast<float>(one.r) * (1 - mix))
                                          + (static_cast<float>(other.r) * mix)),
                .g = static_cast<uint8_t>((static_cast<float>(one.g) * (1 - mix))
                                          + (static_cast<float>(other.g) * mix)),
                .b = static_cast<uint8_t>((static_cast<float>(one.b) * (1 - mix))
                                          + (static_cast<float>(other.b) * mix)),
            };
        }
    }

    return to;
}

// Anything nothing has asked for in this long is not being asked for again.
constexpr auto STALE = std::chrono::hours(24 * 30);

std::filesystem::path titles() {
    return Paths::dataDirectory() / "titles";
}

// Slint loads a paletted picture only off disk, so it is put there and kept.
std::filesystem::path spill(const std::string &file, const Artwork::Title &title) {
    const std::filesystem::path where = titles();
    std::error_code code;

    std::filesystem::create_directories(where, code);

    if (code) {
        return {};
    }

    // A digest, not std::hash: the name has to mean the same thing next build.
    // The suffix matters too, since Slint reads an image by its extension.
    std::filesystem::path kept =
        where / (md5Text(file) + std::string(Artwork::suffixOf(title)));

    if (std::filesystem::exists(kept, code)) {
        // Wanted just now, which keeps the pruning below off it.
        std::filesystem::last_write_time(kept, std::filesystem::file_time_type::clock::now(),
                                         code);

        return kept;
    }

    std::ofstream out(kept, std::ios::binary | std::ios::trunc);

    if (!out) {
        return {};
    }

    out.write(title.lump.data(), static_cast<std::streamsize>(title.lump.size()));

    return out ? kept : std::filesystem::path();
}

}

void IwadArt::prune() {
    std::error_code code;
    const auto now = std::filesystem::file_time_type::clock::now();

    for (std::filesystem::directory_iterator walk(titles(), code), end;
         walk != end && !code; walk.increment(code)) {
        std::error_code each;

        if (const std::filesystem::file_time_type when = walk->last_write_time(each);
            !each && now - when > STALE) {
            std::filesystem::remove(walk->path(), each);
        }
    }
}

IwadArt::~IwadArt() {
    {
        const std::scoped_lock hold(_guard);

        _stopping = true;
    }

    _wake.notify_one();

    if (_reader.joinable()) {
        _reader.join();
    }
}

slint::Image IwadArt::of(const std::string &file) {
    if (file.empty()) {
        return {};
    }

    const auto found = _kept.find(file);

    if (found != _kept.end()) {
        return found->second;
    }

    // Empty at once: the shelf draws its placeholder and fills in when read.
    _kept.emplace(file, slint::Image());
    want(file);

    return {};
}

void IwadArt::want(const std::string &file) {
    {
        const std::scoped_lock hold(_guard);

        _wanted.push_back(file);
    }

    // One thread: reading is all disk, and several would queue on the same one.
    if (!_reader.joinable()) {
        _reader = std::thread([this] { work(); });
    }

    _wake.notify_one();
}

void IwadArt::work() {
    for (;;) {
        std::string file;

        {
            std::unique_lock hold(_guard);

            _wake.wait(hold, [this] { return _stopping || !_wanted.empty(); });

            if (_stopping) {
                return;
            }

            file = std::move(_wanted.front());
            _wanted.pop_front();
        }

        // Back to the drawing thread, the only one that may make a picture.
        slint::invoke_from_event_loop([this, done = read(file)] { deliver(done); });
    }
}

void IwadArt::deliver(const Read &done) {
    slint::Image made;

    if (!done.kept.empty()) {
        made = slint::Image::load_from_path(Convert::fromPath(done.kept));

        if (made.size().height == SQUASHED) {
            // Stretching reads the pixels back, which only this build's renderer
            // may have been able to decode.
            if (std::optional<slint::SharedPixelBuffer<slint::Rgb8Pixel>> pixels = made.to_rgb8()) {
                made = slint::Image(stretch(*pixels, STRETCHED));
            }
        }
    } else if (!done.pixels.empty()) {
        const slint::SharedPixelBuffer<slint::Rgb8Pixel> pixels(
            static_cast<uint32_t>(done.width), static_cast<uint32_t>(done.height),
            reinterpret_cast<const slint::Rgb8Pixel *>(done.pixels.data()));

        made = done.height == SQUASHED
            ? slint::Image(stretch(pixels, STRETCHED))
            : slint::Image(pixels);
    }

    _kept[done.file] = std::move(made);
    ++_revision;

    if (arrived) {
        arrived();
    }
}

IwadArt::Read IwadArt::read(const std::string &file) {
    Read done;

    done.file = file;

    const Artwork::Title title = Artwork::titleOf(file);

    if (title.empty()) {
        return done;
    }

    if (title.image) {
        done.kept = spill(file, title);

        return done;
    }

    Artwork::Picture picture = Artwork::decode(title);

    done.width = picture.width;
    done.height = picture.height;
    done.pixels = std::move(picture.pixels);

    return done;
}
