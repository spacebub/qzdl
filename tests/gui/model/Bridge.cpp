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

#include <vector>

#include <gtest/gtest.h>

#include "gui/model/Bridge.h"

namespace {

class Mover final : public Bridge {
public:
    Mover() : Bridge(nullptr, nullptr) {}

    using Bridge::moveTo;
};

using List = std::vector<int>;

List moved(List list, const int from, const int to) {
    Mover::moveTo(list, from, to);

    return list;
}

TEST(Bridge, MovingForwardShiftsTheOnesBetweenBack) {
    EXPECT_EQ(moved({0, 1, 2, 3, 4}, 1, 3), (List{0, 2, 3, 1, 4}));
}

TEST(Bridge, MovingBackShiftsTheOnesBetweenForward) {
    EXPECT_EQ(moved({0, 1, 2, 3, 4}, 3, 1), (List{0, 3, 1, 2, 4}));
}

TEST(Bridge, MovingToTheEndsReachesThem) {
    EXPECT_EQ(moved({0, 1, 2}, 0, 2), (List{1, 2, 0}));
    EXPECT_EQ(moved({0, 1, 2}, 2, 0), (List{2, 0, 1}));
}

TEST(Bridge, MovingOntoItselfChangesNothing) {
    EXPECT_EQ(moved({0, 1, 2}, 1, 1), (List{0, 1, 2}));
}

TEST(Bridge, AnIndexOutsideTheListChangesNothing) {
    EXPECT_EQ(moved({0, 1, 2}, -1, 1), (List{0, 1, 2}));
    EXPECT_EQ(moved({0, 1, 2}, 1, -1), (List{0, 1, 2}));
    EXPECT_EQ(moved({0, 1, 2}, 3, 0), (List{0, 1, 2}));
    EXPECT_EQ(moved({0, 1, 2}, 0, 3), (List{0, 1, 2}));
}

TEST(Bridge, AnEmptyListStaysEmpty) {
    EXPECT_TRUE(moved({}, 0, 0).empty());
}

}
