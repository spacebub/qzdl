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
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Paths.h"

#include "core/wad/Bytes.h"
#include "gui/services/IwadArt.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"
#include "support/Shell.h"

namespace {

namespace fs = std::filesystem;
namespace Bytes = tests::Bytes;

// Reads land from another thread. Well past what one takes.
constexpr double PATIENCE = 10.0;

fs::path titles() {
    return ttk::Paths::data_directory() / "titles";
}

int filesEndingIn(const std::string &suffix) {
    int count = 0;
    std::error_code code;

    for (const auto &entry : fs::directory_iterator(titles(), code)) {
        count += entry.path().string().ends_with(suffix) ? 1 : 0;
    }

    return count;
}

std::string key(const std::vector<fs::path> &parts) {
    std::string out;

    for (const fs::path &part : parts) {
        out += (out.empty() ? "" : "\n") + part.string();
    }

    return out;
}

class IwadArtTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::error_code code;

        fs::remove_all(titles(), code);

        art = std::make_unique<IwadArt>(&shell);
        art->arrived = [this] { ++arrivals; };
    }

    void TearDown() override { art.reset(); }

    fs::path directory = tests::Sandbox::scratch("art");

    tests::Shell shell;
    std::unique_ptr<IwadArt> art;
    int arrivals = 0;

    fs::path addon(const std::string &name, const std::vector<Bytes::Lump> &lumps) const {
        const fs::path at = directory / name;

        Bytes::write(at, Bytes::wad("PWAD", lumps));

        return at;
    }

    BLImage read(const std::string &asked) {
        (void)art->of(asked);

        EXPECT_TRUE(shell.await(PATIENCE)) << "no answer for " << asked;

        return art->of(asked);
    }

    void restart() {
        art = std::make_unique<IwadArt>(&shell);
        art->arrived = [this] { ++arrivals; };
    }
};

TEST_F(IwadArtTest, AnEmptyKeyIsNoPicture) {
    EXPECT_TRUE(art->of("").is_empty());
    EXPECT_EQ(art->revision(), 0);
}

TEST_F(IwadArtTest, NothingIsDrawnBeforeTheReadArrives) {
    EXPECT_TRUE(art->of(tests::Corpus::iwad().string()).is_empty());
    EXPECT_EQ(art->revision(), 0);
}

TEST_F(IwadArtTest, TheGamesTitleArrivesStretchedToFourByThree) {
    const BLImage image = read(tests::Corpus::iwad().string());

    EXPECT_EQ(image.width(), 320);
    EXPECT_EQ(image.height(), 240);
    EXPECT_EQ(art->revision(), 1);
    EXPECT_EQ(arrivals, 1);
}

TEST_F(IwadArtTest, AnImageTitleKeepsItsSize) {
    const BLImage image = read(tests::Corpus::pk3().string());

    EXPECT_EQ(image.width(), 640);
    EXPECT_EQ(image.height(), 400);
}

TEST_F(IwadArtTest, TheLastAddonWithATitleWins) {
    const fs::path first = addon("first.wad", {{"TITLEPIC", Bytes::patch(100, 50)}});
    const fs::path last = addon("last.wad", {{"TITLEPIC", Bytes::patch(120, 60)}});

    EXPECT_EQ(read(key({tests::Corpus::iwad(), first, last})).width(), 120);
}

TEST_F(IwadArtTest, AnAddonWithoutArtShowsTheGames) {
    const BLImage image = read(key({tests::Corpus::iwad(), tests::Corpus::pwad(1)}));

    EXPECT_EQ(image.width(), 320);
    EXPECT_EQ(image.height(), 240);
}

TEST_F(IwadArtTest, ATitleInAnEarlierAddonBeatsALesserNameInALaterOne) {
    const fs::path title = addon("title.wad", {{"TITLEPIC", Bytes::patch(100, 50)}});
    const fs::path credit = addon("credit.wad", {{"CREDIT", Bytes::patch(60, 30)}});

    EXPECT_EQ(read(key({tests::Corpus::iwad(), title, credit})).width(), 100);
}

TEST_F(IwadArtTest, ALesserNameInAnAddonBeatsTheGamesTitle) {
    const fs::path credit = addon("credit.wad", {{"CREDIT", Bytes::patch(60, 30)}});

    EXPECT_EQ(read(key({tests::Corpus::iwad(), credit})).width(), 60);
}

TEST_F(IwadArtTest, AnAddonPatchIsDrawnInTheGamesColours) {
    const fs::path title = addon("title.wad", {{"TITLEPIC", Bytes::patch(100, 50)}});

    const BLImage image = read(key({tests::Corpus::iwad(), title}));

    BLImageData data{};

    ASSERT_EQ(image.get_data(&data), BL_SUCCESS);

    const auto *pixel = static_cast<const std::uint32_t *>(data.pixel_data);
    const std::array<std::uint8_t, 3> wanted = Bytes::colourOf(0);

    EXPECT_EQ(pixel[0] & 0xFFFFFFU, (std::uint32_t{wanted[0]} << 16U) | (std::uint32_t{wanted[1]} << 8U) | wanted[2]);
}

TEST_F(IwadArtTest, TheGamesPictureStandsInWhileAnAddonIsRead) {
    (void)read(tests::Corpus::iwad().string());

    const BLImage standing = art->of(key({tests::Corpus::iwad(), tests::Corpus::pwad(1)}));

    EXPECT_EQ(standing.width(), 320);
    EXPECT_EQ(standing.height(), 240);
}

TEST_F(IwadArtTest, TheSamePictureArrivingMovesNoRevision) {
    (void)read(tests::Corpus::iwad().string());

    const int was = art->revision();

    (void)read(key({tests::Corpus::iwad(), tests::Corpus::pwad(1)}));

    EXPECT_EQ(art->revision(), was);
    EXPECT_EQ(arrivals, 1);
}

