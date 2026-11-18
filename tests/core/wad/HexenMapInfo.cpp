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

#include <climits>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Bytes.h"
#include "core/wad/HexenMapInfo.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;
namespace Bytes = tests::Bytes;

using namespace std::string_literals;

using Names = std::vector<std::string>;
using Pair = std::pair<int, int>;
using Pairs = std::vector<Pair>;

std::optional<Pairs> parsed(const std::string &text) {
    const std::optional<std::vector<HexenMapInfo::Listed>> listed = HexenMapInfo::parse(text);

    if (!listed) {
        return std::nullopt;
    }

    Pairs out;

    for (const HexenMapInfo::Listed &each : *listed) {
        out.emplace_back(each.map, each.warpTrans);
    }

    return out;
}

using Warps = std::vector<std::pair<std::string, int>>;

Warps warps(const std::vector<fs::path> &files) {
    std::vector<std::string> names;

    for (const fs::path &file : files) {
        names.push_back(file.string());
    }

    const HexenMapInfo::Warps reached = HexenMapInfo::warps(names);
    Warps out;

    for (int map = 1; map <= HexenMapInfo::LAST_WARPED; ++map) {
        if (reached.reached.test(static_cast<size_t>(map))) {
            out.emplace_back(HexenMapInfo::lumpOf(map), reached.warpTrans.at(static_cast<size_t>(map)));
        }
    }

    return out;
}

fs::path wad(const fs::path &path, const char *kind, const std::vector<Bytes::Lump> &lumps) {
    Bytes::write(path, Bytes::wad(kind, lumps));

    return path;
}

// The maps, then a MAPINFO lump.
fs::path withMapInfo(const fs::path &path, const char *kind, const Names &maps, const std::string &mapinfo) {
    std::vector<Bytes::Lump> lumps = Bytes::maps(maps);

    lumps.push_back({.name = "MAPINFO", .bytes = mapinfo});

    return wad(path, kind, lumps);
}

class HexenMapInfoFiles : public ::testing::Test {
protected:
    fs::path folder = tests::Sandbox::scratch("hexen-mapinfo");
};

constexpr const char *HEXEN_EXCERPT = R"(
;
; MAPINFO.TXT  ; a second ; is still the comment
;

map 1 "WINNOWING HALL"
warptrans 1
next 2
cluster 1
sky1 SKY2 0
sky2 SKY2 0
lightning
cdtrack 13

MAP 2 "SEVEN PORTALS ; NOT A COMMENT"
WarpTrans 5    ; the second hub map
Next 3
CLUSTER 1
SKY1 SKY2 0
SKY2 SKY3 0
DOUBLESKY
CDTRACK 14

map 13 "SHADOW WOOD"
next 14
cluster 2
sky1 SKY3 0
fadetable FOGMAP
cd_start_track 2
cd_end1_track 3
cd_end2_track 4
cd_end3_track 5
cd_intermission_track 6
cd_title_track 8
)";

}

TEST(HexenMapInfo, AHexenExcerptListsEachMapWithItsWarptrans) {
    EXPECT_EQ(parsed(HEXEN_EXCERPT), (Pairs{{1, 1}, {2, 5}, {13, 13}}));
}

TEST(HexenMapInfo, ALaterEntryReplacesTheMapAndItsWarptrans) {
    EXPECT_EQ(parsed("map 4 \"First\" warptrans 9\nmap 4 \"Again\"\n"), (Pairs{{4, 4}}));
}

TEST(HexenMapInfo, NumbersAreReadAsStrtolReadsThem) {
    EXPECT_EQ(parsed("map 0x0A \"Hex\" warptrans 010\n"), (Pairs{{10, 8}}));
    EXPECT_EQ(parsed("map 1 \"Bad\" warptrans 08\n"), std::nullopt);
}

TEST(HexenMapInfo, ANegativeWarptransIsKeptAndANegativeMapStops) {
    EXPECT_EQ(parsed("map 1 \"Hall\" warptrans -3\n"), (Pairs{{1, -3}}));
    EXPECT_EQ(parsed("map -1 \"Hall\"\n"), std::nullopt);
}

