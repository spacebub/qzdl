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
#include <cstdint>
#include <vector>

#include <blend2d/blend2d.h>
#include <gtest/gtest.h>

#include "Pixels.h"
#include "gui/draw/Mark.h"
#include "support/Canvas.h"

namespace {

using draw::texel;

std::size_t opaque(const BLImage &image) {
    std::size_t found = 0;

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            found += draw::channel(texel(image, x, y), 24) > 0 ? 1U : 0U;
        }
    }

    return found;
}

TEST(Mark, TheSmallMarkIs128Square) {
    const BLImage &mark = Mark::of(128);

    EXPECT_EQ(mark.width(), 128);
    EXPECT_EQ(mark.height(), 128);
}

TEST(Mark, TheLargeMarkIs256Square) {
    const BLImage &mark = Mark::of(256);

    EXPECT_EQ(mark.width(), 256);
    EXPECT_EQ(mark.height(), 256);
}

TEST(Mark, AnythingUpTo128IsTheSmallMark) {
    EXPECT_EQ(&Mark::of(16), &Mark::of(128));
    EXPECT_EQ(&Mark::of(0), &Mark::of(128));
}

TEST(Mark, AnythingOver128IsTheLargeMark) {
    EXPECT_EQ(&Mark::of(129), &Mark::of(256));
    EXPECT_EQ(&Mark::of(4096), &Mark::of(256));
}

TEST(Mark, TheMarkIsDecodedOnce) {
    const BLImage &first = Mark::of(256);
    const BLImage &again = Mark::of(256);

    EXPECT_EQ(&first, &again);
}

TEST(Mark, TheMarkHasInk) {
    EXPECT_GT(opaque(Mark::of(128)), 128U * 128U / 4U);
    EXPECT_GT(opaque(Mark::of(256)), 256U * 256U / 4U);
}

TEST(Mark, TheMiddleOfTheMarkIsOpaque) {
    EXPECT_EQ(draw::channel(texel(Mark::of(128), 64, 64), 24), 255);
    EXPECT_EQ(draw::channel(texel(Mark::of(256), 128, 128), 24), 255);
}

TEST(Mark, TheTwoSizesCoverTheSameShare) {
    const double small = static_cast<double>(opaque(Mark::of(128))) / (128.0 * 128.0);
    const double large = static_cast<double>(opaque(Mark::of(256))) / (256.0 * 256.0);

    EXPECT_NEAR(small, large, 0.05);
}

class MarkOnCanvas : public tests::Painted {
protected:
    tests::Canvas canvas{200, 160};

    std::uint32_t ground = 0;

    void SetUp() override {
        tests::Painted::SetUp();

        if (IsSkipped()) {
            return;
        }

        canvas.full();

        ground = canvas.at(0, 0);
    }

    void blit(const BLRect &box, const int side) {
        canvas.context().blit_image(box, Mark::of(side));
        canvas.context().flush(BL_CONTEXT_FLUSH_SYNC);
    }
};

TEST_F(MarkOnCanvas, ThePaintedMarkStaysInsideItsBox) {
    blit(BLRect{30.0, 20.0, 56.0, 56.0}, 128);

    const draw::Ink found = draw::ink(canvas, BLRectI{0, 0, 200, 160}, ground);

    EXPECT_GT(found.pixels, 56U * 56U / 4U);
    EXPECT_GE(found.left, 30);
    EXPECT_GE(found.top, 20);
    EXPECT_LE(found.right, 86);
    EXPECT_LE(found.bottom, 76);
}

TEST_F(MarkOnCanvas, ThePaintedMarkFillsMostOfItsBox) {
    blit(BLRect{30.0, 20.0, 56.0, 56.0}, 128);

    const draw::Ink found = draw::ink(canvas, BLRectI{0, 0, 200, 160}, ground);

    EXPECT_GT(found.right - found.left, 40);
    EXPECT_GT(found.bottom - found.top, 40);
}

TEST_F(MarkOnCanvas, PaintingTheMarkTwiceGivesTheSamePixels) {
    blit(BLRect{30.0, 20.0, 56.0, 56.0}, 128);

    const std::vector<std::uint32_t> first = canvas.pixels();

    canvas.full();
    blit(BLRect{30.0, 20.0, 56.0, 56.0}, 128);

    EXPECT_EQ(canvas.pixels(), first);
}

TEST_F(MarkOnCanvas, TwoSizesPaintDifferentPixels) {
    blit(BLRect{10.0, 10.0, 40.0, 40.0}, 128);

    const draw::Ink small = draw::ink(canvas, BLRectI{0, 0, 200, 160}, ground);

    canvas.full();
    blit(BLRect{10.0, 10.0, 120.0, 120.0}, 256);

    const draw::Ink large = draw::ink(canvas, BLRectI{0, 0, 200, 160}, ground);

    EXPECT_GT(large.pixels, small.pixels * 4U);
    EXPECT_GT(large.right - large.left, small.right - small.left);
}

}
