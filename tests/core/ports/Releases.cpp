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

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Paths.h"

#include "Zip.h"
#include "core/ports/Catalog.h"
#include "core/ports/Releases.h"

namespace {

namespace fs = std::filesystem;

constexpr long long MINUTE = 60;

fs::path cache() {
    return ttk::Paths::data_directory() / ".releases";
}

Releases::Answer answer(const std::string &id) {
    return {.portId = id,
            .version = "1.2.3",
            .url = "https://example.invalid/" + id + ".zip",
            .asset = id + ".zip",
            .verdict = 3,
            .note = "a note",
            .checked = 1700000000,
            .size = 123456789012LL,
            .sha256 = "4b44a1834f1961453ac01c7dd267e32c2f1d0c205464e788765edf3048150751"};
}

bool same(const Releases::Answer &left, const Releases::Answer &right) {
    return left.portId == right.portId && left.version == right.version && left.url == right.url
           && left.asset == right.asset && left.verdict == right.verdict && left.note == right.note
           && left.checked == right.checked && left.size == right.size && left.sha256 == right.sha256;
}

class ReleasesCache : public ::testing::Test {
protected:
    void SetUp() override { fs::remove(cache()); }

    static std::string written(const std::vector<Releases::Answer> &answers) {
        Releases::write(answers);

        return ports::slurp(cache());
    }
};

}

TEST(ReleasesClock, NowIsTheSystemClockInSeconds) {
    const long long expected =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                    .count();

    EXPECT_NEAR(static_cast<double>(Releases::now()), static_cast<double>(expected), 5.0);
}

TEST(ReleasesClock, ACheckJustMadeIsFresh) {
    EXPECT_TRUE(Releases::fresh(Releases::now()));
    EXPECT_TRUE(Releases::fresh(Releases::now() - (29 * MINUTE)));
}

TEST(ReleasesClock, ACheckHalfAnHourOldIsStale) {
    EXPECT_FALSE(Releases::fresh(Releases::now() - (31 * MINUTE)));
}

TEST(ReleasesClock, NeverCheckedIsStale) {
    EXPECT_FALSE(Releases::fresh(0));
    EXPECT_FALSE(Releases::fresh(-1));
}

TEST(ReleasesClock, ACheckFromTheFutureIsStale) {
    EXPECT_FALSE(Releases::fresh(Releases::now() + (5 * MINUTE)));
}

TEST_F(ReleasesCache, NoCacheReadsAsNothing) {
    EXPECT_TRUE(Releases::read().empty());
}

TEST_F(ReleasesCache, WhatIsWrittenReadsBack) {
    const std::vector<Releases::Answer> answers = {answer("uzdoom"), answer("dsda-doom")};

    Releases::write(answers);

    const std::vector<Releases::Answer> back = Releases::read();

    ASSERT_EQ(back.size(), 2U);
    EXPECT_TRUE(same(back[0], answers[0]));
    EXPECT_TRUE(same(back[1], answers[1]));
}

TEST_F(ReleasesCache, EmptyTextsReadBackEmpty) {
    Releases::Answer bare;

    bare.portId = "woof";
    Releases::write(std::vector<Releases::Answer>{bare});

    const std::vector<Releases::Answer> back = Releases::read();

    ASSERT_EQ(back.size(), 1U);
    EXPECT_EQ(back[0].portId, "woof");
    EXPECT_TRUE(back[0].version.empty());
    EXPECT_TRUE(back[0].note.empty());
    EXPECT_TRUE(back[0].sha256.empty());
    EXPECT_EQ(back[0].checked, 0);
}

TEST_F(ReleasesCache, WritingNothingReadsBackNothing) {
    Releases::write({});

    EXPECT_TRUE(fs::exists(cache()));
    EXPECT_TRUE(Releases::read().empty());
}

TEST_F(ReleasesCache, ANewWriteReplacesTheOld) {
    Releases::write(std::vector<Releases::Answer>{answer("uzdoom"), answer("woof")});
    Releases::write(std::vector<Releases::Answer>{answer("helion")});

    const std::vector<Releases::Answer> back = Releases::read();

    ASSERT_EQ(back.size(), 1U);
    EXPECT_EQ(back[0].portId, "helion");
}

TEST_F(ReleasesCache, AWrongMagicIsDiscarded) {
    std::string bytes = written({answer("uzdoom")});

    bytes[0] = 'X';
    ports::write(cache(), bytes);

    EXPECT_TRUE(Releases::read().empty());
    EXPECT_FALSE(fs::exists(cache()));
}

TEST_F(ReleasesCache, AnotherVersionIsDiscarded) {
    std::string bytes = written({answer("uzdoom")});

    bytes[4] = 1;
    ports::write(cache(), bytes);

    EXPECT_TRUE(Releases::read().empty());
    EXPECT_FALSE(fs::exists(cache()));
}

TEST_F(ReleasesCache, ATruncatedCacheIsDiscardedWhole) {
    const std::string bytes = written({answer("uzdoom"), answer("woof")});

    ports::write(cache(), bytes.substr(0, bytes.size() - 3));

    EXPECT_TRUE(Releases::read().empty());
    EXPECT_FALSE(fs::exists(cache()));
}

TEST_F(ReleasesCache, MoreAnswersThanPortsIsDiscarded) {
    std::vector<Releases::Answer> answers;

    for (size_t at = 0; at <= Catalog::ports().size(); at++) {
        answers.push_back(answer("port" + std::to_string(at)));
    }

    Releases::write(answers);

    EXPECT_TRUE(Releases::read().empty());
}

TEST_F(ReleasesCache, ATextLongerThanAnyRealOneIsDiscarded) {
    Releases::Answer huge = answer("uzdoom");

    huge.note = std::string(5000, 'n');
    Releases::write(std::vector<Releases::Answer>{huge});

    EXPECT_TRUE(Releases::read().empty());
}

TEST_F(ReleasesCache, ARandomFileIsDiscarded) {
    ports::write(cache(), "{\"not\": \"a cache\"}");

    EXPECT_TRUE(Releases::read().empty());
    EXPECT_FALSE(fs::exists(cache()));
}

TEST_F(ReleasesCache, TheCacheIsMadeWithItsDirectory) {
    fs::remove_all(ttk::Paths::data_directory());

    Releases::write(std::vector<Releases::Answer>{answer("uzdoom")});

    EXPECT_EQ(Releases::read().size(), 1U);
}
