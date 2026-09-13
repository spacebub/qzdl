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

#include <array>
#include <memory>
#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "core/util/Md5.h"
#include "core/wad/Artwork.h"
#include "core/wad/FileInfo.h"
#include "core/wad/MapFile.h"
#include "support/Corpus.h"
#include "support/Fixtures.h"

namespace {

constexpr std::array<std::string_view, 3> TITLE_NAMES = {"TITLEPIC", "TITLE", "INTERPIC"};

void MapFile_openWad(benchmark::State &state) {
    const std::filesystem::path &file = bench::Corpus::iwad();

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(MapFile::open(file));
    }
}

BENCHMARK(MapFile_openWad);

void MapFile_openPk3(benchmark::State &state) {
    const std::filesystem::path &file = bench::Corpus::pk3();

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(MapFile::open(file));
    }
}

BENCHMARK(MapFile_openPk3);

void Wad_mapNames(benchmark::State &state) {
    const std::unique_ptr<MapFile> wad = MapFile::open(bench::Corpus::iwad());

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(wad->mapNames());
    }
}

BENCHMARK(Wad_mapNames);

void Wad_lumpNames(benchmark::State &state) {
    const std::unique_ptr<MapFile> wad = MapFile::open(bench::Corpus::iwad());

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(wad->lumpNames());
    }
}

BENCHMARK(Wad_lumpNames);

void Wad_lump(benchmark::State &state) {
    const std::unique_ptr<MapFile> wad = MapFile::open(bench::Corpus::iwad());

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(wad->lump("PLAYPAL"));
    }
}

BENCHMARK(Wad_lump);

void Wad_picture(benchmark::State &state) {
    const std::unique_ptr<MapFile> wad = MapFile::open(bench::Corpus::iwad());

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(wad->picture(TITLE_NAMES));
    }
}

BENCHMARK(Wad_picture);

void Wad_isGame(benchmark::State &state) {
    const std::unique_ptr<MapFile> wad = MapFile::open(bench::Corpus::iwad());

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(wad->isGame());
    }
}

BENCHMARK(Wad_isGame);

void Pk3_mapNames(benchmark::State &state) {
    const std::unique_ptr<MapFile> pk3 = MapFile::open(bench::Corpus::pk3());

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(pk3->mapNames());
    }
}

BENCHMARK(Pk3_mapNames);

void Pk3_lumpNames(benchmark::State &state) {
    const std::unique_ptr<MapFile> pk3 = MapFile::open(bench::Corpus::pk3());

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(pk3->lumpNames());
    }
}

BENCHMARK(Pk3_lumpNames);

void Pk3_picture(benchmark::State &state) {
    const std::unique_ptr<MapFile> pk3 = MapFile::open(bench::Corpus::pk3());

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(pk3->picture(TITLE_NAMES));
    }
}

BENCHMARK(Pk3_picture);

// Cold: the warp list asks for a file it has not seen.
void MapFile_mapsCold(benchmark::State &state) {
    const std::vector<std::string> &files = bench::Corpus::addons(64);
    size_t at = 0;

    for ([[maybe_unused]] auto step : state) {
        const MapFile::Maps &found = MapFile::maps(files[at++ % files.size()]);

        benchmark::DoNotOptimize(found.names.size());
    }
}

BENCHMARK(MapFile_mapsCold);

void MapFile_mapsCached(benchmark::State &state) {
    const std::string file = bench::Corpus::iwad().string();

    for ([[maybe_unused]] auto step : state) {
        const MapFile::Maps &found = MapFile::maps(file);

        benchmark::DoNotOptimize(found.names.size());
    }
}

BENCHMARK(MapFile_mapsCached);

void Artwork_titleOfWad(benchmark::State &state) {
    const std::filesystem::path &file = bench::Corpus::iwad();

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Artwork::titleOf(file));
    }
}

BENCHMARK(Artwork_titleOfWad);

void Artwork_titleOfPk3(benchmark::State &state) {
    const std::filesystem::path &file = bench::Corpus::pk3();

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Artwork::titleOf(file));
    }
}

BENCHMARK(Artwork_titleOfPk3);

// The title screen a library card shows: patch plus palette to RGBA.
void Artwork_decodePatch(benchmark::State &state) {
    const Artwork::Title title = Artwork::titleOf(bench::Corpus::iwad());

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Artwork::decode(title));
    }

    state.SetItemsProcessed(state.iterations() * 320 * 200);
}

BENCHMARK(Artwork_decodePatch);

void Artwork_decodePng(benchmark::State &state) {
    const Artwork::Title title = Artwork::titleOf(bench::Corpus::pk3());

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Artwork::decode(title));
    }

    state.SetItemsProcessed(state.iterations() * 640 * 400);
}

BENCHMARK(Artwork_decodePng);

void Artwork_paletteOf(benchmark::State &state) {
    const std::filesystem::path &file = bench::Corpus::iwad();

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(Artwork::paletteOf(file));
    }
}

BENCHMARK(Artwork_paletteOf);

// Every file added to a list is described: MD5, then IWADINFO, then the name.
void FileInfo_describeIwad(benchmark::State &state) {
    const std::filesystem::path &file = bench::Corpus::iwad();

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(FileInfo::describeIwad(file));
    }
}

BENCHMARK(FileInfo_describeIwad);

void Md5_file(benchmark::State &state) {
    const std::filesystem::path &file = bench::Corpus::iwad();
    const auto size = static_cast<int64_t>(std::filesystem::file_size(file));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(md5File(file));
    }

    state.SetBytesProcessed(state.iterations() * size);
}

BENCHMARK(Md5_file);

void Md5_text(benchmark::State &state) {
    const std::string text = bench::Fixtures::paragraph(64);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(md5Text(text));
    }

    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(text.size()));
}

BENCHMARK(Md5_text);

}
