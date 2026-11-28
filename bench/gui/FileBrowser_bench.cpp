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
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <benchmark/benchmark.h>

#include "ttk/dialogs/FilePicker.h"
#include "ttk/toolkit/Event.h"
#include "ttk/toolkit/controls/Field.h"

#include "core/config/FileBrowser.h"
#include "core/config/Session.h"
#include "gui/components/AddonList.h"
#include "gui/components/BrowserBand.h"
#include "gui/model/BrowserBridge.h"
#include "gui/services/Filters.h"
#include "gui/state/State.h"
#include "support/Canvas.h"
#include "support/Corpus.h"
#include "support/Fixtures.h"
#include "support/Rig.h"

namespace {

using Clock = std::chrono::steady_clock;

constexpr double ROW = components::BrowserRows::ROW;
constexpr ttk::Key BACKSPACE{
        .code = ttk::Code::Backspace,
        .ctrl = false,
        .shift = false,
        .alt = false,
        .repeat = false,
};

// About 200 files in four folders, and about 3000 in a hundred folders three deep.
const bench::Corpus::Tree &tree(const int files) {
    return bench::Corpus::tree(files, files < 1000 ? 1 : 3);
}

// Spread over the tree, so the folders that hold them open on the first listing.
std::vector<std::string> picksOf(const bench::Corpus::Tree &from) {
    std::vector<std::string> out;

    out.reserve(8);

    for (size_t at = 0; at < 8; ++at) {
        out.push_back(from.files[(at * from.files.size()) / 8]);
    }

    return out;
}

enum class Shown : std::uint8_t {
    Off,
    Folded,
    Some,
    All,
};

template <typename Kind>
Kind *first(ttk::Widget &from) {
    if (auto *hit = dynamic_cast<Kind *>(&from); hit != nullptr) {
        return hit;
    }

    for (const ttk::Widget::Ptr &child : from.children()) {
        if (Kind *found = first<Kind>(*child); found != nullptr) {
            return found;
        }
    }

    return nullptr;
}

components::BrowserBand &bandOf(bench::Rig &rig) {
    return *first<components::BrowserBand>(rig.profile());
}

components::BrowserRows &rowsOf(bench::Rig &rig) {
    return *first<components::BrowserRows>(rig.profile());
}

// The profile page with `files` in the tree and 16 add-ons from elsewhere, or `addons` of them.
bench::Rig &page(const Shown band, const int files, const int addons = 16) {
    bench::Rig &rig = bench::shared();
    const bench::Corpus::Tree &listed = tree(files);

    rig.forget();

    // A listing of the same folder keeps the folds of the last one, so the last benchmark is listed away first.
    bench::Fixtures::install(bench::Fixtures::config(1, 0));
    rig.config().reload();
    BrowserBridge::setFilter({});

    Config made = bench::Fixtures::config(64, addons);

    made.general.fileBrowser = band != Shown::Off;
    made.general.fileBrowserOpen = band == Shown::Some || band == Shown::All;
    made.general.fileBrowserFolder = listed.root.generic_string();

    for (const std::string &path : picksOf(listed)) {
        made.profiles.front().files.push_back(FileEntry{.file = path, .enabled = true, .gone = false, .group = 0});
    }

    bench::Fixtures::install(std::move(made));
    rig.config().reload();

    if (band == Shown::All) {
        const std::vector<State::BrowserRow> &rows = State::get().browser.rows;

        for (size_t at = 0; at < rows.size(); ++at) {
            if (rows[at].folder && rows[at].folded) {
                BrowserBridge::fold(static_cast<int>(at), false);
            }
        }
    }

    rig.go(State::Page::Profile);
    rig.sync();
    rig.ready();

    return rig;
}

// The first line on screen that shows a file.
int fileLine() {
    const State::BrowserState &state = State::get().browser;

    for (size_t line = 0; line < state.shown.size(); ++line) {
        if (!state.rows[static_cast<size_t>(state.shown[line])].folder) {
            return static_cast<int>(line);
        }
    }

    return -1;
}

double lineY(bench::Rig &rig, const int line) {
    return rowsOf(rig).box().y + (line * ROW) + (ROW / 2.0);
}

ttk::Pointer point(const double x, const double y) {
    return ttk::Pointer{.x = x, .y = y, .button = ttk::Click::Left, .ctrl = false, .shift = false};
}

bool wanted(const std::string_view name) {
    return ttk::FilePicker::passes(Filters::addon(), name);
}

void FileBrowser_scan(benchmark::State &state) {
    const auto files = static_cast<int>(state.range(0));
    const bench::Corpus::Tree &listed = tree(files);
    size_t found = 0;

    for ([[maybe_unused]] const auto step : state) {
        found = FileBrowser::scan(listed.root, wanted).size();
    }

    state.counters["rows"] = static_cast<double>(found);
    state.SetItemsProcessed(state.iterations() * files);
}

BENCHMARK(FileBrowser_scan)->Arg(200)->Arg(3000)->Unit(benchmark::kMicrosecond);

// A click toggles one file, and every other one takes it back. A Shift run adds the next 50 and takes them back.
void FileBrowser_pick(benchmark::State &state) {
    const bench::Corpus::Tree &listed = tree(static_cast<int>(state.range(0)));
    const auto run = static_cast<size_t>(state.range(1));
    std::vector<std::string> rows;
    std::vector<std::string> ran;

    for (const FileBrowser::Entry &entry : FileBrowser::scan(listed.root, wanted)) {
        rows.push_back(entry.folder ? std::string() : fileKey(entry.path));

        if (!entry.folder && ran.size() < run) {
            ran.push_back(entry.path);
        }
    }

    std::vector<FileEntry> files = bench::Fixtures::config(1, 16).profiles.front().files;
    const std::vector<std::string> picks = picksOf(listed);
    std::vector<std::string> more = picks;

    for (const std::string &path : picks) {
        files.push_back(FileEntry{.file = path, .enabled = true, .gone = false, .group = 0});
    }

    more.insert(more.end(), ran.begin(), ran.end());

    bool added = false;

    for ([[maybe_unused]] const auto step : state) {
        added = !added;

        benchmark::DoNotOptimize(FileBrowser::pick(files, rows, added ? more : picks));
    }
}

BENCHMARK(FileBrowser_pick)->ArgsProduct({{200, 3000}, {1, 50}})->ArgNames({"files", "run"});

// Unfolding the band lists the folder again.
void FileBrowser_bridgeRescan(benchmark::State &state) {
    bench::Rig &rig = page(Shown::Some, static_cast<int>(state.range(0)));

    for ([[maybe_unused]] const auto step : state) {
        rig.config().browser().setOpen(true);
    }
}

BENCHMARK(FileBrowser_bridgeRescan)->Arg(200)->Arg(3000)->Unit(benchmark::kMicrosecond);

// A file list change: the picks are marked again, and one moved.
void FileBrowser_bridgePush(benchmark::State &state) {
    const auto files = static_cast<int>(state.range(0));
    bench::Rig &rig = page(Shown::Some, files);
    std::vector<FileEntry> &list = Session::get().config().activeProfile().files;
    const std::string extra = tree(files).files[1];
    bool added = false;

    for ([[maybe_unused]] const auto step : state) {
        added = !added;

        if (added) {
            list.push_back(FileEntry{.file = extra, .enabled = true, .gone = false, .group = 0});
        } else {
            list.pop_back();
        }

        rig.config().browser().push();
    }

    if (added) {
        list.pop_back();
    }
}

BENCHMARK(FileBrowser_bridgePush)->Arg(200)->Arg(3000);

void FileBrowser_bridgeFold(benchmark::State &state) {
    page(Shown::All, static_cast<int>(state.range(0)));

    bool folded = false;

    for ([[maybe_unused]] const auto step : state) {
        folded = !folded;

        BrowserBridge::fold(0, folded);
    }

    BrowserBridge::fold(0, false);
}

BENCHMARK(FileBrowser_bridgeFold)->Arg(200)->Arg(3000);

void FileBrowser_bridgeFilter(benchmark::State &state) {
    page(Shown::Some, static_cast<int>(state.range(0)));

    const std::string needle = "hell";
    size_t at = 0;

    for ([[maybe_unused]] const auto step : state) {
        BrowserBridge::setFilter(needle.substr(0, (at++ % needle.size()) + 1));
    }

    BrowserBridge::setFilter({});
}

BENCHMARK(FileBrowser_bridgeFilter)->Arg(200)->Arg(3000);

// The whole window, or only what one change in the band damages. With the browser off that is the add-on list.
void paint(benchmark::State &state, const Shown band, const int files, const int addons = 16) {
    bench::Rig &rig = page(band, files, addons);
    const bool full = state.range(1) != 0;
    const ttk::Widget *changed =
            band == Shown::Off ? static_cast<ttk::Widget *>(first<components::AddonList>(rig.profile())) : &bandOf(rig);
    std::size_t painted = 0;

    for ([[maybe_unused]] const auto step : state) {
        if (full) {
            painted = rig.canvas().full();
        } else {
            changed->invalidate();
            painted = rig.canvas().frame();
        }
    }

    state.counters["pixels"] = static_cast<double>(painted);
}

void FileBrowser_paintOff(benchmark::State &state) {
    paint(state, Shown::Off, static_cast<int>(state.range(0)));
}

void FileBrowser_paintFolded(benchmark::State &state) {
    paint(state, Shown::Folded, static_cast<int>(state.range(0)));
}

void FileBrowser_paintSome(benchmark::State &state) {
    paint(state, Shown::Some, static_cast<int>(state.range(0)));
}

void FileBrowser_paintAll(benchmark::State &state) {
    paint(state, Shown::All, static_cast<int>(state.range(0)));
}

// The add-on list holding as many rows as the small tree, with the browser off.
void FileBrowser_paintAddonList(benchmark::State &state) {
    paint(state, Shown::Off, 200, static_cast<int>(state.range(0)));
}

BENCHMARK(FileBrowser_paintOff)->Args({200, 1})->Args({200, 0})->ArgNames({"files", "full"});
BENCHMARK(FileBrowser_paintFolded)->ArgsProduct({{200, 3000}, {1, 0}})->ArgNames({"files", "full"});
BENCHMARK(FileBrowser_paintSome)->ArgsProduct({{200, 3000}, {1, 0}})->ArgNames({"files", "full"});
BENCHMARK(FileBrowser_paintAll)->ArgsProduct({{200, 3000}, {1, 0}})->ArgNames({"files", "full"});
BENCHMARK(FileBrowser_paintAddonList)->Args({200, 1})->Args({200, 0})->ArgNames({"addons", "full"});

// Runs the frames a click started to their end at 280 Hz, then the sync and frame of the fold it ends in. The
// counters give the frames and the mean of advance, arrange and paint per frame.
template <typename Where>
void slide(benchmark::State &state, bench::Rig &rig, const Where &where) {
    ttk::Root &root = rig.ui();
    std::size_t painted = 0;
    std::size_t frames = 0;
    Clock::duration framing{};

    for ([[maybe_unused]] const auto step : state) {
        const BLPoint spot = where();

        root.press(point(spot.x, spot.y));
        root.release(point(spot.x, spot.y));
        rig.sync();

        for (int frame = 0; frame < 120 && root.busy(); ++frame) {
            const Clock::time_point from = Clock::now();

            painted += rig.canvas().frameAt(rig.canvas().tick());
            framing += Clock::now() - from;
            ++frames;
        }

        rig.sync();
        painted += rig.canvas().frame();
        ++frames;
    }

    const auto count = static_cast<double>(frames);

    state.counters["frames"] = count / static_cast<double>(state.iterations());
    state.counters["pixels"] = frames == 0 ? 0.0 : static_cast<double>(painted) / count;
    state.counters["frame_us"] = frames == 0 ? 0.0 : std::chrono::duration<double, std::micro>(framing).count() / count;
}

// A click on the band header, which opens it and lists the folder, or shuts it.
void FileBrowser_slideBand(benchmark::State &state) {
    bench::Rig &rig = page(Shown::Some, static_cast<int>(state.range(0)));
    const components::BrowserBand &band = bandOf(rig);

    // The header moves to the bottom as the band shuts.
    slide(state, rig, [&band] { return BLPoint{band.box().x + 120.0, band.box().y + 15.0}; });
}

BENCHMARK(FileBrowser_slideBand)->Arg(200)->Arg(3000)->Unit(benchmark::kMicrosecond);

// A click on the first folder with everything below it open.
void FileBrowser_slideFolder(benchmark::State &state) {
    bench::Rig &rig = page(Shown::All, static_cast<int>(state.range(0)));

    const BLPoint spot{rowsOf(rig).box().x + 100.0, lineY(rig, 0)};

    slide(state, rig, [spot] { return spot; });
}

BENCHMARK(FileBrowser_slideFolder)->Arg(200)->Arg(3000)->Unit(benchmark::kMicrosecond);

// The pointer from one file row to the next and back.
void FileBrowser_hover(benchmark::State &state) {
    bench::Rig &rig = page(Shown::All, static_cast<int>(state.range(0)));
    ttk::Root &root = rig.ui();
    const double x = rowsOf(rig).box().x + 100.0;
    const int line = fileLine();
    std::size_t painted = 0;
    bool down = false;

    root.motion(x, lineY(rig, line));
    rig.canvas().frame();

    for ([[maybe_unused]] const auto step : state) {
        down = !down;

        root.motion(x, lineY(rig, line + (down ? 1 : 0)));
        painted += rig.canvas().frame();
    }

    state.counters["pixels"] = static_cast<double>(painted) / static_cast<double>(state.iterations());
}

BENCHMARK(FileBrowser_hover)->Arg(200)->Arg(3000);

// A click on a file row picks it, and the next takes it back: the pick, the pushes, the sync and the frame.
void FileBrowser_clickPick(benchmark::State &state) {
    bench::Rig &rig = page(Shown::All, static_cast<int>(state.range(0)));
    ttk::Root &root = rig.ui();
    const double x = rowsOf(rig).box().x + 100.0;
    const double y = lineY(rig, fileLine());
    std::size_t painted = 0;

    root.motion(x, y);
    rig.canvas().frame();

    for ([[maybe_unused]] const auto step : state) {
        root.press(point(x, y));
        root.release(point(x, y));
        rig.sync();

        painted += rig.canvas().frame();
    }

    state.counters["pixels"] = static_cast<double>(painted) / static_cast<double>(state.iterations());
}

BENCHMARK(FileBrowser_clickPick)->Arg(200)->Arg(3000)->Unit(benchmark::kMicrosecond);

// One character typed into the filter and taken back, each with its sync and frame.
void FileBrowser_typeFilter(benchmark::State &state) {
    bench::Rig &rig = page(Shown::Some, static_cast<int>(state.range(0)));
    ttk::Root &root = rig.ui();
    const auto *field = first<ttk::Field>(bandOf(rig));
    const BLRect box = field->box();
    std::size_t painted = 0;
    bool typed = false;

    root.press(point(box.x + 60.0, box.y + (box.h / 2.0)));
    root.release(point(box.x + 60.0, box.y + (box.h / 2.0)));
    rig.sync();
    rig.canvas().frame();

    for ([[maybe_unused]] const auto step : state) {
        typed = !typed;

        if (typed) {
            root.wrote("h");
        } else {
            root.key(BACKSPACE);
        }

        rig.sync();
        painted += rig.canvas().frame();
    }

    if (typed) {
        root.key(BACKSPACE);
    }

    root.focus(nullptr);

    state.counters["pixels"] = static_cast<double>(painted) / static_cast<double>(state.iterations());
}

BENCHMARK(FileBrowser_typeFilter)->Arg(200)->Arg(3000)->Unit(benchmark::kMicrosecond);

}
