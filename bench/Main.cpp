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

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#include "core/system/Paths.h"
#include "support/Canvas.h"
#include "support/Sandbox.h"

int main(int argc, char **argv) {
    bench::Sandbox::enter();

    if (argc > 0) {
        Paths::setExecutable(argv[0]);
    }

    if (!bench::fontsLoaded()) {
        std::fputs("No system face could be loaded; the interface benchmarks need one.\n", stderr);

        return 1;
    }

    // The caches these share, such as shaped runs, blurred sprites and decoded art,
    // are warm in a running interface and cold in the first iteration here. Warming
    // them first is what makes a number the same alone as it is in the whole run.
    std::vector<char *> args(argv, argv + argc);
    bool warmup = false;

    for (const char *arg : args) {
        warmup = warmup || std::strstr(arg, "--benchmark_min_warmup_time") != nullptr;
    }

    std::string held = "--benchmark_min_warmup_time=0.3";

    if (!warmup) {
        args.push_back(held.data());
    }

    argc = static_cast<int>(args.size());
    argv = args.data();

    benchmark::Initialize(&argc, argv);

    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }

    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();

    bench::Sandbox::leave();

    return 0;
}