// strtol saturates where long is as wide as int, and the wider long is cut to its low bits.
TEST(HexenMapInfo, AnOversizedNumberIsCutToIntAsThePortCutsIt) {
    constexpr bool narrow = sizeof(long) == sizeof(int);

    EXPECT_EQ(parsed("map 1 \"Hall\" warptrans 4294967301\n"), (Pairs{{1, narrow ? INT_MAX : 5}}));
    EXPECT_EQ(parsed("map 4294967298 \"Hall\"\n"), narrow ? std::nullopt : std::optional<Pairs>(Pairs{{2, 2}}));
}

TEST(HexenMapInfo, EmptyTextListsNoMaps) {
    EXPECT_EQ(parsed(""), Pairs{});
    EXPECT_EQ(parsed("; only a comment"), Pairs{});
}

TEST(HexenMapInfo, TextThePortStopsOnListsNothing) {
    for (const char *text : {
                 "map MAP01 \"Entryway\"\n{\n    levelnum = 1\n    next = \"MAP02\"\n}\n",
                 "defaultmap\n{\n}\nmap 1 \"Hall\"\n",
                 "map 1 \"Hall\" music D_RUNNIN\n",
                 "map 0 \"Zero\"\n",
                 "map 100 \"Past\"\n",
                 "map 1\n",
                 "map 1 \"Hall\" warptrans\n",
                 "map 1 \"Hall\" sky1 SKY1\n",
                 "map 1 \"Hall\" fadetable\n",
                 "map 1x \"Hall\"\n",
         }) {
        EXPECT_EQ(parsed(text), std::nullopt) << text;
    }
}

// The token stops after 63 letters, and the rest of it is the next token.
TEST(HexenMapInfo, ATokenOverSixtyThreeLettersIsCut) {
    EXPECT_EQ(parsed("map 1 \"Hall\" warptrans " + std::string(62, '0') + "5lightning\n"), (Pairs{{1, 5}}));
}

// A cut quoted token also drops the letter after the cut, and the rest is read outside the quotes.
TEST(HexenMapInfo, ACutQuotedTokenGoesOnOutsideTheQuotes) {
    EXPECT_EQ(parsed("map 1 \"" + std::string(63, 'n') + "X warptrans 6 \"lightning\"\n"), (Pairs{{1, 6}}));
}

TEST(HexenMapInfo, AnUnterminatedQuoteRunsToTheEnd) {
    EXPECT_EQ(parsed("map 1 \"Hall"), (Pairs{{1, 1}}));
    EXPECT_EQ(parsed("map 1 \"Hall\" warptrans \"5"), (Pairs{{1, 5}}));
}

TEST(HexenMapInfo, BytesFromOneHundredTwentyEightUpAreBlank) {
    EXPECT_EQ(parsed("map\xE9"
                     "1\xA0\"Hall\"\xFFwarptrans 3\x80\n"),
              (Pairs{{1, 3}}));
}

TEST(HexenMapInfo, ANulEndsAQuotedToken) {
    EXPECT_EQ(parsed("\"MAP\0x\" 2 \"Two\" \"warptrans\0junk\" \"4\0x\"\n"s), (Pairs{{2, 4}}));
}

TEST(HexenMapInfo, ACommentGluedToANumberEndsIt) {
    EXPECT_EQ(parsed("map 1 \"Hall\" warptrans 5;comment lightning\nlightning\n"), (Pairs{{1, 5}}));
}

TEST(HexenMapInfo, AQuotedKeywordIsTheKeyword) {
    EXPECT_EQ(parsed("\"map\" 2 \"Two\" \"WarpTrans\" 9 \"lightning\"\n"), (Pairs{{2, 9}}));
}

// strtol reads an empty token as a whole 0.
TEST(HexenMapInfo, AnEmptyQuotedNameIsANameAndAnEmptyNumberIsZero) {
    EXPECT_EQ(parsed("map 3 \"\" warptrans 4\n"), (Pairs{{3, 4}}));
    EXPECT_EQ(parsed("map 3 \"\" warptrans \"\"\n"), (Pairs{{3, 0}}));
}

