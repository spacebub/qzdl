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
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <benchmark/benchmark.h>

#include "ttk/system/Paths.h"
#include "ttk/toolkit/controls/Check.h"

#include "core/config/Session.h"
#include "core/launch/Dialect.h"
#include "core/launch/Flags.h"
#include "core/launch/Storage.h"
#include "gui/components/AddonList.h"
#include "gui/dialogs/FlagsDialog.h"
#include "gui/state/State.h"
#include "support/Canvas.h"
#include "support/Corpus.h"
#include "support/Fixtures.h"
#include "support/Rig.h"

namespace {

// Installed unconditionally. See the note in Library_bench.cpp.
bench::Rig &page(const int profiles, const int files, const bool grouped = false) {
    bench::Rig &rig = bench::shared();
    Config made = bench::Fixtures::config(profiles, files);

    rig.forget();

    bench::Fixtures::install(grouped ? bench::Fixtures::grouped(std::move(made)) : std::move(made));

    rig.config().reload();

    rig.go(State::Page::Profile);
    rig.sync();
    rig.ready();

    return rig;
}

template <typename Kind>
void gather(ttk::Widget *from, std::vector<Kind *> &out) {
    if (auto *hit = dynamic_cast<Kind *>(from); hit != nullptr) {
        out.push_back(hit);
    }

    for (const ttk::Widget::Ptr &child : from->children()) {
        gather(child.get(), out);
    }
}

void Profile_sync(benchmark::State &state) {
    const auto files = static_cast<int>(state.range(0));

    bench::Rig &rig = page(64, files);

    for ([[maybe_unused]] const auto step : state) {
        rig.sync();
    }

    state.SetItemsProcessed(state.iterations() * files);
}

BENCHMARK(Profile_sync)->Arg(0)->Arg(16)->Arg(512);

void Profile_syncGrouped(benchmark::State &state) {
    const auto files = static_cast<int>(state.range(0));

    bench::Rig &rig = page(64, files, true);

    for ([[maybe_unused]] const auto step : state) {
        rig.sync();
    }

    state.SetItemsProcessed(state.iterations() * files);
}

BENCHMARK(Profile_syncGrouped)->Arg(16)->Arg(512);

void Profile_fullPaint(benchmark::State &state) {
    const auto files = static_cast<int>(state.range(0));

    bench::Rig &rig = page(64, files);

    std::size_t painted = 0;

    for ([[maybe_unused]] const auto step : state) {
        painted = rig.canvas().full();
    }

    state.counters["pixels"] = static_cast<double>(painted);
}

// A wheel notch down the page and the frames it animates over. The profile page
// is a column of controls, not a grid of sprites, so it damages differently.
void Profile_scroll(benchmark::State &state) {
    const auto files = static_cast<int>(state.range(0));

    bench::Rig &rig = page(64, files);

    ttk::Root &root = rig.ui();

    std::size_t painted = 0;
    double steps = -1.0;

    for ([[maybe_unused]] const auto step : state) {
        root.wheel(steps, 600.0, 400.0);

        painted = 0;

        for (int frame = 0; frame < 8; ++frame) {
            painted += rig.canvas().frameAt(rig.canvas().tick());
        }

        steps = -steps;
    }

    state.counters["pixels"] = static_cast<double>(painted) / 8.0;
}

BENCHMARK(Profile_scroll)->Arg(16)->Arg(512);

BENCHMARK(Profile_fullPaint)->Arg(16)->Arg(512);

void Profile_scrollGrouped(benchmark::State &state) {
    bench::Rig &rig = page(64, static_cast<int>(state.range(0)), true);

    ttk::Root &root = rig.ui();

    std::size_t painted = 0;
    double steps = -1.0;

    for ([[maybe_unused]] const auto step : state) {
        root.wheel(steps, 600.0, 400.0);

        painted = 0;

        for (int frame = 0; frame < 8; ++frame) {
            painted += rig.canvas().frameAt(rig.canvas().tick());
        }

        steps = -steps;
    }

    state.counters["pixels"] = static_cast<double>(painted) / 8.0;
}

BENCHMARK(Profile_scrollGrouped)->Arg(16)->Arg(512);

void Profile_fullPaintGrouped(benchmark::State &state) {
    bench::Rig &rig = page(64, static_cast<int>(state.range(0)), true);

    std::size_t painted = 0;

    for ([[maybe_unused]] const auto step : state) {
        painted = rig.canvas().full();
    }

    state.counters["pixels"] = static_cast<double>(painted);
}

BENCHMARK(Profile_fullPaintGrouped)->Arg(16)->Arg(512);

// Picking another profile: the config, every panel and the whole page.
void Profile_switchProfile(benchmark::State &state) {
    bench::Rig &rig = page(64, 16);

    int at = 0;

    for ([[maybe_unused]] const auto step : state) {
        rig.config().profile().setProfileIndex(at++ % 64);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());

        state.PauseTiming();
        rig.still();
        state.ResumeTiming();
    }
}

