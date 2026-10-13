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

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "core/util/Ansi.h"
#include "support/Fixtures.h"

namespace {

constexpr int LINES = 24;

// Past two AVX2 vectors, so the scan runs its vector loop and its tail.
constexpr std::size_t LONG = 100;

std::string strip(const std::string_view text) {
    Ansi filter;
    std::string out;

    filter.filter(text, out);

    return out;
}

std::string bytewise(const std::string_view text) {
    Ansi filter;
    std::string out;

    for (const char each : text) {
        filter.filter(std::string_view(&each, 1), out);
    }

    return out;
}

std::string split(const std::string_view text, const std::size_t at) {
    Ansi filter;
    std::string out;

    filter.filter(text.substr(0, at), out);
    filter.filter(text.substr(at), out);

    return out;
}

std::string plain() {
    std::string out;

    for (int at = 0; at < LINES; ++at) {
        out += tests::Fixtures::words(8 + (at % 5), static_cast<unsigned>(at) + 1U);
        out += "\r\n";
    }

    return out;
}

// The lines the way uzdoom writes them to a terminal: coloured, each one followed by
// a progress bar redrawn on the bottom row behind a saved cursor.
std::string decorated() {
    const std::string clean = "\0337\033[24;0H\033[0J\0338";
    const std::string bar =
            "\0337\033[24;0H\033[2K[" + std::string(30, '=') + std::string(48, '.') + "\033[24;80H]\0338";
    std::string out;

    for (int at = 0; at < LINES; ++at) {
        out += clean;
        out += "\033[38;2;223;223;223m";
        out += tests::Fixtures::words(8 + (at % 5), static_cast<unsigned>(at) + 1U);
        out += "\033[0m\r\n";
        out += bar;
    }

    return out;
}

std::vector<std::string> samples() {
    return {
            decorated(),
            "\033]0;title\atext\033]8;;http://x\033\\link\033]8;;\033\\ end\n",
            "\033[1;31m\xC3\xA4\xE6\x97\xA5\033[0m\t\033(Bdone\033Pdcs\033\\\x7f\n",
            std::string(LONG, 'x') + "\033[31m" + std::string(LONG, 'y') + "\x01" + std::string(LONG, 'z'),
            "\0337hidden\tbar\0338shown\0337again\nafter\033[31\nmalformed",
    };
}

}

TEST(Ansi, PlainTextPassesThrough) {
    EXPECT_EQ(strip("hello world"), "hello world");
}

TEST(Ansi, AnEmptyChunkAppendsNothing) {
    EXPECT_EQ(strip(""), "");
}

TEST(Ansi, AppendsToWhatTheOutputAlreadyHolds) {
    Ansi filter;
    std::string out = "kept ";

    filter.filter("\033[1mnew\033[0m", out);

    EXPECT_EQ(out, "kept new");
}

TEST(Ansi, NewlinesCarriageReturnsAndTabsSurvive) {
    EXPECT_EQ(strip("a\tb\r\nc\n"), "a\tb\r\nc\n");
}

TEST(Ansi, OtherControlBytesAreDropped) {
    EXPECT_EQ(strip("a\x07"
                    "b\x08"
                    "c\x01"
                    "d\x7f"
                    "e\x0b"
                    "f\x0c"),
              "abcdef");
}

TEST(Ansi, Utf8PassesThrough) {
    EXPECT_EQ(strip("\xC3\xA4\xE6\x97\xA5\xF0\x9F\x98\x80"), "\xC3\xA4\xE6\x97\xA5\xF0\x9F\x98\x80");
}

TEST(Ansi, ColourSequencesAreDropped) {
    EXPECT_EQ(strip("\033[38;2;223;223;223mgrey\033[0m \033[1;31mred\033[m"), "grey red");
}

TEST(Ansi, CursorAndEraseSequencesAreDropped) {
    EXPECT_EQ(strip("\033[?25l\033[2J\033[24;80H\033[2Kx\033[?25h"), "x");
}

TEST(Ansi, AnOscEndedByABellIsDropped) {
    EXPECT_EQ(strip("\033]0;window title\aafter"), "after");
}

TEST(Ansi, AnOscEndedByAStringTerminatorIsDropped) {
    EXPECT_EQ(strip("\033]8;;http://example.org\033\\link\033]8;;\033\\"), "link");
}

TEST(Ansi, DeviceControlAndPrivateStringsAreDropped) {
    EXPECT_EQ(strip("a\033Pdcs\033\\b\033Xsos\033\\c\033^pm\033\\d\033_apc\033\\e"), "abcde");
}