TEST(HexenMapInfo, MapNumbersAreMapLumpsUpToNinetyEight) {
    EXPECT_EQ(HexenMapInfo::mapNumber("MAP01"), 1);
    EXPECT_EQ(HexenMapInfo::mapNumber("map98"), 98);

    for (const char *name : {"MAP99", "MAP00", "MAP1", "MAP001", "E1M1", "MAPXX", ""}) {
        EXPECT_EQ(HexenMapInfo::mapNumber(name), 0) << name;
    }

    EXPECT_EQ(HexenMapInfo::lumpOf(7), "MAP07");
    EXPECT_EQ(HexenMapInfo::lumpOf(42), "MAP42");
}

TEST_F(HexenMapInfoFiles, AWarpReachesEachListedMapWithALump) {
    const fs::path iwad = withMapInfo(folder / "excerpt.wad", "IWAD", {"MAP01", "MAP02"}, HEXEN_EXCERPT);
    const fs::path more = wad(folder / "more.wad", "PWAD", Bytes::maps({"MAP13"}));

    EXPECT_EQ(warps({iwad}), (Warps{{"MAP01", 1}, {"MAP02", 5}}));
    EXPECT_EQ(warps({iwad, more}), (Warps{{"MAP01", 1}, {"MAP02", 5}, {"MAP13", 13}}));
}

TEST_F(HexenMapInfoFiles, TheLastMapInfoReplacesTheEarlierOnesWhole) {
    const fs::path iwad = withMapInfo(folder / "game.wad", "IWAD", {"MAP01", "MAP02"}, HEXEN_EXCERPT);
    const fs::path patch = withMapInfo(folder / "patch.wad", "PWAD", {}, "map 2 \"Only\" warptrans 7\n");
    const fs::path plain = wad(folder / "plain.wad", "PWAD", Bytes::maps({"MAP01"}));

    EXPECT_EQ(warps({iwad, patch, plain}), (Warps{{"MAP02", 7}}));
    EXPECT_EQ(warps({patch, iwad}), (Warps{{"MAP01", 1}, {"MAP02", 5}}));
}

TEST_F(HexenMapInfoFiles, TheLastMapInfoInOneFileCounts) {
    std::vector<Bytes::Lump> lumps = Bytes::maps({"MAP01"});

    lumps.push_back({.name = "MAPINFO", .bytes = "map 1 \"First\" warptrans 3\n"});
    lumps.push_back({.name = "MAPINFO", .bytes = "map 1 \"Last\" warptrans 4\n"});

    EXPECT_EQ(warps({wad(folder / "both.wad", "IWAD", lumps)}), (Warps{{"MAP01", 4}}));
}

// W_AddFile loads a file whose name does not end in wad as one lump, named up to the first dot.
TEST_F(HexenMapInfoFiles, AFileNamedMapInfoIsOneMapInfoLump) {
    const fs::path iwad = withMapInfo(folder / "lumps.wad", "IWAD", {"MAP01", "MAP02"}, HEXEN_EXCERPT);
    const fs::path text = folder / "mapinfo.Script.txt";
    const fs::path bare = folder / "MAPINFO";
    const fs::path inside = withMapInfo(folder / "inside.dat", "PWAD", {"MAP03"}, "map 3 \"Three\"\n");
    const fs::path longer = folder / "MAPINFOS.txt";

    Bytes::write(text, "map 2 \"Only\" warptrans 7\n");
    Bytes::write(bare, "map 1 \"Hall\"\n");
    Bytes::write(longer, "map 1 \"Hall\"\n");

    EXPECT_EQ(warps({iwad, text}), (Warps{{"MAP02", 7}}));
    EXPECT_EQ(warps({iwad, bare}), (Warps{{"MAP01", 1}}));
    EXPECT_EQ(warps({iwad, inside}), (Warps{{"MAP01", 1}, {"MAP02", 5}}));
    EXPECT_EQ(warps({iwad, longer}), (Warps{{"MAP01", 1}, {"MAP02", 5}}));
    EXPECT_EQ(warps({iwad, folder / "MAPINFO.missing"}), (Warps{{"MAP01", 1}, {"MAP02", 5}}));
}

