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
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Bytes.h"
#include "core/wad/Artwork.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;
namespace Bytes = tests::Bytes;

using Rgb = std::array<std::uint8_t, 3>;

Rgb at(const Artwork::Picture &picture, const int x, const int y) {
    const size_t from = ((static_cast<size_t>(y) * picture.width) + x) * 3;

    return {picture.pixels[from], picture.pixels[from + 1], picture.pixels[from + 2]};
}

::testing::AssertionResult near(const Rgb &got, const Rgb &wanted, const int slack = 2) {
    for (size_t channel = 0; channel < 3; ++channel) {
        if (std::abs(static_cast<int>(got[channel]) - static_cast<int>(wanted[channel])) > slack) {
            return ::testing::AssertionFailure()
                   << "got " << int(got[0]) << "," << int(got[1]) << "," << int(got[2]) << " wanted " << int(wanted[0])
                   << "," << int(wanted[1]) << "," << int(wanted[2]);
        }
    }

    return ::testing::AssertionSuccess();
}

Artwork::Title paletted(std::string lump) {
    return {.lump = std::move(lump), .palette = Bytes::playpal()};
}

Artwork::Title image(std::string lump) {
    return {.lump = std::move(lump), .palette = {}, .image = true};
}

// A patch of one column per post list given, each post (top, pixels).
std::string posts(const int height, const std::vector<std::vector<std::pair<int, std::string>>> &columns) {
    std::string offsets;
    std::string data;
    const size_t base = 8 + (columns.size() * 4);

    for (const auto &column : columns) {
        Bytes::put32(offsets, static_cast<std::uint32_t>(base + data.size()));

        for (const auto &[top, pixels] : column) {
            data.push_back(static_cast<char>(top));
            data.push_back(static_cast<char>(pixels.size()));
            data.push_back('\0');
            data += pixels;
            data.push_back('\0');
        }

        data.push_back(static_cast<char>(0xFF));
    }

    std::string out;

    Bytes::put16(out, static_cast<unsigned>(columns.size()));
    Bytes::put16(out, static_cast<unsigned>(height));
    Bytes::put16(out, 0);
    Bytes::put16(out, 0);

    return out + offsets + data;
}

class ArtworkTest : public ::testing::Test {
protected:
    fs::path directory = tests::Sandbox::scratch("artwork");

    fs::path file(const std::string &name, const std::string &bytes) {
        const fs::path at = directory / name;

        Bytes::write(at, bytes);

        return at;
    }

    // The IWAD and an add-on beside it, sharing its map lumps but carrying no palette.
    fs::path besideTheGame() {
        fs::copy_file(tests::Corpus::iwad(), directory / "game.wad");

        std::vector<Bytes::Lump> lumps = {{"TITLEPIC", Bytes::patch(40, 30)}};

        for (Bytes::Lump &lump : Bytes::maps({"MAP01", "MAP02"})) {
            lumps.push_back(std::move(lump));
        }

        return file("addon.wad", Bytes::wad("PWAD", lumps));
    }
};

TEST_F(ArtworkTest, TheTitleOfAnIwadIsItsTitlepicWithItsOwnPalette) {
    const Artwork::Title title = Artwork::titleOf(tests::Corpus::iwad());

    EXPECT_EQ(title.lump, Bytes::patch(320, 200));
    EXPECT_EQ(title.palette, Bytes::playpal());
    EXPECT_TRUE(title.own);
    EXPECT_FALSE(title.image);
    EXPECT_FALSE(title.empty());
}

TEST_F(ArtworkTest, TheTitleOfAPk3IsItsImageWithNoPalette) {
    const Artwork::Title title = Artwork::titleOf(tests::Corpus::pk3());

    EXPECT_TRUE(title.image);
    EXPECT_TRUE(title.palette.empty());
    EXPECT_FALSE(title.own);
    EXPECT_EQ(Artwork::suffixOf(title), ".png");
}

TEST_F(ArtworkTest, ATitlepicIsPreferredToALesserName) {
    const fs::path at =
            file("both.wad", Bytes::wad("PWAD", {{"CREDIT", Bytes::patch(8, 8)}, {"TITLEPIC", Bytes::patch(16, 16)}}));

    EXPECT_EQ(Artwork::titleOf(at, Bytes::playpal()).lump, Bytes::patch(16, 16));
}

