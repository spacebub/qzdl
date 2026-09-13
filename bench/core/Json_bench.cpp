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

#include <utility>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <benchmark/benchmark.h>

#include "core/util/Json.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

const std::string &text(const int profiles, const int files) {
    static std::string held;
    static int lastProfiles = -1;
    static int lastFiles = -1;

    if (profiles != lastProfiles || files != lastFiles) {
        const std::filesystem::path &path = bench::Corpus::json(profiles, files);

        std::ifstream in(path, std::ios::binary);

        held.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

        lastProfiles = profiles;
        lastFiles = files;
    }

    return held;
}

void Json_readFile(benchmark::State &state) {
    const auto profiles = static_cast<int>(state.range(0));
    const std::filesystem::path &path = bench::Corpus::json(profiles, 16);

    for ([[maybe_unused]] auto step : state) {
        Json::Doc doc = Json::readFile(path);

        if (!doc.valid()) {
            state.SkipWithError("the file did not parse");

            break;
        }

        benchmark::DoNotOptimize(doc.root());
    }

    state.SetItemsProcessed(state.iterations() * profiles);
}

BENCHMARK(Json_readFile)->Arg(8)->Arg(64)->Arg(512);

void Json_readData(benchmark::State &state) {
    const std::string &data = text(64, 16);

    for ([[maybe_unused]] auto step : state) {
        Json::Doc doc = Json::readData(data);

        benchmark::DoNotOptimize(doc.root());
    }

    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(data.size()));
}

BENCHMARK(Json_readData);

void Json_objGetString(benchmark::State &state) {
    const std::string &data = text(64, 16);
    const Json::Doc doc = Json::readData(data);

    yyjson_val *root = doc.root();

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Json::objGetString(root, "activeProfileId"));
    }
}

BENCHMARK(Json_objGetString);

void Json_buildAndWrite(benchmark::State &state) {
    const auto rows = static_cast<int>(state.range(0));
    const std::filesystem::path at = bench::Sandbox::scratch("json") / "out.json";

    for ([[maybe_unused]] auto step : state) {
        const Json::Builder builder;

        yyjson_mut_val *root = builder.newObject();
        yyjson_mut_val *list = builder.newArray();

        for (int item = 0; item < rows; ++item) {
            yyjson_mut_val *entry = builder.newObject();

            builder.addString(entry, "file", "/addons/addon.wad");
            builder.addInt(entry, "index", item);
            builder.addBool(entry, "enabled", item % 3 != 0);

            Json::Builder::appendValue(list, entry);
        }

        builder.addValue(root, "files", list);
        builder.setRoot(root);

        benchmark::DoNotOptimize(builder.writeFile(at));
    }

    state.SetItemsProcessed(state.iterations() * rows);
}

BENCHMARK(Json_buildAndWrite)->Arg(16)->Arg(256);

}
