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

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "ttk/system/Paths.h"
#include "ttk/toolkit/controls/StatusIndicator.h"

#include "Drive.h"
#include "Steps.h"
#include "World.h"
#include "core/config/ConfigPaths.h"
#include "core/config/Schema.h"
#include "gui/draw/Hues.h"
#include "gui/services/IwadArt.h"
#include "support/Canvas.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

using Clock = std::chrono::steady_clock;

// A sitting: the config's shape, the window and the display it is on.
struct Sitting {
    const char *name;
    int profiles;
    int filesPerProfile;
    int width;
    int height;
    double hz;
    const char *theme;
};

// The first is the shape a launcher's config settles into: a dozen profiles over as
// many games and eight ports, a few add-ons each, on a 144 Hz display in the window
// the app opens with. Then a 60 Hz display, the window maximised on a 1440p display,
// the light theme, a collector's config and a first run.
constexpr std::array SITTINGS = std::to_array<Sitting>({
        {"typical", 12, 2, 1180, 760, 144.0, ThemeMode::DARK},
        {"typical-60hz", 12, 2, 1180, 760, 60.0, ThemeMode::DARK},
        {"maximised", 12, 3, 2560, 1400, 144.0, ThemeMode::DARK},
        {"light", 12, 1, 1180, 760, 144.0, ThemeMode::LIGHT},
        {"collector", 64, 16, 1600, 1000, 144.0, ThemeMode::DARK},
        {"first-run", 0, 0, 1180, 760, 144.0, ThemeMode::SYSTEM},
});

struct Row {
    std::string name;
    std::size_t frames = 0;
    std::size_t pixels = 0;
    double seconds = 0.0;
};

void print(const Row &row, const char *unit) {
    std::println("  {:<18} {:>8} {:<6} {:>9.1f} Mpx {:>8.0f} ms",
                 row.name,
                 row.frames,
                 unit,
                 static_cast<double>(row.pixels) / 1e6,
                 row.seconds * 1000.0);
}

bool wanted(const std::vector<std::string> &only, const std::string_view name) {
    return only.empty() || std::ranges::find(only, name) != only.end();
}

double since(const Clock::time_point began) {
    return std::chrono::duration<double>(Clock::now() - began).count();
}

void usage() {
    std::println(stderr,
                 "Usage: qzdl_pgo [--sitting NAME]... [--step NAME]... [--repeat N] [--list]\n"
                 "Drives the interface through a few sittings and the core through what\n"
                 "surrounds them, for an instrumented build to profile. Exits non-zero when\n"
                 "a sitting did not go as scripted.");
}

