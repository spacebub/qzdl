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
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "core/util/Ini.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

using Pairs = std::vector<std::pair<std::string, std::string>>;

constexpr int MANY = 10000;

std::string slurp(const fs::path &path) {
    std::ifstream file(path, std::ios::binary);

    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void spill(const fs::path &path, const std::string_view text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);

    file.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::vector<std::string> keysOf(const std::vector<const std::pair<std::string, std::string> *> &found) {
    std::vector<std::string> out;

    out.reserve(found.size());

    for (const auto *entry : found) {
        out.push_back(entry->first);
    }

    return out;
}

class IniTest : public ::testing::Test {
protected:
    void SetUp() override { _dir = tests::Sandbox::scratch("ini"); }

    [[nodiscard]] const fs::path &dir() const { return _dir; }

    Ini parse(const std::string_view text) {
        const fs::path path = _dir / "in.ini";

        spill(path, text);

        Ini ini;

        EXPECT_TRUE(ini.read(path));

        return ini;
    }

private:
    fs::path _dir;
};

}

TEST_F(IniTest, ReadsSectionsAndTheirKeys) {
    const Ini ini = parse("[zdl.general]\nalwaysadd=-nomusic\n[zdl.save]\nskill=3\nwarp=MAP07\n");

    ASSERT_NE(ini.section("zdl.general"), nullptr);
    ASSERT_NE(ini.section("zdl.save"), nullptr);
    EXPECT_EQ(ini.section("zdl.general")->get("alwaysadd"), "-nomusic");
    EXPECT_EQ(ini.section("zdl.save")->values, (Pairs{{"skill", "3"}, {"warp", "MAP07"}}));
}

TEST_F(IniTest, AMissingFileFailsToRead) {
    Ini ini;

    EXPECT_FALSE(ini.read(dir() / "absent.ini"));
    EXPECT_EQ(ini.section("anything"), nullptr);
}

TEST_F(IniTest, ADirectoryFailsToRead) {
    Ini ini;

    EXPECT_FALSE(ini.read(dir()));
}

TEST_F(IniTest, AnEmptyFileReadsAsNoSections) {
    const Ini ini = parse("");

    EXPECT_EQ(ini.section(""), nullptr);
}

TEST_F(IniTest, WhitespaceAroundNamesKeysAndValuesIsTrimmed) {
    const Ini ini = parse("  [ zdl.save ]  \n\t key \t=  some value  \n");

    ASSERT_NE(ini.section("zdl.save"), nullptr);
    EXPECT_EQ(ini.section("zdl.save")->values, (Pairs{{"key", "some value"}}));
}

TEST_F(IniTest, WindowsLineEndingsAreTrimmed) {
    const Ini ini = parse("[s]\r\nkey=value\r\nother=two\r\n");

    ASSERT_NE(ini.section("s"), nullptr);
    EXPECT_EQ(ini.section("s")->values, (Pairs{{"key", "value"}, {"other", "two"}}));
}

TEST_F(IniTest, ALastLineWithoutANewlineIsRead) {
    const Ini ini = parse("[s]\nkey=value");

    ASSERT_NE(ini.section("s"), nullptr);
    EXPECT_EQ(ini.section("s")->get("key"), "value");
}

TEST_F(IniTest, HashAndSemicolonLinesAreComments) {
    const Ini ini = parse("# top\n[s]\n; one\n   # indented\n\t;tabbed\nkey=value\n");

    ASSERT_NE(ini.section("s"), nullptr);
    EXPECT_EQ(ini.section("s")->values, (Pairs{{"key", "value"}}));
}

TEST_F(IniTest, ACommentMarkAfterAValueStaysInTheValue) {
    const Ini ini = parse("[s]\nkey=a ; b # c\n");

    EXPECT_EQ(ini.section("s")->get("key"), "a ; b # c");
}

TEST_F(IniTest, BlankLinesAreSkipped) {
    const Ini ini = parse("\n\n[s]\n\n   \nkey=value\n\n");

    EXPECT_EQ(ini.section("s")->values, (Pairs{{"key", "value"}}));
}

TEST_F(IniTest, KeysBeforeAnySectionAreDropped) {
    const Ini ini = parse("orphan=1\n[s]\nkey=value\n");

    ASSERT_NE(ini.section("s"), nullptr);
    EXPECT_FALSE(ini.section("s")->has("orphan"));
    EXPECT_EQ(ini.section(""), nullptr);
}

TEST_F(IniTest, ALineWithoutAnEqualsSignIsSkipped) {
    const Ini ini = parse("[s]\njust words\nkey=value\n");

    EXPECT_EQ(ini.section("s")->values, (Pairs{{"key", "value"}}));
}

TEST_F(IniTest, AValueKeepsEveryEqualsSignAfterTheFirst) {
    const Ini ini = parse("[s]\nextra=+set a=b -x=1\n");

    EXPECT_EQ(ini.section("s")->get("extra"), "+set a=b -x=1");
}