TEST_F(IwadArtTest, EachNewPictureMovesTheRevision) {
    (void)read(tests::Corpus::iwad().string());
    (void)read(tests::Corpus::pk3().string());

    EXPECT_EQ(art->revision(), 2);
    EXPECT_EQ(arrivals, 2);
}

TEST_F(IwadArtTest, AFileWithoutArtIsNoPicture) {
    EXPECT_TRUE(read(tests::Corpus::pwad(1).string()).is_empty());
    EXPECT_EQ(art->revision(), 0);
    EXPECT_EQ(arrivals, 0);
}

TEST_F(IwadArtTest, AMissingGameIsNoPicture) {
    EXPECT_TRUE(read((directory / "absent.wad").string()).is_empty());
}

TEST_F(IwadArtTest, AnAddonPatchWithNoPaletteAnywhereIsNoPicture) {
    const fs::path bare = addon("bare.wad", {{"TITLEPIC", Bytes::patch(10, 10)}});

    EXPECT_TRUE(read(key({tests::Corpus::pwad(1), bare})).is_empty());
}

TEST_F(IwadArtTest, TheAnswerIsCachedOnDisk) {
    (void)read(tests::Corpus::iwad().string());

    EXPECT_EQ(filesEndingIn(".pick"), 1);
    EXPECT_EQ(filesEndingIn(".title"), 1);
    EXPECT_EQ(filesEndingIn(".part"), 0);
}

TEST_F(IwadArtTest, AnImageIsCachedAsItsOwnFile) {
    (void)read(tests::Corpus::pk3().string());

    EXPECT_EQ(filesEndingIn(".png"), 1);
}

TEST_F(IwadArtTest, AFileWithoutArtIsRememberedAsNone) {
    (void)read(tests::Corpus::pwad(1).string());

    EXPECT_EQ(filesEndingIn(".none"), 1);
}

TEST_F(IwadArtTest, ACachedAnswerIsReadAgainAfterARestart) {
    (void)read(tests::Corpus::iwad().string());

    restart();

    const BLImage image = read(tests::Corpus::iwad().string());

    EXPECT_EQ(image.width(), 320);
    EXPECT_EQ(image.height(), 240);
}

TEST_F(IwadArtTest, ACorruptCachedPictureIsDroppedAndMadeAgain) {
    (void)read(tests::Corpus::iwad().string());

    for (const auto &entry : fs::directory_iterator(titles())) {
        if (entry.path().extension() == ".title") {
            Bytes::write(entry.path(), "garbage");
        }
    }

    restart();

    EXPECT_TRUE(read(tests::Corpus::iwad().string()).is_empty());
    EXPECT_EQ(filesEndingIn(".title"), 0);

    restart();

    EXPECT_EQ(read(tests::Corpus::iwad().string()).width(), 320);
}

TEST_F(IwadArtTest, AChangedAddonIsReadAfresh) {
    const fs::path title = addon("title.wad", {{"TITLEPIC", Bytes::patch(100, 50)}});

    EXPECT_EQ(read(key({tests::Corpus::iwad(), title})).width(), 100);

    (void)addon("title.wad", {{"TITLEPIC", Bytes::patch(140, 50)}});

    restart();

    EXPECT_EQ(read(key({tests::Corpus::iwad(), title})).width(), 140);
}

TEST_F(IwadArtTest, PruneRemovesOnlyStaleCacheFiles) {
    fs::create_directories(titles());
    Bytes::write(titles() / "old.title", "old");
    Bytes::write(titles() / "new.title", "new");

    fs::last_write_time(titles() / "old.title", fs::file_time_type::clock::now() - std::chrono::hours(24 * 31));

    IwadArt::prune();

    EXPECT_FALSE(fs::exists(titles() / "old.title"));
    EXPECT_TRUE(fs::exists(titles() / "new.title"));
}

TEST_F(IwadArtTest, PruneWithoutACacheDoesNothing) {
    IwadArt::prune();

    EXPECT_FALSE(fs::exists(titles()));
}

class IwadArtBudgetTest : public IwadArtTest {
protected:
    // A 768 square is 2.25 MB, so 29 of them are past the 64 MB budget.
    static constexpr int MANY = 30;

    std::vector<std::string> keys;

    void SetUp() override {
        IwadArtTest::SetUp();

        const std::string picture = Bytes::png(768, 768, 0xFF808080);

        for (int at = 0; at < MANY; ++at) {
            keys.push_back(addon("big" + std::to_string(at) + ".wad", {{"TITLEPIC", picture}}).string());
        }
    }
};

TEST_F(IwadArtBudgetTest, TheLeastRecentlyDrawnGoPastTheBudget) {
    for (const std::string &each : keys) {
        ASSERT_EQ(read(each).width(), 768);
    }

    EXPECT_TRUE(art->of(keys.front()).is_empty());
    EXPECT_FALSE(art->of(keys.back()).is_empty());
}

TEST_F(IwadArtBudgetTest, WhatIsDrawnUnderTheLatestRevisionIsKept) {
    for (const std::string &each : keys) {
        (void)art->of(each);
    }

    while (art->revision() < MANY) {
        ASSERT_TRUE(shell.await(PATIENCE));
    }

    EXPECT_FALSE(art->of(keys.front()).is_empty());
    EXPECT_FALSE(art->of(keys.back()).is_empty());
}

TEST_F(IwadArtBudgetTest, AnEvictedPictureIsReadAgainWhenAskedFor) {
    for (const std::string &each : keys) {
        (void)read(each);
    }

    ASSERT_TRUE(art->of(keys.front()).is_empty());

    EXPECT_EQ(read(keys.front()).width(), 768);
}

}