BENCHMARK(Profile_switchProfile);

// A fold opening or closing, which relays out everything below it.
void Profile_toggleMultiplayer(benchmark::State &state) {
    bench::Rig &rig = page(64, 16);

    bool open = false;

    for ([[maybe_unused]] const auto step : state) {
        open = !open;

        rig.config().panels().setMultiplayerOpen(open);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());
    }
}

BENCHMARK(Profile_toggleMultiplayer);

// Typing into the extra arguments field, sync and frame per keystroke.
void Profile_typeExtra(benchmark::State &state) {
    const auto files = static_cast<int>(state.range(0));

    bench::Rig &rig = page(64, files);

    const std::string typed = "-fast -respawn -nomonsters";
    size_t at = 0;

    std::size_t painted = 0;

    for ([[maybe_unused]] const auto step : state) {
        rig.config().profile().setExtra(typed.substr(0, (at++ % typed.size()) + 1));
        rig.sync();

        painted += rig.canvas().frame();
    }

    state.counters["pixels"] = static_cast<double>(painted) / static_cast<double>(state.iterations());
}

BENCHMARK(Profile_typeExtra)->Arg(16)->Arg(512);

// The same on the port's own settings, where every edit asks the port where it keeps saves.
void Profile_typeExtraOnPortSettings(benchmark::State &state) {
    bench::Rig &rig = page(64, 16);
    const std::filesystem::path home = ttk::Paths::home_directory();
#ifdef _WIN32
    const std::filesystem::path ini = home / "Documents" / "My Games" / "GZDoom" / "gzdoom.ini";
#else
    const std::filesystem::path ini = home / ".config" / "gzdoom" / "gzdoom.ini";
#endif
    const std::filesystem::path saves = home / "doom-saves";

    std::filesystem::create_directories(ini.parent_path());
    std::ofstream(ini) << "[GlobalSettings]\nsave_dir=$HOME/doom-saves\n";
    bench::Corpus::saves(saves, 8);

    // The first profile is on a DOS port, which keeps no saves of its own.
    rig.config().profile().setProfileIndex(1);
    rig.config().profile().setSharedConfig(true);
    rig.config().panels().setSaveOpen(true);
    rig.sync();
    rig.ready();

    // Without the ini read, the port answers a default folder and the bench would time that.
    if (std::error_code code; !std::filesystem::equivalent(Storage::saveFolder(Session::get().config()), saves, code)) {
        state.SkipWithError("the port did not take its saves folder from the ini");
    }

    const std::string typed = "-fast -respawn -nomonsters";
    size_t at = 0;

    for ([[maybe_unused]] const auto step : state) {
        rig.config().profile().setExtra(typed.substr(0, (at++ % typed.size()) + 1));
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());
    }

    std::filesystem::remove(ini);
    std::filesystem::remove_all(saves);
}

BENCHMARK(Profile_typeExtraOnPortSettings);

void Profile_setSkill(benchmark::State &state) {
    bench::Rig &rig = page(64, 16);

    int at = 0;

    for ([[maybe_unused]] const auto step : state) {
        rig.config().profile().setSkill((at++ % 5) + 1);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());
    }
}

BENCHMARK(Profile_setSkill);