TEST_F(IniTest, AnEmptyValueIsStillAKey) {
    const Ini ini = parse("[s]\nkey=\n");

    EXPECT_TRUE(ini.section("s")->has("key"));
    EXPECT_EQ(ini.section("s")->get("key"), "");
}

TEST_F(IniTest, ARepeatedKeyKeepsTheLastValueInTheFirstPlace) {
    const Ini ini = parse("[s]\nkey=one\nother=x\nkey=two\n");

    EXPECT_EQ(ini.section("s")->values, (Pairs{{"key", "two"}, {"other", "x"}}));
}

TEST_F(IniTest, KeysCompareWithoutCaseAndKeepTheirFirstSpelling) {
    const Ini ini = parse("[s]\nWarp=MAP01\nWARP=MAP02\n");

    EXPECT_EQ(ini.section("s")->values, (Pairs{{"Warp", "MAP02"}}));
    EXPECT_EQ(ini.section("s")->get("warp"), "MAP02");
    EXPECT_TRUE(ini.section("s")->has("wArP"));
}

TEST_F(IniTest, ARepeatedSectionMergesIntoTheFirst) {
    const Ini ini = parse("[s]\na=1\n[t]\nb=2\n[S]\nc=3\na=4\n");

    ASSERT_NE(ini.section("s"), nullptr);
    EXPECT_EQ(ini.section("s")->name, "s");
    EXPECT_EQ(ini.section("s")->values, (Pairs{{"a", "4"}, {"c", "3"}}));
}

TEST_F(IniTest, SectionNamesCompareWithoutCase) {
    const Ini ini = parse("[Zdl.Save]\nskill=2\n");

    ASSERT_NE(ini.section("zdl.save"), nullptr);
    EXPECT_EQ(ini.section("ZDL.SAVE")->get("skill"), "2");
}

TEST_F(IniTest, AnUnclosedHeaderLeavesKeysInThePreviousSection) {
    const Ini ini = parse("[s]\na=1\n[broken\nb=2\n");

    EXPECT_EQ(ini.section("broken"), nullptr);
    EXPECT_EQ(ini.section("[broken"), nullptr);
    EXPECT_EQ(ini.section("s")->values, (Pairs{{"a", "1"}, {"b", "2"}}));
}

TEST_F(IniTest, TextAfterTheClosingBracketIsIgnored) {
    const Ini ini = parse("[s] trailing words\nkey=value\n");

    ASSERT_NE(ini.section("s"), nullptr);
    EXPECT_EQ(ini.section("s")->get("key"), "value");
}

TEST_F(IniTest, AnEmptyHeaderIsASectionWithNoName) {
    const Ini ini = parse("[]\nkey=value\n");

    ASSERT_NE(ini.section(""), nullptr);
    EXPECT_EQ(ini.section("")->get("key"), "value");
}

TEST_F(IniTest, Utf8AndWindowsPathsAreKeptByteForByte) {
    const Ini ini = parse("[s]\nname=\xC3\xA9"
                          "dition \xE6\x97\xA5\nfile=C:\\Games\\Doom\\doom2.wad\n");

    EXPECT_EQ(ini.section("s")->get("name"),
              "\xC3\xA9"
              "dition \xE6\x97\xA5");
    EXPECT_EQ(ini.section("s")->get("file"), "C:\\Games\\Doom\\doom2.wad");
}

TEST_F(IniTest, AByteOrderMarkDoesNotHideTheFirstSection) {
    const Ini ini = parse("\xEF\xBB\xBF[zdl.save]\nskill=3\n");

    ASSERT_NE(ini.section("zdl.save"), nullptr);
    EXPECT_EQ(ini.section("zdl.save")->get("skill"), "3");
}

TEST_F(IniTest, ManyKeysAreAllRead) {
    std::string text = "[big]\n";

    for (int at = 0; at < MANY; ++at) {
        text += "k" + std::to_string(at) + "=v" + std::to_string(at) + "\n";
    }

    const Ini ini = parse(text);

    ASSERT_NE(ini.section("big"), nullptr);
    EXPECT_EQ(ini.section("big")->values.size(), static_cast<size_t>(MANY));
    EXPECT_EQ(ini.section("big")->get("k9999"), "v9999");
}

TEST_F(IniTest, TheLegacyCorpusReadsEverySection) {
    Ini ini;

    ASSERT_TRUE(ini.read(tests::Corpus::ini()));

    for (const char *name : {"zdl.general", "zdl.save", "zdl.iwads", "zdl.ports"}) {
        ASSERT_NE(ini.section(name), nullptr) << name;
    }

    EXPECT_EQ(ini.section("zdl.iwads")->values.size(), 24U);
    EXPECT_EQ(ini.section("zdl.save")->get("file63"), "/addons/addon63.wad");
    EXPECT_EQ(ini.section("zdl.save")->get("file4d"), "/addons/addon4.wad");
}

TEST(IniSection, AMissingKeyReadsAsEmpty) {
    const Ini::Section section{.name = "s", .values = {{"a", "1"}}};

    EXPECT_FALSE(section.has("b"));
    EXPECT_EQ(section.get("b"), "");
}

