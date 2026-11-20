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

#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Bytes.h"
#include "core/wad/Readmes.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;
namespace Bytes = tests::Bytes;

std::vector<std::string> names(const std::vector<Readmes::File> &texts) {
    std::vector<std::string> out;

    for (const Readmes::File &text : texts) {
        out.push_back(text.name);
    }

    return out;
}

class ReadmesTest : public ::testing::Test {
protected:
    fs::path directory = tests::Sandbox::scratch("readmes");

    fs::path file(const std::string &name, const std::string &bytes = "text") {
        const fs::path at = directory / name;

        Bytes::write(at, bytes);

        return at;
    }
};

TEST_F(ReadmesTest, TakesTheTextBesideAWadWithTheSameStem) {
    const fs::path wad = file("mod.wad", Bytes::wad("PWAD", {}));

    file("MOD.TXT", "about the mod");
    file("other.txt");
    file("mod.deh");

    const std::vector<Readmes::File> found = Readmes::of(wad);

    ASSERT_EQ(names(found), std::vector<std::string>{"MOD.TXT"});
    EXPECT_FALSE(found[0].inside);
    EXPECT_EQ(found[0].body, "about the mod");
}

TEST_F(ReadmesTest, TakesTopTextsAndReadmesOfAPk3ButNotEngineLumps) {
    const fs::path pk3 = file("mod.pk3",
                              Bytes::zip({
                                      {.name = "credits.txt", .bytes = "people"},
                                      {.name = "MAPINFO.txt", .bytes = "map MAP01"},
                                      {.name = "zscript.txt", .bytes = "version"},
                                      {.name = "gldefs.bm.txt", .bytes = "brightmap"},
                                      {.name = "actors/monster.txt", .bytes = "actor"},
                                      {.name = "docs/README.md", .bytes = "# Mod"},
                                      {.name = "zscript.zs", .bytes = "class"},
                              }));

    file("mod.txt", "beside");

    const std::vector<Readmes::File> found = Readmes::of(pk3);

    EXPECT_EQ(names(found), (std::vector<std::string>{"mod.txt", "credits.txt", "docs/README.md"}));
    EXPECT_TRUE(found[1].inside);
    EXPECT_EQ(found[2].body, "# Mod");
}

TEST_F(ReadmesTest, TakesTheTopTextsOfAFolder) {
    file("pack/readme");
    file("pack/notes.txt");
    file("pack/decorate.txt");
    file("pack/maps.wad");
    file("pack/docs/deep.txt");

    EXPECT_EQ(names(Readmes::of(directory / "pack")), (std::vector<std::string>{"notes.txt", "readme"}));
}

TEST_F(ReadmesTest, FindsNothingForAMissingFile) {
    EXPECT_TRUE(Readmes::of(directory / "gone.wad").empty());
}

TEST_F(ReadmesTest, KeepsUtf8AndEvensOutLinesAndTabs) {
    EXPECT_EQ(Readmes::readable("\xEF\xBB\xBFgr\xC3\xBC\xC3\x9F\r\nab\tc\rd\x1A"), "grüß\nab      c\nd");
}

TEST_F(ReadmesTest, ReadsBoxDrawingAsCp437) {
    EXPECT_EQ(Readmes::readable("\xC9\xCD\xBB\n\xC8\xCD\xBC caf\x82"), "╔═╗\n╚═╝ café");
}

TEST_F(ReadmesTest, ReadsOtherBytesAsWindows1252) {
    EXPECT_EQ(Readmes::readable("it\x92s \x93"
                                "caf\xE9\x94"),
              "it’s “café”");
}

}
