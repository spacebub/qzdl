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

#include "core/launch/Dialect.h"
#include "core/launch/Netgame.h"

namespace {

using Strings = std::vector<std::string>;

Strings argumentsOf(const char *program, const MultiplayerSettings &mp, const std::filesystem::path &save = {}) {
    Profile profile;

    profile.multiplayer = mp;

    Strings args;

    Netgame::arguments(args, profile, Dialect::of(std::filesystem::path(program)), save);

    return args;
}

MultiplayerSettings hosting(const GameType type, const int players = 4) {
    MultiplayerSettings mp;

    mp.gameType = type;
    mp.players = players;

    return mp;
}

MultiplayerSettings joining(const std::string &host, const std::string &port = {}) {
    MultiplayerSettings mp;

    mp.gameType = GameType::Coop;
    mp.host = host;
    mp.port = port;

    return mp;
}

}

TEST(Netgame, NoGameTypeAddsNothing) {
    MultiplayerSettings mp = hosting(GameType::None);

    mp.extratic = true;
    mp.dup = 2;

    EXPECT_TRUE(argumentsOf("gzdoom", mp).empty());
}

TEST(Netgame, AZdoomHostWaitsForItsPlayerCount) {
    EXPECT_EQ(argumentsOf("gzdoom", hosting(GameType::Coop, 3)), (Strings{"-host", "3"}));
}

TEST(Netgame, DeathmatchAndAltdeathPrecedeTheHostSwitch) {
    EXPECT_EQ(argumentsOf("gzdoom", hosting(GameType::Deathmatch)), (Strings{"-deathmatch", "-host", "4"}));
    EXPECT_EQ(argumentsOf("gzdoom", hosting(GameType::AltDeathmatch)), (Strings{"-altdeath", "-host", "4"}));
}

TEST(Netgame, AZdoomHostSetsEveryCvarInOrder) {
    MultiplayerSettings mp = hosting(GameType::Deathmatch, 2);

    mp.dmflags = "1024";
    mp.dmflags2 = "512";
    mp.port = "5029";
    mp.fragLimit = "20";
    mp.timeLimit = "10";

    EXPECT_EQ(argumentsOf("gzdoom", mp),
              (Strings{"-deathmatch",
                       "+set",
                       "dmflags",
                       "1024",
                       "+set",
                       "dmflags2",
                       "512",
                       "-host",
                       "2",
                       "-port",
                       "5029",
                       "+set",
                       "fraglimit",
                       "20",
                       "+set",
                       "timelimit",
                       "10"}));
}

TEST(Netgame, AZdoomHostLoadsTheNetgameSaveWhenNoOtherIsOnTheLine) {
    MultiplayerSettings mp = hosting(GameType::Coop, 2);

    mp.savegame = "net.zds";

    EXPECT_EQ(argumentsOf("gzdoom", mp), (Strings{"-host", "2", "-loadgame", "net.zds"}));
    EXPECT_EQ(argumentsOf("gzdoom", mp, "/saves/own.zds"), (Strings{"-host", "2"}));
}

TEST(Netgame, AZdoomPortTakesEveryTuningSwitch) {
    MultiplayerSettings mp = hosting(GameType::Coop, 2);

    mp.extratic = true;
    mp.netmode = 1;
    mp.dup = 3;

    EXPECT_EQ(argumentsOf("gzdoom", mp), (Strings{"-host", "2", "-extratic", "-netmode", "1", "-dup", "3"}));
}

TEST(Netgame, UnsetTuningAddsNothing) {
    MultiplayerSettings mp = hosting(GameType::Coop, 2);

    mp.netmode = -1;
    mp.dup = 0;

    EXPECT_EQ(argumentsOf("gzdoom", mp), (Strings{"-host", "2"}));
}

TEST(Netgame, UzdoomIsNotHandedNetmode) {
    MultiplayerSettings mp = hosting(GameType::Coop, 2);

    mp.netmode = 0;

    EXPECT_EQ(argumentsOf("uzdoom", mp), (Strings{"-host", "2"}));
}

TEST(Netgame, AChocolateHostIsAPrivateServerUnlessListed) {
    MultiplayerSettings mp = hosting(GameType::Coop, 2);

    EXPECT_EQ(argumentsOf("chocolate-doom", mp), (Strings{"-privateserver"}));

    mp.listed = true;

    EXPECT_EQ(argumentsOf("chocolate-doom", mp), (Strings{"-server"}));
}

TEST(Netgame, AChocolateHostTimesWithTimerAndSkipsZdoomCvars) {
    MultiplayerSettings mp = hosting(GameType::Deathmatch, 2);

    mp.dmflags = "4";
    mp.fragLimit = "20";
    mp.timeLimit = "10";
    mp.savegame = "net.dsg";
    mp.extratic = true;
    mp.netmode = 1;
    mp.dup = 2;

    EXPECT_EQ(argumentsOf("chocolate-doom", mp),
              (Strings{"-deathmatch", "-privateserver", "-timer", "10", "-dup", "2"}));
}

TEST(Netgame, APrboomPortCannotHost) {
    EXPECT_TRUE(argumentsOf("prboom-plus", hosting(GameType::Coop, 2)).empty());
}

TEST(Netgame, APortWithoutNetgamesAddsNothing) {
    EXPECT_TRUE(argumentsOf("dsda-doom", hosting(GameType::Coop, 2)).empty());
    EXPECT_TRUE(argumentsOf("dsda-doom", joining("10.0.0.1")).empty());
}

TEST(Netgame, EachFamilyJoinsWithItsOwnSwitch) {
    EXPECT_EQ(argumentsOf("gzdoom", joining("10.0.0.1")), (Strings{"-join", "10.0.0.1"}));
    EXPECT_EQ(argumentsOf("chocolate-doom", joining("10.0.0.1")), (Strings{"-connect", "10.0.0.1"}));
    EXPECT_EQ(argumentsOf("prboom-plus", joining("10.0.0.1")), (Strings{"-net", "10.0.0.1"}));
}

TEST(Netgame, AJoinAddsThePortToTheHost) {
    EXPECT_EQ(argumentsOf("gzdoom", joining("10.0.0.1", "5029")), (Strings{"-join", "10.0.0.1:5029"}));
}

TEST(Netgame, AJoinReplacesAPortAlreadyOnTheHost) {
    EXPECT_EQ(argumentsOf("gzdoom", joining("doom.example:666", "5029")), (Strings{"-join", "doom.example:5029"}));
}

TEST(Netgame, AJoinDropsTrailingSpaceFromTheHost) {
    EXPECT_EQ(argumentsOf("gzdoom", joining("doom.example  ", "5029")), (Strings{"-join", "doom.example:5029"}));
}

TEST(Netgame, AJoinKeepsTheLastOctetOfAnAddress) {
    EXPECT_EQ(argumentsOf("gzdoom", joining("192.168.1.20", "5029")), (Strings{"-join", "192.168.1.20:5029"}));
}

TEST(Netgame, AJoinWithNoHostJoinsNothing) {
    EXPECT_TRUE(argumentsOf("gzdoom", joining("")).empty());
}