TEST_F(ArtworkTest, UnderTitleALesserNameIsNoTitle) {
    const fs::path at = file("credit.wad", Bytes::wad("PWAD", {{"CREDIT", Bytes::patch(8, 8)}}));

    EXPECT_TRUE(Artwork::titleOf(at, Bytes::playpal(), Artwork::Under::Title).empty());
    EXPECT_EQ(Artwork::titleOf(at, Bytes::playpal(), Artwork::Under::Any).lump, Bytes::patch(8, 8));
}

TEST_F(ArtworkTest, UnderTitleTheTitleNameCounts) {
    const fs::path at = file("title.wad", Bytes::wad("PWAD", {{"TITLE", Bytes::patch(8, 8)}}));

    EXPECT_EQ(Artwork::titleOf(at, Bytes::playpal(), Artwork::Under::Title).lump, Bytes::patch(8, 8));
}

TEST_F(ArtworkTest, AFileWithNoPictureHasNoTitle) {
    const Artwork::Title title = Artwork::titleOf(tests::Corpus::pwad(1));

    EXPECT_TRUE(title.empty());
    EXPECT_TRUE(title.palette.empty());
}

TEST_F(ArtworkTest, AMissingFileHasNoTitle) {
    EXPECT_TRUE(Artwork::titleOf(directory / "absent.wad").empty());
}

TEST_F(ArtworkTest, AGivenPaletteIsUsedWhenTheFileHasNone) {
    const fs::path at = file("bare.wad", Bytes::wad("PWAD", {{"TITLEPIC", Bytes::patch(8, 8)}}));
    const std::string given(768, '\7');

    const Artwork::Title title = Artwork::titleOf(at, given);

    EXPECT_EQ(title.palette, given);
    EXPECT_FALSE(title.own);
}

TEST_F(ArtworkTest, TheFilesOwnPaletteBeatsAGivenOne) {
    const fs::path at =
            file("own.wad", Bytes::wad("PWAD", {{"PLAYPAL", Bytes::playpal()}, {"TITLEPIC", Bytes::patch(8, 8)}}));

    const Artwork::Title title = Artwork::titleOf(at, std::string(768, '\7'));

    EXPECT_EQ(title.palette, Bytes::playpal());
    EXPECT_TRUE(title.own);
}

TEST_F(ArtworkTest, AnImageLumpInAWadIsAnImage) {
    const fs::path at =
            file("png.wad", Bytes::wad("PWAD", {{"PLAYPAL", Bytes::playpal()}, {"TITLEPIC", Bytes::png(32, 20)}}));

    const Artwork::Title title = Artwork::titleOf(at);

    EXPECT_TRUE(title.image);
    EXPECT_TRUE(title.palette.empty());
}

TEST_F(ArtworkTest, ThePaletteIsBorrowedFromTheGameBesideIt) {
    const Artwork::Title title = Artwork::titleOf(besideTheGame());

    EXPECT_EQ(title.palette, Bytes::playpal());
    EXPECT_FALSE(title.own);
}

TEST_F(ArtworkTest, AGivenPaletteSkipsTheGameBesideIt) {
    const std::string given(768, '\7');

    EXPECT_EQ(Artwork::titleOf(besideTheGame(), given).palette, given);
}

TEST_F(ArtworkTest, AGameSharingTooFewLumpsLendsNoPalette) {
    fs::copy_file(tests::Corpus::iwad(), directory / "game.wad");

    const fs::path at = file("stranger.wad",
                             Bytes::wad("PWAD",
                                        {{"TITLEPIC", Bytes::patch(8, 8)},
                                         {"ONE", "1"},
                                         {"TWO", "2"},
                                         {"THREE", "3"},
                                         {"FOUR", "4"},
                                         {"THINGS", "5"}}));

    EXPECT_TRUE(Artwork::titleOf(at).palette.empty());
}

TEST_F(ArtworkTest, AnAddonBesideItLendsNoPalette) {
    file("other.wad",
         Bytes::wad("PWAD", {{"PLAYPAL", Bytes::playpal()}, {"TITLEPIC", Bytes::patch(8, 8)}, {"MAP01", ""}}));

    const fs::path at = file("bare.wad", Bytes::wad("PWAD", {{"TITLEPIC", Bytes::patch(8, 8)}, {"MAP01", ""}}));

    EXPECT_TRUE(Artwork::titleOf(at).palette.empty());
}

TEST_F(ArtworkTest, ThePaletteOfAFileIsItsPlaypal) {
    EXPECT_EQ(Artwork::paletteOf(tests::Corpus::iwad()), Bytes::playpal());
    EXPECT_EQ(Artwork::paletteOf(tests::Corpus::pk3()), Bytes::playpal());
}

