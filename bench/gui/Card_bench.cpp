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

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#include "ttk/draw/Theme.h"

#include "core/wad/Artwork.h"
#include "gui/components/LibraryCard.h"
#include "gui/draw/Cards.h"
#include "support/Canvas.h"
#include "support/Corpus.h"

using namespace ttk;

namespace {

bench::Canvas &sheet() {
    static bench::Canvas made(1280, 800);

    return made;
}

// The title screen a card shows, decoded once from the corpus IWAD.
const BLImage &artwork() {
    static const BLImage made = [] {
        const Artwork::Picture picture = Artwork::decode(Artwork::titleOf(bench::Corpus::iwad()));

        BLImage image;

        if (picture.empty()) {
            return image;
        }

        image.create(picture.width, picture.height, BL_FORMAT_PRGB32);

        BLImageData data{};

        image.make_mutable(&data);

        auto *pixels = static_cast<uint8_t *>(data.pixel_data);

        for (int y = 0; y < picture.height; ++y) {
            std::memcpy(pixels + (static_cast<size_t>(y) * data.stride),
                        picture.pixels.data() + (static_cast<size_t>(y) * picture.width * 4),
                        static_cast<size_t>(picture.width) * 4);
        }

        return image;
    }();

    return made;
}

std::unique_ptr<components::LibraryCard> card() {
    auto made = std::make_unique<components::LibraryCard>();

    made->title = "Eviternity II";
    made->subtitle = "Doom II";
    made->caption = "GZDoom";
    made->artKey = "doom2.wad";
    made->playHint = "Launch Eviternity II";
    made->badges = {{.text = "MAP07", .kind = State::BadgeKind::None, .dot = false},
                    {.text = "9 files", .kind = State::BadgeKind::Muted, .dot = false},
                    {.text = "Ultra-Violence", .kind = State::BadgeKind::Muted, .dot = false}};
    made->actions = {};
    made->draggable = true;
    made->artwork = [](const std::string &) { return artwork(); };

    return made;
}

components::LibraryCard *placed() {
    return bench::mount(sheet(), card(), Cards::width, 232.0);
}

// First paint: the rest and lit sprites are built, which is what a resize costs.
void Card_paintFirst(benchmark::State &state) {
    for ([[maybe_unused]] auto step : state) {
        state.PauseTiming();

        components::LibraryCard *made = placed();

        state.ResumeTiming();

        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Card_paintFirst);

// At rest: one blit of the still image, shadow and all.
void Card_paintRest(benchmark::State &state) {
    components::LibraryCard *made = placed();

    bench::paintOnce(sheet(), *made);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Card_paintRest);

// Hovered: the face is turned to the pointer and laid down in cells.
void Card_paintTurned(benchmark::State &state) {
    components::LibraryCard *made = placed();

    bench::paintOnce(sheet(), *made);

    made->enter();
    made->hover(ttk::Pointer{.x = 60.0, .y = 60.0});

    double now = 0.0;

    for (int at = 0; at < 120; ++at) {
        now += 1.0 / 280.0;

        made->advance(now);
    }

    bench::paintOnce(sheet(), *made);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Card_paintTurned);

void Card_advanceStill(benchmark::State &state) {
    components::LibraryCard *made = placed();

    double now = 0.0;

    for ([[maybe_unused]] auto step : state) {
        now += 1.0 / 280.0;

        benchmark::DoNotOptimize(made->advance(now));
    }
}

BENCHMARK(Card_advanceStill);

void Card_hover(benchmark::State &state) {
    components::LibraryCard *made = placed();

    made->enter();

    double x = 0.0;

    for ([[maybe_unused]] auto step : state) {
        x = x > Cards::width ? 0.0 : x + 1.0;

        made->hover(ttk::Pointer{.x = x, .y = 60.0});
    }
}

BENCHMARK(Card_hover);

void Card_drawn(benchmark::State &state) {
    components::LibraryCard *made = placed();

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->drawn());
    }
}

BENCHMARK(Card_drawn);

void Card_spread(benchmark::State &state) {
    const BLRect box{0, 0, Cards::width, 232.0};

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(components::LibraryCard::spread(box));
    }
}

BENCHMARK(Card_spread);

// A shelf of resting cards, which is the library at idle.
void Card_shelfAtRest(benchmark::State &state) {
    const auto count = static_cast<int>(state.range(0));

    sheet().ui().content()->clear();

    std::vector<components::LibraryCard *> made;

    made.reserve(static_cast<size_t>(count));

    for (int at = 0; at < count; ++at) {
        components::LibraryCard *one = sheet().ui().content()->append(card());

        made.push_back(one);
    }

    sheet().ui().relayout();
    sheet().ui().settle();

    for (int at = 0; at < count; ++at) {
        const double x = (at % 4) * (Cards::width + Theme::gap);
        const double y = (at / 4) * 244.0;

        static_cast<ttk::Widget *>(made[static_cast<size_t>(at)])
            ->place(BLRect{x, y, Cards::width, 232.0}, sheet().type());

        bench::paintOnce(sheet(), *made[static_cast<size_t>(at)]);
    }

    for ([[maybe_unused]] auto step : state) {
        for (components::LibraryCard *one : made) {
            benchmark::DoNotOptimize(bench::paintOnce(sheet(), *one));
        }
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(Card_shelfAtRest)->Arg(8)->Arg(24);

}
