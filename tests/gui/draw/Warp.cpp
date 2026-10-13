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

#include <cmath>
#include <cstdint>

#include <blend2d/blend2d.h>
#include <gtest/gtest.h>

#include "Pixels.h"
#include "gui/draw/Warp.h"
#include "support/Canvas.h"

namespace {

using draw::apart;
using draw::texel;

constexpr Warp::Map IDENTITY{{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};

// Every texel a colour of its own, opaque.
BLImage checker(const int width, const int height) {
    BLImage made(width, height, BL_FORMAT_PRGB32);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            draw::put(made,
                      x,
                      y,
                      0xff000000U | (static_cast<std::uint32_t>(x * 8) << 16U)
                              | (static_cast<std::uint32_t>(y * 8) << 8U) | 0x40U);
        }
    }

    return made;
}

// Opaque red inside a clear rim one texel wide.
BLImage framed(const int side) {
    BLImage made(side, side, BL_FORMAT_PRGB32);

    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < side; ++x) {
            const bool rim = x == 0 || y == 0 || x == side - 1 || y == side - 1;

            draw::put(made, x, y, rim ? 0U : 0xffff0000U);
        }
    }

    return made;
}

// A card tipped back about the vertical through `middle`: narrower further down.
Warp::Map tipped(const double middle, const double k) {
    return Warp::Map{{{1.0, middle * k, 0.0}, {0.0, 1.0, 0.0}, {0.0, k, 1.0}}};
}

bool near(const BLPoint one, const BLPoint two) {
    return std::abs(one.x - two.x) < 1e-9 && std::abs(one.y - two.y) < 1e-9;
}

TEST(Warp, TheIdentityMapsAPointToItself) {
    EXPECT_TRUE(near(IDENTITY.apply(BLPoint{3.5, -2.0}), BLPoint{3.5, -2.0}));
}

TEST(Warp, ApplyDividesByTheThirdRow) {
    const Warp::Map perspective{{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.01, 0.0, 1.0}}};
    const BLPoint got = perspective.apply(BLPoint{100.0, 50.0});

    EXPECT_NEAR(got.x, 50.0, 1e-9);
    EXPECT_NEAR(got.y, 25.0, 1e-9);
}

TEST(Warp, TheInverseUndoesTheMap) {
    const Warp::Map map{{{1.2, 0.1, 5.0}, {-0.2, 0.9, 3.0}, {0.001, 0.002, 1.0}}};
    const Warp::Map back = map.inverse();

    for (const BLPoint at : {BLPoint{0.0, 0.0}, BLPoint{10.0, 20.0}, BLPoint{-7.5, 33.0}}) {
        EXPECT_TRUE(near(back.apply(map.apply(at)), at)) << at.x << ", " << at.y;
    }
}

TEST(Warp, TheIdentityIsItsOwnInverse) {
    const Warp::Map back = IDENTITY.inverse();

    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            EXPECT_DOUBLE_EQ(back.m[row][column], IDENTITY.m[row][column]);
        }
    }
}

TEST(Warp, ASingularMapHasAZeroInverse) {
    const Warp::Map flat{{{1.0, 2.0, 3.0}, {2.0, 4.0, 6.0}, {0.0, 0.0, 1.0}}};
    const Warp::Map back = flat.inverse();

    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            EXPECT_DOUBLE_EQ(back.m[row][column], 0.0);
        }
    }
}

TEST(Warp, NothingIsDrawnFromAnEmptySource) {
    BLImage out;

    EXPECT_FALSE(Warp::render(BLImage(), BLPointI{0, 0}, IDENTITY, BLRectI{0, 0, 8, 8}, false, out));
}

TEST(Warp, NothingIsDrawnFromASourceTooThinToBlend) {
    BLImage out;

    EXPECT_FALSE(Warp::render(checker(1, 8), BLPointI{0, 0}, IDENTITY, BLRectI{0, 0, 8, 8}, true, out));
    EXPECT_FALSE(Warp::render(checker(8, 1), BLPointI{0, 0}, IDENTITY, BLRectI{0, 0, 8, 8}, true, out));
}

TEST(Warp, NothingIsDrawnIntoAnEmptyArea) {
    BLImage out;

    EXPECT_FALSE(Warp::render(checker(8, 8), BLPointI{0, 0}, IDENTITY, BLRectI{0, 0, 0, 8}, false, out));
    EXPECT_FALSE(Warp::render(checker(8, 8), BLPointI{0, 0}, IDENTITY, BLRectI{0, 0, 8, 0}, false, out));
}