TEST_F(ArtworkTest, AFileWithoutAPlaypalHasNoPalette) {
    EXPECT_EQ(Artwork::paletteOf(tests::Corpus::pwad(1)), "");
    EXPECT_EQ(Artwork::paletteOf(directory / "absent.wad"), "");
}

TEST_F(ArtworkTest, APatchIsDecodedThroughThePalette) {
    const Artwork::Picture picture = Artwork::decode(Artwork::titleOf(tests::Corpus::iwad()));

    ASSERT_EQ(picture.width, 320);
    ASSERT_EQ(picture.height, 200);
    ASSERT_EQ(picture.pixels.size(), 320U * 200U * 3U);

    EXPECT_EQ(at(picture, 0, 0), Bytes::colourOf(0));
    EXPECT_EQ(at(picture, 10, 20), Bytes::colourOf(30));
    EXPECT_EQ(at(picture, 319, 199), Bytes::colourOf((319 + 199) & 0xFF));
}

TEST_F(ArtworkTest, AFlatIsDecodedRowByRow) {
    std::string flat(64000, '\0');

    flat[1] = '\5';
    flat[320] = '\11';

    const Artwork::Picture picture = Artwork::decode(paletted(flat));

    ASSERT_EQ(picture.width, 320);
    ASSERT_EQ(picture.height, 200);
    EXPECT_EQ(at(picture, 1, 0), Bytes::colourOf(5));
    EXPECT_EQ(at(picture, 0, 1), Bytes::colourOf(9));
}

TEST_F(ArtworkTest, WhatNoPostCoversIsTheFirstColour) {
    const Artwork::Picture picture = Artwork::decode(paletted(posts(6, {{{2, "\x10\x11\x12"}}})));

    ASSERT_EQ(picture.width, 1);
    ASSERT_EQ(picture.height, 6);
    EXPECT_EQ(at(picture, 0, 0), Bytes::colourOf(0));
    EXPECT_EQ(at(picture, 0, 2), Bytes::colourOf(0x10));
    EXPECT_EQ(at(picture, 0, 4), Bytes::colourOf(0x12));
    EXPECT_EQ(at(picture, 0, 5), Bytes::colourOf(0));
}

TEST_F(ArtworkTest, SeveralPostsFillOneColumn) {
    const Artwork::Picture picture = Artwork::decode(paletted(posts(4, {{{0, "\x01"}, {3, "\x04"}}})));

    ASSERT_EQ(picture.height, 4);
    EXPECT_EQ(at(picture, 0, 0), Bytes::colourOf(1));
    EXPECT_EQ(at(picture, 0, 3), Bytes::colourOf(4));
}

TEST_F(ArtworkTest, APostPastTheBottomIsClipped) {
    const Artwork::Picture picture = Artwork::decode(paletted(posts(2, {{{1, "\x07\x08\x09"}}})));

    ASSERT_EQ(picture.height, 2);
    EXPECT_EQ(at(picture, 0, 1), Bytes::colourOf(7));
}

TEST_F(ArtworkTest, AColumnOffsetPastTheEndDecodesNothing) {
    std::string lump = posts(4, {{{0, "\x01"}}});

    lump[8] = '\x7F';

    EXPECT_TRUE(Artwork::decode(paletted(lump)).empty());
}

TEST_F(ArtworkTest, APostRunningPastTheEndDecodesNothing) {
    std::string lump = posts(4, {{{0, "\x01\x02"}}});

    lump.resize(lump.size() - 3);

    EXPECT_TRUE(Artwork::decode(paletted(lump)).empty());
}

TEST_F(ArtworkTest, AShortPaletteDecodesNothing) {
    Artwork::Title title = paletted(Bytes::patch(8, 8));

    title.palette.resize(767);

    EXPECT_TRUE(Artwork::decode(title).empty());
}

TEST_F(ArtworkTest, NoPaletteDecodesNothing) {
    EXPECT_TRUE(Artwork::decode({.lump = Bytes::patch(8, 8), .palette = {}}).empty());
}

TEST_F(ArtworkTest, AnEmptyTitleDecodesNothing) {
    EXPECT_TRUE(Artwork::decode({}).empty());
}