TEST(IniSection, SetReplacesAKeyWhateverItsCase) {
    Ini::Section section{.name = "s", .values = {{"Key", "1"}, {"other", "2"}}};

    section.set("KEY", "3");

    EXPECT_EQ(section.values, (Pairs{{"Key", "3"}, {"other", "2"}}));
}

TEST(IniSection, SetAppendsANewKey) {
    Ini::Section section{.name = "s", .values = {{"a", "1"}}};

    section.set("b", "2");

    EXPECT_EQ(section.values, (Pairs{{"a", "1"}, {"b", "2"}}));
}

TEST(IniSection, StartingWithMatchesThePrefixWithoutCaseInOrder) {
    const Ini::Section section{.name = "s",
                               .values = {{"file0", "a"}, {"skill", "3"}, {"FILE1d", "b"}, {"File10", "c"}}};

    EXPECT_EQ(keysOf(section.startingWith("file")), (std::vector<std::string>{"file0", "FILE1d", "File10"}));
}

TEST(IniSection, StartingWithLeavesOutTheBarePrefix) {
    const Ini::Section section{.name = "s", .values = {{"file", "bare"}, {"file0", "a"}}};

    EXPECT_EQ(keysOf(section.startingWith("file")), (std::vector<std::string>{"file0"}));
}

TEST(IniSection, StartingWithAnEmptyPrefixMatchesEveryNamedKey) {
    const Ini::Section section{.name = "s", .values = {{"a", "1"}, {"", "2"}, {"b", "3"}}};

    EXPECT_EQ(keysOf(section.startingWith("")), (std::vector<std::string>{"a", "b"}));
}

TEST(IniEnsure, ReturnsTheSectionThereWhateverItsCase) {
    Ini ini;

    ini.ensure("zdl.save").set("skill", "4");

    Ini::Section &again = ini.ensure("ZDL.SAVE");

    EXPECT_EQ(again.name, "zdl.save");
    EXPECT_EQ(again.get("skill"), "4");
}

TEST(IniEnsure, AddsAMissingSectionEmpty) {
    Ini ini;

    const Ini::Section &made = ini.ensure("new");

    EXPECT_EQ(made.name, "new");
    EXPECT_TRUE(made.values.empty());
    EXPECT_EQ(ini.section("new"), &made);
}

TEST_F(IniTest, WriteSpellsEverySectionInOrder) {
    Ini ini;

    ini.ensure("b").set("x", "1");
    ini.ensure("a").set("y", "2");
    ini.ensure("a").set("z", "");

    ASSERT_TRUE(ini.write(dir() / "out.ini"));

    EXPECT_EQ(slurp(dir() / "out.ini"), "[b]\nx=1\n\n[a]\ny=2\nz=\n\n");
}

TEST_F(IniTest, WritingNothingLeavesAnEmptyFile) {
    const Ini ini;

    ASSERT_TRUE(ini.write(dir() / "empty.ini"));

    EXPECT_TRUE(fs::is_regular_file(dir() / "empty.ini"));
    EXPECT_EQ(slurp(dir() / "empty.ini"), "");
}

TEST_F(IniTest, WriteCreatesMissingDirectories) {
    Ini ini;

    ini.ensure("s").set("k", "v");

    ASSERT_TRUE(ini.write(dir() / "deep" / "er" / "out.ini"));

    EXPECT_EQ(slurp(dir() / "deep" / "er" / "out.ini"), "[s]\nk=v\n\n");
}

TEST_F(IniTest, WriteFailsWhenTheParentIsAFile) {
    spill(dir() / "blocker", "x");

    Ini ini;

    ini.ensure("s").set("k", "v");

    EXPECT_FALSE(ini.write(dir() / "blocker" / "out.ini"));
}

TEST_F(IniTest, WriteReplacesAnExistingFile) {
    spill(dir() / "out.ini", "[old]\nlonger=content than the new one\n");

    Ini ini;

    ini.ensure("s").set("k", "v");

    ASSERT_TRUE(ini.write(dir() / "out.ini"));

    EXPECT_EQ(slurp(dir() / "out.ini"), "[s]\nk=v\n\n");
}

TEST_F(IniTest, WhatIsWrittenReadsBackTheSame) {
    Ini ini;

    ini.ensure("zdl.save").set("file0", "C:\\Doom\\a b.wad");
    ini.ensure("zdl.save").set("extra", "+set x=1");
    ini.ensure("zdl.profile")
            .set("name",
                 "\xC3\xA9"
                 "dition");

    ASSERT_TRUE(ini.write(dir() / "round.ini"));

    Ini back;

    ASSERT_TRUE(back.read(dir() / "round.ini"));

    for (const char *name : {"zdl.save", "zdl.profile"}) {
        ASSERT_NE(back.section(name), nullptr) << name;
        EXPECT_EQ(back.section(name)->values, ini.section(name)->values) << name;
    }
}