// The file list growing and emptying, which is the list view's worst case.
void Profile_addAndClearFiles(benchmark::State &state) {
    bench::Rig &rig = page(8, 0);

    const std::vector<std::string> &files = bench::Corpus::addons(static_cast<int>(state.range(0)));

    for ([[maybe_unused]] const auto step : state) {
        rig.config().lists().addFiles(files);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());

        rig.config().lists().clearFiles();
        rig.sync();
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK(Profile_addAndClearFiles)->Arg(16)->Arg(64);

void Profile_toggleFile(benchmark::State &state) {
    bench::Rig &rig = page(static_cast<int>(state.range(0)), 64);

    int at = 0;
    bool on = false;

    for ([[maybe_unused]] const auto step : state) {
        on = !on;

        rig.config().lists().setFileEnabled(at++ % 64, on);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());
    }
}

BENCHMARK(Profile_toggleFile)->Arg(64)->Arg(512);

void Profile_moveFile(benchmark::State &state) {
    bench::Rig &rig = page(static_cast<int>(state.range(0)), 64);

    int at = 0;

    for ([[maybe_unused]] const auto step : state) {
        const int from = at++ % 63;

        rig.config().lists().moveFile(from, from + 1);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());
    }
}

BENCHMARK(Profile_moveFile)->Arg(64)->Arg(512);

// What a carry onto a group lands as: the file joins the next group and the load order stays.
void Profile_setFileGroup(benchmark::State &state) {
    bench::Rig &rig = page(64, 64, true);

    const auto groups = static_cast<std::uint16_t>(Session::get().config().activeProfile().fileGroups.size());
    int at = 0;

    for ([[maybe_unused]] const auto step : state) {
        const int row = at++ % 64;
        const std::uint16_t group = Session::get().config().activeProfile().files[static_cast<size_t>(row)].group;

        rig.config().lists().setFileGroup(row, static_cast<std::uint16_t>((group % groups) + 1));
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());
    }
}

BENCHMARK(Profile_setFileGroup);

components::AddonList *addonList(bench::Rig &rig) {
    std::vector<components::AddonList *> lists;

    gather(&rig.profile(), lists);

    return lists.size() == 1 ? lists.front() : nullptr;
}

// A click on the first header and the slide it starts, run to its end at 280 Hz. Every other one opens the group.
void Profile_foldGroup(benchmark::State &state) {
    bench::Rig &rig = page(64, 64, true);
    const components::AddonList *list = addonList(rig);

    if (list == nullptr) {
        state.SkipWithError("the page has no add-on list");

        return;
    }

    ttk::Root &root = rig.ui();
    const double x = list->box().x + 120.0;
    const double y = list->box().y + 15.0;
    const Group &first = Session::get().config().activeProfile().fileGroups.front();
    const bool was = first.collapsed;
    std::size_t painted = 0;
    std::size_t frames = 0;

    root.motion(x, y);
    rig.canvas().frame();

    for ([[maybe_unused]] const auto step : state) {
        root.press(ttk::Pointer{.x = x, .y = y});
        root.release(ttk::Pointer{.x = x, .y = y});

        for (int frame = 0; frame < 120 && root.busy(); ++frame) {
            painted += rig.canvas().frameAt(rig.canvas().tick());
            ++frames;
        }

        rig.sync();
        painted += rig.canvas().frame();
        ++frames;
    }

    if (state.iterations() % 2 == 1 && first.collapsed == was) {
        state.SkipWithError("the click did not fold the group");
    }

    state.counters["frames"] = static_cast<double>(frames) / static_cast<double>(state.iterations());
    state.counters["pixels"] = frames == 0 ? 0.0 : static_cast<double>(painted) / static_cast<double>(frames);
}

BENCHMARK(Profile_foldGroup);

// Groups to Load order and back: every line and section of the list is rebuilt.
void Profile_switchAddonView(benchmark::State &state) {
    bench::Rig &rig = page(64, 64, true);

    bool order = false;

    for ([[maybe_unused]] const auto step : state) {
        order = !order;

        rig.config().lists().setAddonView(order ? State::AddonView::LoadOrder : State::AddonView::Groups);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());

        state.PauseTiming();
        rig.still();
        state.ResumeTiming();
    }
}

BENCHMARK(Profile_switchAddonView);

// A GZDoom profile, whose Gameplay section shows the most words and rules.
bench::Rig &gameplay(const bool open) {
    bench::Rig &rig = page(64, 16);

    rig.config().profile().setProfileIndex(1);
    rig.config().panels().setGameplayOpen(open);
    rig.sync();
    rig.ready();

    return rig;
}