TEST_F(ArtworkTest, APngIsDecodedToItsOwnColours) {
    const Artwork::Picture picture = Artwork::decode(Artwork::titleOf(tests::Corpus::pk3()));

    ASSERT_EQ(picture.width, 640);
    ASSERT_EQ(picture.height, 400);
    ASSERT_EQ(picture.pixels.size(), 640U * 400U * 3U);
    EXPECT_TRUE(near(at(picture, 0, 0), {0x2A, 0x1A, 0x10}, 4));
}

TEST_F(ArtworkTest, AFlatColourPngKeepsItsColour) {
    const Artwork::Picture picture = Artwork::decode(image(Bytes::png(12, 6, 0xFF336699)));

    ASSERT_EQ(picture.width, 12);
    ASSERT_EQ(picture.height, 6);
    EXPECT_EQ(at(picture, 11, 5), (Rgb{0x33, 0x66, 0x99}));
}

TEST_F(ArtworkTest, ABrokenPngDecodesNothing) {
    EXPECT_TRUE(Artwork::decode(image("\x89PNG\r\n\x1A\nbroken")).empty());
}

TEST_F(ArtworkTest, ATruncatedPngDecodesNothing) {
    const std::string whole = Bytes::png(64, 64);

    EXPECT_TRUE(Artwork::decode(image(whole.substr(0, 40))).empty());
}

TEST_F(ArtworkTest, AnOversizedPngDecodesNothing) {
    EXPECT_TRUE(Artwork::decode(image(Bytes::png(5000, 2))).empty());
}

TEST_F(ArtworkTest, MeasureReadsAPatchHeader) {
    int width = -1;
    int height = -1;

    Artwork::measure(paletted(Bytes::patch(37, 21)), width, height);

    EXPECT_EQ(width, 37);
    EXPECT_EQ(height, 21);
}

TEST_F(ArtworkTest, MeasureReadsAFlatBySize) {
    int width = 0;
    int height = 0;

    Artwork::measure(paletted(std::string(64000, '\0')), width, height);

    EXPECT_EQ(width, 320);
    EXPECT_EQ(height, 200);
}

TEST_F(ArtworkTest, MeasureReadsAPng) {
    int width = 0;
    int height = 0;

    Artwork::measure(image(Bytes::png(48, 30)), width, height);

    EXPECT_EQ(width, 48);
    EXPECT_EQ(height, 30);
}

TEST_F(ArtworkTest, MeasureRefusesWhatIsNoPicture) {
    struct Case {
        const char *what;
        std::string lump;
    };

    std::string zeroWide = Bytes::patch(4, 4);
    std::string tooWide = Bytes::patch(4, 4);
    std::string negative = Bytes::patch(4, 4);

    zeroWide[0] = '\0';
    tooWide[1] = '\x20';
    negative[3] = '\x80';

    const std::vector<Case> cases = {
            {"empty", ""},
            {"short", "\x04\x00\x04"},
            {"zero wide", zeroWide},
            {"too wide", tooWide},
            {"negative height", negative},
            {"offsets cut short", Bytes::patch(40, 4).substr(0, 8 + 40)},
    };

    for (const Case &each : cases) {
        int width = -1;
        int height = -1;

        Artwork::measure(paletted(each.lump), width, height);

        EXPECT_EQ(width, 0) << each.what;
        EXPECT_EQ(height, 0) << each.what;
    }
}

TEST_F(ArtworkTest, MeasureRefusesAnOversizedPng) {
    int width = -1;
    int height = -1;

    Artwork::measure(image(Bytes::png(4097, 1)), width, height);

    EXPECT_EQ(width, 0);
    EXPECT_EQ(height, 0);
}

TEST_F(ArtworkTest, TheSuffixFollowsTheMagic) {
    EXPECT_EQ(Artwork::suffixOf(image(Bytes::png(2, 2))), ".png");
    EXPECT_EQ(Artwork::suffixOf(image("\xFF\xD8\xFF\xE0jfif")), ".jpg");
    EXPECT_EQ(Artwork::suffixOf(image("GIF89a")), ".gif");
    EXPECT_EQ(Artwork::suffixOf(paletted(Bytes::patch(2, 2))), "");
    EXPECT_EQ(Artwork::suffixOf({}), "");
}

TEST_F(ArtworkTest, TheSuffixesAreTheImageKinds) {
    const std::vector<std::string_view> suffixes(Artwork::suffixes().begin(), Artwork::suffixes().end());

    EXPECT_EQ(suffixes, (std::vector<std::string_view>{".png", ".jpg", ".gif"}));
}

}