int run(const int argc, char *argv[]) {
    std::vector<std::string> sittings;
    std::vector<std::string> steps;
    int repeat = 1;

    for (int at = 1; at < argc; ++at) {
        const std::string_view argument = argv[at];
        const bool more = at + 1 < argc;

        if (argument == "--sitting" && more) {
            sittings.emplace_back(argv[++at]);
        } else if (argument == "--step" && more) {
            steps.emplace_back(argv[++at]);
        } else if (argument == "--repeat" && more) {
            repeat = std::max(1, static_cast<int>(std::strtol(argv[++at], nullptr, 10)));
        } else if (argument == "--list") {
            for (const Sitting &sitting : SITTINGS) {
                std::println("sitting {}", sitting.name);
            }

            for (const pgo::Step &step : pgo::steps()) {
                std::println("step {}", step.name);
            }

            return 0;
        } else {
            usage();

            return argument == "--help" ? 0 : 2;
        }
    }

    tests::Sandbox::enter();

    // What qzdl_gui.cpp and App set up before anything reads a path or paints.
    ttk::Paths::set_application("qzdl");
    ttk::Paths::set_executable(tests::Sandbox::root() / "bin" / "ZDL4");
    ttk::StatusIndicator::set_tones(&Hues::statusTones);
    IwadArt::prune();

    if (!tests::fontsLoaded()) {
        std::println(stderr, "qzdl_pgo: no system font could be loaded, and the interface needs one.");
        tests::Sandbox::leave();

        return 1;
    }

    const pgo::World world;
    std::vector<Row> totals;
    std::vector<std::string> failed;
    const Clock::time_point began = Clock::now();

    const auto add = [&totals](const std::string &name,
                               const std::size_t frames,
                               const std::size_t pixels,
                               const double seconds) {
        auto found = std::ranges::find(totals, name, &Row::name);

        if (found == totals.end()) {
            totals.push_back(Row{.name = name, .frames = 0, .pixels = 0, .seconds = 0.0});
            found = totals.end() - 1;
        }

        found->frames += frames;
        found->pixels += pixels;
        found->seconds += seconds;
    };

    for (int round = 0; round < repeat; ++round) {
        for (const Sitting &sitting : SITTINGS) {
            if (!wanted(sittings, sitting.name)) {
                continue;
            }

            // The config where a start looks for it, as the run before left it. A first
            // run finds none.
            const fs::path user = ConfigPaths::get().configPath(ConfigPaths::USER);
            std::error_code code;

            fs::remove(user, code);

            if (sitting.profiles > 0) {
                Config made = world.config(sitting.profiles, sitting.filesPerProfile);

                made.general.theme = sitting.theme;

                if (!made.save(user)) {
                    failed.push_back(std::string(sitting.name) + ": the config could not be written");

                    continue;
                }
            }

            std::println("{}: {} profiles, {}x{} at {:.0f} Hz",
                         sitting.name,
                         sitting.profiles,
                         sitting.width,
                         sitting.height,
                         sitting.hz);

            pgo::Drive drive(sitting.width, sitting.height, sitting.hz);

            for (const pgo::Step &step : pgo::steps()) {
                if (!wanted(steps, step.name) && std::string_view(step.name) != "startup") {
                    continue;
                }

                const pgo::Tally before = drive.tally;
                const Clock::time_point started = Clock::now();

                step.run(drive, world);

                const Row row{
                        .name = step.name,
                        .frames = drive.tally.frames - before.frames,
                        .pixels = drive.tally.pixels - before.pixels,
                        .seconds = since(started),
                };

                print(row, "frames");
                add(row.name, row.frames, row.pixels, row.seconds);
            }

            for (const std::string &what : drive.tally.failed) {
                failed.push_back(std::string(sitting.name) + ": " + what);
            }
        }
    }

    if (steps.empty() || wanted(steps, "core")) {
        pgo::Tally tally;
        const Clock::time_point started = Clock::now();

        pgo::core(world, tally);

        std::println("core");
        print(Row{.name = "core", .frames = tally.frames, .pixels = 0, .seconds = since(started)}, "items");

        for (const std::string &what : tally.failed) {
            failed.push_back("core: " + what);
        }
    }

    std::println("\ntotals");

    std::size_t frames = 0;
    std::size_t pixels = 0;

    for (const Row &row : totals) {
        print(row, "frames");

        frames += row.frames;
        pixels += row.pixels;
    }

    std::println("\n{} frames, {:.1f} Mpx, {:.1f} s", frames, static_cast<double>(pixels) / 1e6, since(began));

    if (!failed.empty()) {
        std::println(stderr, "\n{} checks failed:", failed.size());

        for (const std::string &what : failed) {
            std::println(stderr, "  {}", what);
        }
    }

    tests::Sandbox::leave();

    return failed.empty() ? 0 : 1;
}

}

int main(int argc, char *argv[]) {
    try {
        return run(argc, argv);
    } catch (const std::exception &trouble) {
        // NOLINTNEXTLINE(cert-err33-c,modernize-use-std-print): std::println can throw here of all places.
        std::fprintf(stderr, "qzdl_pgo stopped: %s\n", trouble.what());
    } catch (...) {
        // NOLINTNEXTLINE(cert-err33-c,modernize-use-std-print)
        std::fputs("qzdl_pgo stopped: something that gave no account of itself\n", stderr);
    }

    return 1;
}