TEST(Warp, TheOutputGrowsToTheArea) {
    BLImage out(4, 4, BL_FORMAT_PRGB32);

    ASSERT_TRUE(Warp::render(checker(8, 8), BLPointI{0, 0}, IDENTITY, BLRectI{0, 0, 12, 10}, false, out));

    EXPECT_EQ(out.width(), 12);
    EXPECT_EQ(out.height(), 10);
}

TEST(Warp, TheOutputIsNeverShrunk) {
    BLImage out;

    ASSERT_TRUE(Warp::render(checker(8, 8), BLPointI{0, 0}, IDENTITY, BLRectI{0, 0, 12, 10}, false, out));
    ASSERT_TRUE(Warp::render(checker(8, 8), BLPointI{0, 0}, IDENTITY, BLRectI{0, 0, 2, 3}, false, out));

    EXPECT_EQ(out.width(), 12);
    EXPECT_EQ(out.height(), 10);
}

TEST(Warp, TheNearestTexelThroughTheIdentityIsACopy) {
    const BLImage source = checker(37, 9);
    BLImage out;

    ASSERT_TRUE(Warp::render(source, BLPointI{0, 0}, IDENTITY, BLRectI{0, 0, 37, 9}, false, out));

    for (int y = 0; y < 9; ++y) {
        for (int x = 0; x < 37; ++x) {
            ASSERT_EQ(texel(out, x, y), texel(source, x, y)) << x << ", " << y;
        }
    }
}

TEST(Warp, ABlendThroughTheIdentityIsACopyToARounding) {
    const BLImage source = checker(37, 9);
    BLImage out;

    ASSERT_TRUE(Warp::render(source, BLPointI{0, 0}, IDENTITY, BLRectI{0, 0, 37, 9}, true, out));

    for (int y = 0; y < 9; ++y) {
        for (int x = 0; x < 37; ++x) {
            ASSERT_LE(apart(texel(out, x, y), texel(source, x, y)), 2) << x << ", " << y;
        }
    }
}

TEST(Warp, TheOriginPlacesTheSource) {
    const BLImage source = checker(8, 8);
    BLImage out;

    ASSERT_TRUE(Warp::render(source, BLPointI{20, 30}, IDENTITY, BLRectI{20, 30, 8, 8}, false, out));

    EXPECT_EQ(texel(out, 0, 0), texel(source, 0, 0));
    EXPECT_EQ(texel(out, 7, 7), texel(source, 7, 7));
    EXPECT_EQ(texel(out, 3, 5), texel(source, 3, 5));
}

TEST(Warp, AMovedMapMovesThePicture) {
    const BLImage source = checker(16, 16);
    const Warp::Map moved{{{1.0, 0.0, 3.0}, {0.0, 1.0, 2.0}, {0.0, 0.0, 1.0}}};
    BLImage out;

    ASSERT_TRUE(Warp::render(source, BLPointI{0, 0}, moved, BLRectI{0, 0, 16, 16}, false, out));

    EXPECT_EQ(texel(out, 3, 2), texel(source, 0, 0));
    EXPECT_EQ(texel(out, 10, 9), texel(source, 7, 7));
}

TEST(Warp, PastTheEdgeTheSourceIsExtended) {
    const BLImage source = checker(16, 16);
    const Warp::Map moved{{{1.0, 0.0, 3.0}, {0.0, 1.0, 2.0}, {0.0, 0.0, 1.0}}};
    BLImage out;

    ASSERT_TRUE(Warp::render(source, BLPointI{0, 0}, moved, BLRectI{0, 0, 16, 16}, false, out));

    EXPECT_EQ(texel(out, 0, 0), texel(source, 0, 0));
    EXPECT_EQ(texel(out, 1, 5), texel(source, 0, 3));
}

TEST(Warp, AMirrorReversesEachRow) {
    const BLImage source = checker(24, 4);
    const Warp::Map mirror{{{-1.0, 0.0, 24.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};
    BLImage out;

    ASSERT_TRUE(Warp::render(source, BLPointI{0, 0}, mirror, BLRectI{0, 0, 24, 4}, false, out));

    for (int x = 0; x < 24; ++x) {
        EXPECT_EQ(texel(out, x, 2), texel(source, 23 - x, 2)) << x;
    }
}

TEST(Warp, DoublingTheScaleDoublesEachTexel) {
    const BLImage source = checker(8, 8);
    const Warp::Map doubled{{{2.0, 0.0, 0.0}, {0.0, 2.0, 0.0}, {0.0, 0.0, 1.0}}};
    BLImage out;

    ASSERT_TRUE(Warp::render(source, BLPointI{0, 0}, doubled, BLRectI{0, 0, 16, 16}, false, out));

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            EXPECT_EQ(texel(out, (2 * x) + 1, (2 * y) + 1), texel(source, x, y)) << x << ", " << y;
        }
    }
}