TEST(Ansi, AnEscapeInsideAStringStartsTheNextSequence) {
    EXPECT_EQ(strip("\033]0;title\033[31mred"), "red");
}

TEST(Ansi, ACharsetDesignationDropsItsOneByte) {
    EXPECT_EQ(strip("\033(Babc\033)0def\033%Gghi\033#8jkl"), "abcdefghijkl");
}

TEST(Ansi, ATwoByteEscapeDropsItsFinal) {
    EXPECT_EQ(strip("\033cabc\033=def\033>ghi"), "abcdefghi");
}

TEST(Ansi, ADoubledEscapeStillStartsASequence) {
    EXPECT_EQ(strip("\033\033[31mx"), "x");
}

TEST(Ansi, AControlByteInsideACsiEndsItAndIsKept) {
    EXPECT_EQ(strip("\033[31\nnext"), "\nnext");
}

TEST(Ansi, AHighByteInsideACsiEndsItAndIsKept) {
    EXPECT_EQ(strip("\033[1\xC3\xA4"), "\xC3\xA4");
}

TEST(Ansi, TextDrawnBehindASavedCursorIsDropped) {
    EXPECT_EQ(strip("log\0337\033[24;0Hstatus bar\0338 more"), "log more");
}

TEST(Ansi, ANewlineEndsASavedCursor) {
    EXPECT_EQ(strip("\0337status\nnext"), "\nnext");
}

TEST(Ansi, AProgressBarRedrawnAtTheBottomLeavesOnlyTheLog) {
    EXPECT_EQ(strip(decorated()), plain());
}

TEST(Ansi, ASequenceSplitBetweenChunksIsStillDropped) {
    Ansi filter;
    std::string out;

    filter.filter("a\033[3", out);
    filter.filter("8;5;12", out);
    filter.filter("mb", out);

    EXPECT_EQ(out, "ab");
}

TEST(Ansi, ASavedCursorCarriesAcrossChunks) {
    Ansi filter;
    std::string out;

    filter.filter("a\0337hid", out);
    filter.filter("den\0338b", out);

    EXPECT_EQ(out, "ab");
}

TEST(Ansi, AnyTwoWaySplitGivesTheSameText) {
    for (const std::string &sample : samples()) {
        const std::string whole = strip(sample);

        for (std::size_t at = 0; at <= sample.size(); ++at) {
            ASSERT_EQ(split(sample, at), whole) << "split at " << at;
        }
    }
}

TEST(Ansi, ByteAtATimeGivesTheSameTextAsOneChunk) {
    for (const std::string &sample : samples()) {
        EXPECT_EQ(bytewise(sample), strip(sample));
    }
}

TEST(Ansi, AControlByteIsFoundAtEveryOffsetOfALongRun) {
    for (std::size_t at = 0; at < LONG; ++at) {
        std::string text(LONG, 'x');

        text[at] = '\x01';

        ASSERT_EQ(strip(text), std::string(LONG - 1, 'x')) << "control at " << at;
    }
}

TEST(Ansi, ADeleteIsFoundAtEveryOffsetOfALongRun) {
    for (std::size_t at = 0; at < LONG; ++at) {
        std::string text(LONG, 'x');

        text[at] = '\x7f';

        ASSERT_EQ(strip(text), std::string(LONG - 1, 'x')) << "delete at " << at;
    }
}

TEST(Ansi, AnEscapeIsFoundAtEveryOffsetOfALongRun) {
    for (std::size_t at = 0; at < LONG; ++at) {
        const std::string text = std::string(at, 'x') + "\033[0m" + std::string(LONG - at, 'y');

        ASSERT_EQ(strip(text), std::string(at, 'x') + std::string(LONG - at, 'y')) << "escape at " << at;
    }
}

TEST(Ansi, PrintableRunsOfEveryLengthAroundTheVectorWidthPassWhole) {
    for (std::size_t length = 1; length <= LONG; ++length) {
        std::string text;

        for (std::size_t at = 0; at < length; ++at) {
            text.push_back(static_cast<char>('!' + (at % 90)));
        }

        ASSERT_EQ(strip(text), text) << "length " << length;
    }
}

TEST(Ansi, AnUnfinishedSequenceAtTheEndSwallowsNothingElse) {
    Ansi filter;
    std::string out;

    filter.filter("text\033[", out);

    EXPECT_EQ(out, "text");

    filter.filter("Kmore", out);

    EXPECT_EQ(out, "textmore");
}