TEST_F(HexenMapInfoFiles, AMapInfoOverAMebibyteReachesNothing) {
    const std::string huge = "map 1 \"Hall\"\n" + std::string(std::size_t{1} << 20U, ' ');
    const fs::path iwad = withMapInfo(folder / "huge.wad", "IWAD", {"MAP01"}, huge);
    const fs::path text = folder / "MAPINFO.huge";

    Bytes::write(text, huge);

    EXPECT_EQ(warps({iwad}), Warps{});
    EXPECT_EQ(warps({withMapInfo(folder / "small.wad", "IWAD", {"MAP01"}, "map 1 \"Hall\"\n"), text}), Warps{});
}

TEST_F(HexenMapInfoFiles, ADuplicateWarptransReachesOnlyTheLowestMap) {
    const fs::path iwad = withMapInfo(folder / "twice.wad",
                                      "IWAD",
                                      {"MAP01", "MAP02", "MAP03"},
                                      "map 3 \"Third\" warptrans 2\nmap 2 \"Second\"\nmap 1 \"First\"\n");

    EXPECT_EQ(warps({iwad}), (Warps{{"MAP01", 1}, {"MAP02", 2}}));
}

// A map MAPINFO leaves out has warptrans 0, so -warp 0 goes to the lowest of those.
TEST_F(HexenMapInfoFiles, WarptransZeroLosesToALowerUnlistedMap) {
    const fs::path iwad =
            withMapInfo(folder / "zero.wad", "IWAD", {"MAP01", "MAP02", "MAP03"}, "map 3 \"Third\" warptrans 0\n");
    const fs::path full = withMapInfo(
            folder / "zero-full.wad", "IWAD", {"MAP01", "MAP02"}, "map 1 \"First\"\nmap 2 \"Second\" warptrans 0\n");

    EXPECT_EQ(warps({iwad}), Warps{});
    EXPECT_EQ(warps({full}), (Warps{{"MAP01", 1}, {"MAP02", 0}}));
}

TEST_F(HexenMapInfoFiles, MapNinetyNineIsNeverReached) {
    const fs::path iwad = withMapInfo(
            folder / "last.wad", "IWAD", {"MAP98", "MAP99"}, "map 98 \"Edge\"\nmap 99 \"Past\" warptrans 1\n");

    EXPECT_EQ(warps({iwad}), (Warps{{"MAP98", 98}}));
}

TEST_F(HexenMapInfoFiles, AMapInfoThePortStopsOnReachesNothing) {
    const fs::path iwad = withMapInfo(folder / "good.wad", "IWAD", {"MAP01", "MAP02"}, HEXEN_EXCERPT);
    const fs::path zdoom =
            withMapInfo(folder / "zdoom.wad", "PWAD", {"MAP01"}, "map MAP01 \"Entryway\"\n{\n    levelnum = 1\n}\n");
    const fs::path empty = withMapInfo(folder / "empty.wad", "PWAD", {}, "");

    EXPECT_EQ(warps({iwad, zdoom}), Warps{});
    EXPECT_EQ(warps({iwad, empty}), Warps{});
}

TEST_F(HexenMapInfoFiles, FilesWithoutMapInfoReachNothing) {
    const fs::path plain = wad(folder / "nomapinfo.wad", "IWAD", Bytes::maps({"MAP01"}));

    EXPECT_EQ(warps({plain}), Warps{});
    EXPECT_EQ(warps({folder / "missing.wad"}), Warps{});
    EXPECT_EQ(warps({}), Warps{});
}

TEST_F(HexenMapInfoFiles, ARewrittenFileIsReadAgain) {
    const fs::path iwad = withMapInfo(folder / "rewritten.wad", "IWAD", {"MAP01"}, "map 1 \"Hall\"\n");

    EXPECT_EQ(warps({iwad}), (Warps{{"MAP01", 1}}));

    withMapInfo(iwad, "IWAD", {"MAP01"}, "map 1 \"Hall\" warptrans 12\n");

    EXPECT_EQ(warps({iwad}), (Warps{{"MAP01", 12}}));
}