TEST(Warp, AClearRimStaysClearPastTheCard) {
    const BLImage source = framed(16);
    const Warp::Map shrunk{{{0.5, 0.0, 8.0}, {0.0, 0.5, 8.0}, {0.0, 0.0, 1.0}}};
    BLImage out;

    ASSERT_TRUE(Warp::render(source, BLPointI{0, 0}, shrunk, BLRectI{0, 0, 24, 24}, true, out));

    EXPECT_EQ(texel(out, 0, 0), 0U);
    EXPECT_EQ(texel(out, 23, 23), 0U);
    EXPECT_EQ(texel(out, 12, 4), 0U);
    EXPECT_EQ(texel(out, 12, 12), 0xffff0000U);
}

TEST(Warp, ATippedCardStaysSymmetricAboutItsMiddle) {
    constexpr int SIDE = 48;
    const BLImage source = framed(SIDE);
    BLImage out;

    ASSERT_TRUE(Warp::render(source, BLPointI{0, 0}, tipped(SIDE / 2.0, 0.01), BLRectI{0, 0, SIDE, SIDE}, true, out));

    int crooked = 0;

    for (int y = 0; y < SIDE; ++y) {
        for (int x = 0; x < SIDE / 2; ++x) {
            crooked += apart(texel(out, x, y), texel(out, SIDE - 1 - x, y)) > 4 ? 1 : 0;
        }
    }

    EXPECT_EQ(crooked, 0);
}

TEST(Warp, ATippedCardNarrowsFurtherDown) {
    constexpr int SIDE = 48;
    const BLImage source = framed(SIDE);
    BLImage out;

    ASSERT_TRUE(Warp::render(source, BLPointI{0, 0}, tipped(SIDE / 2.0, 0.01), BLRectI{0, 0, SIDE, SIDE}, false, out));

    const auto width = [&out](const int y) {
        int lit = 0;

        for (int x = 0; x < SIDE; ++x) {
            lit += texel(out, x, y) != 0U ? 1 : 0;
        }

        return lit;
    };

    EXPECT_GT(width(4), width(SIDE / 2));
    EXPECT_GT(width(SIDE / 2), 0);
}

TEST(Warp, TwoRendersOfOneMapAgree) {
    const BLImage source = checker(30, 30);
    const Warp::Map map = tipped(15.0, 0.02);
    BLImage one;
    BLImage two;

    ASSERT_TRUE(Warp::render(source, BLPointI{0, 0}, map, BLRectI{0, 0, 30, 30}, true, one));
    ASSERT_TRUE(Warp::render(source, BLPointI{0, 0}, map, BLRectI{0, 0, 30, 30}, true, two));

    for (int y = 0; y < 30; ++y) {
        for (int x = 0; x < 30; ++x) {
            ASSERT_EQ(texel(one, x, y), texel(two, x, y)) << x << ", " << y;
        }
    }
}

class WarpOnCanvas : public tests::Painted {};

TEST_F(WarpOnCanvas, AWarpedCardLandsInsideItsArea) {
    tests::Canvas canvas(160, 120);

    canvas.full();

    const std::uint32_t ground = canvas.at(0, 0);
    const BLRectI area{40, 30, 64, 48};
    BLImage out;

    ASSERT_TRUE(Warp::render(framed(32),
                             BLPointI{40, 30},
                             Warp::Map{{{2.0, 0.0, -40.0}, {0.0, 1.5, -15.0}, {0.0, 0.0, 1.0}}},
                             area,
                             true,
                             out));

    canvas.context().blit_image(BLPointI{area.x, area.y}, out, BLRectI{0, 0, area.w, area.h});
    canvas.context().flush(BL_CONTEXT_FLUSH_SYNC);

    const draw::Ink found = draw::ink(canvas, BLRectI{0, 0, 160, 120}, ground);

    EXPECT_GT(found.pixels, 0U);
    EXPECT_GE(found.left, area.x);
    EXPECT_GE(found.top, area.y);
    EXPECT_LE(found.right, area.x + area.w);
    EXPECT_LE(found.bottom, area.y + area.h);
}

}