void Profile_toggleGameplay(benchmark::State &state) {
    bench::Rig &rig = gameplay(false);

    bool open = false;

    for ([[maybe_unused]] const auto step : state) {
        open = !open;

        rig.config().panels().setGameplayOpen(open);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());

        state.PauseTiming();
        rig.still();
        state.ResumeTiming();
    }
}

BENCHMARK(Profile_toggleGameplay);

// Typing a word by hand, which parses it and rebuilds the command preview per keystroke.
void Profile_typeFlag(benchmark::State &state) {
    bench::Rig &rig = gameplay(true);

    const std::string typed = "2113540";
    size_t at = 0;

    for ([[maybe_unused]] const auto step : state) {
        rig.config().panels().setFlag(Flags::Word::dmflags, typed.substr(0, (at++ % typed.size()) + 1));
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());
    }
}

BENCHMARK(Profile_typeFlag);

void Profile_setRule(benchmark::State &state) {
    bench::Rig &rig = gameplay(true);

    bool on = false;

    for ([[maybe_unused]] const auto step : state) {
        on = !on;

        rig.config().panels().setOption(Dialect::Feature::alwaysApplyDmflags, on);
        rig.config().panels().setNumber(Dialect::Feature::turbo, on ? "150" : "");
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());
    }
}

BENCHMARK(Profile_setRule);

// Stepping through the compatibility modes, each of which fills both compatibility words.
void Profile_setCompatibility(benchmark::State &state) {
    bench::Rig &rig = gameplay(true);

    const int modes = static_cast<int>(State::get().cfg.compatibilityNames.size());
    int at = 0;

    if (modes < 2) {
        state.SkipWithError("the port offers no compatibility modes");
    }

    for ([[maybe_unused]] const auto step : state) {
        rig.config().panels().setCompatibility(at++ % modes);
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frame());
    }
}

BENCHMARK(Profile_setCompatibility);

std::unique_ptr<dialogs::FlagsDialog> flagsDialog(const Flags::Family family) {
    const Config &config = Session::get().config();

    return std::make_unique<dialogs::FlagsDialog>(
            family, Dialect::of(config), config.activeProfile().flags, [](const Flags::Edits &) {});
}

// The check box dialog built, laid out and painted over the page, then put away.
void Profile_flagsDialog(benchmark::State &state) {
    bench::Rig &rig = gameplay(true);
    const auto family = static_cast<Flags::Family>(state.range(0));

    for ([[maybe_unused]] const auto step : state) {
        rig.dialogs().show(flagsDialog(family));
        rig.sync();

        benchmark::DoNotOptimize(rig.canvas().frameAt(rig.canvas().tick()));

        rig.dialogs().dismiss();
        rig.sync();

        state.PauseTiming();
        rig.still();
        state.ResumeTiming();
    }
}

BENCHMARK(Profile_flagsDialog)
        ->Arg(static_cast<int>(Flags::Family::gameplay))
        ->Arg(static_cast<int>(Flags::Family::compatibility));

// A click on a box in the open dialog, which rewrites the values line under it.
void Profile_flagsDialogTick(benchmark::State &state) {
    bench::Rig &rig = gameplay(true);
    ttk::Dialog *dialog = rig.dialogs().show(flagsDialog(Flags::Family::gameplay));

    rig.sync();
    rig.ready();

    std::vector<ttk::Check *> checks;

    gather(dialog, checks);

    if (checks.empty()) {
        state.SkipWithError("the dialog has no check boxes");
    }

    ttk::Root &root = rig.ui();
    const BLRect box = checks.empty() ? BLRect{} : checks.front()->box();
    const double x = box.x + 8.0;
    const double y = box.y + (box.h / 2.0);

    root.motion(x, y);
    rig.canvas().frame();

    for ([[maybe_unused]] const auto step : state) {
        root.press(ttk::Pointer{.x = x, .y = y});
        root.release(ttk::Pointer{.x = x, .y = y});

        benchmark::DoNotOptimize(rig.canvas().frame());
    }

    rig.dialogs().dismiss();
    rig.sync();
    rig.ready();
}

BENCHMARK(Profile_flagsDialogTick);

}
