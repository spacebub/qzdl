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

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/launch/Dialect.h"
#include "core/launch/Netgame.h"

namespace {

using Strings = std::vector<std::string>;

Strings argumentsOf(const char *file, const MultiplayerSettings &mp, const std::filesystem::path &save = {}) {
    Profile profile;

    profile.multiplayer = mp;

    Strings args;

    std::ignore = Netgame::arguments(args, profile, Dialect::of(std::filesystem::path(file)), save);

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

TEST(Netgame, UnsupportedMeansThePortDoesNotTakeTheSide) {
    const auto unsupported = [](const char *file, const MultiplayerSettings &mp) {
        Profile profile;

        profile.multiplayer = mp;

        return Netgame::unsupported(profile, Dialect::of(std::filesystem::path(file)));
    };

    EXPECT_FALSE(unsupported("gzdoom", hosting(GameType::Coop)));
    EXPECT_FALSE(unsupported("gzdoom", joining("10.0.0.9")));
    EXPECT_TRUE(unsupported("prboom-plus", hosting(GameType::Coop)));
    EXPECT_FALSE(unsupported("prboom-plus", joining("10.0.0.9")));
    EXPECT_TRUE(unsupported("helion", joining("10.0.0.9")));
    EXPECT_FALSE(unsupported("helion", MultiplayerSettings{}));
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

    mp.port = "5029";
    mp.fragLimit = "20";
    mp.timeLimit = "10";

    EXPECT_EQ(argumentsOf("gzdoom", mp),
              (Strings{"-deathmatch",
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

TEST(Netgame, APickedSaveIsHandedAsTheNameThePortLooksFor) {
    MultiplayerSettings mp = hosting(GameType::Coop, 2);

    mp.savegame = "/home/player/saves/net.zds";

    EXPECT_EQ(argumentsOf("uzdoom", mp), (Strings{"-host", "2", "-loadgame", "net.zds"}));
    EXPECT_EQ(argumentsOf("zdoom", mp), (Strings{"-host", "2", "-loadgame", "/home/player/saves/net.zds"}));
}

TEST(Netgame, AServerLoadsNoSaveAndHasNoPlayerOfItsOwn) {
    MultiplayerSettings mp = hosting(GameType::Coop, 2);

    mp.savegame = "/home/player/saves/net.zds";
    mp.playerName = "Doom Guy";
    mp.playerColor = "#ff8000";

    for (const char *file : {"zandronum", "skulltag"}) {
        EXPECT_EQ(argumentsOf(file, mp), (Strings{"-host", "2"})) << file;
    }
}

TEST(Netgame, AServerAndItsClientsTuneNoTics) {
    MultiplayerSettings host = hosting(GameType::Coop, 2);
    MultiplayerSettings client = joining("10.0.0.9");

    for (MultiplayerSettings *mp : {&host, &client}) {
        mp->extratic = true;
        mp->netmode = 1;
        mp->dup = 3;
    }

    EXPECT_EQ(argumentsOf("zandronum", host), (Strings{"-host", "2"}));
    EXPECT_EQ(argumentsOf("zandronum", client), (Strings{"-connect", "10.0.0.9"}));
}

TEST(Netgame, OnlyAServerLeavesTheSaveReplayAndPlayerOut) {
    const auto leftOut = [](const char *file, const MultiplayerSettings &mp) {
        Profile profile;

        profile.multiplayer = mp;

        return Netgame::leftOut(profile, Dialect::of(std::filesystem::path(file)));
    };

    EXPECT_EQ(leftOut("zandronum", hosting(GameType::Coop)), Netgame::SERVER_LEAVES_OUT);
    EXPECT_EQ(leftOut("skulltag", hosting(GameType::Coop, 1)), Netgame::SERVER_LEAVES_OUT);
    EXPECT_EQ(leftOut("zandronum", joining("10.0.0.9")), Netgame::CLIENT_LEAVES_OUT);
    EXPECT_EQ(leftOut("zandronum", MultiplayerSettings{}), Dialect::Features());
    EXPECT_EQ(leftOut("gzdoom", hosting(GameType::Coop)), Dialect::Features());
    EXPECT_EQ(leftOut("gzdoom", joining("10.0.0.9")), Dialect::Features());
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
    EXPECT_EQ(argumentsOf("zandronum", joining("10.0.0.1")), (Strings{"-connect", "10.0.0.1"}));
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

TEST(Netgame, TeamDeathmatchIsDeathmatchWithTeamplay) {
    MultiplayerSettings mp = hosting(GameType::TeamDeathmatch);

    mp.team = 2;

    const Strings args = argumentsOf("gzdoom", mp);

    EXPECT_EQ(Strings(args.begin(), args.begin() + 3), (Strings{"-deathmatch", "+teamplay", "1"}));
    EXPECT_NE(std::ranges::search(args, Strings{"+team", "2"}).begin(), args.end());
}

TEST(Netgame, AHostOutsideTeamsKeepsItsTeamToItself) {
    MultiplayerSettings mp = hosting(GameType::Deathmatch);

    mp.team = 2;

    const Strings args = argumentsOf("gzdoom", mp);

    EXPECT_EQ(std::ranges::find(args, "+team"), args.end());
    EXPECT_EQ(std::ranges::find(args, "+teamplay"), args.end());
}

TEST(Netgame, AJoiningPlayerHandsOnItsTeamAnyway) {
    MultiplayerSettings mp = joining("10.0.0.7");

    mp.team = 0;

    const Strings args = argumentsOf("uzdoom", mp);

    EXPECT_NE(std::ranges::search(args, Strings{"+team", "0"}).begin(), args.end());
}

TEST(Netgame, ZandronumPlaysTeamsButTakesNoTeamPick) {
    MultiplayerSettings mp = hosting(GameType::TeamDeathmatch);

    mp.team = 1;

    const Strings args = argumentsOf("zandronum", mp);

    EXPECT_NE(std::ranges::search(args, Strings{"+teamplay", "1"}).begin(), args.end());
    EXPECT_EQ(std::ranges::find(args, "+team"), args.end());
}

TEST(Netgame, SkulltagJoinsAServerWithConnect) {
    EXPECT_EQ(argumentsOf("skulltag", joining("10.0.0.9", "10667")), (Strings{"-connect", "10.0.0.9:10667"}));
    EXPECT_EQ(argumentsOf("skulltag", hosting(GameType::Coop)), (Strings{"-host", "4"}));
}

TEST(Netgame, AHostIsOpenedForNoMoreThanThePortTakes) {
    EXPECT_EQ(argumentsOf("gzdoom", hosting(GameType::Coop, 40)), (Strings{"-host", "8"}));
    EXPECT_EQ(argumentsOf("zandronum", hosting(GameType::Coop, 40)), (Strings{"-host", "40"}));
    EXPECT_EQ(argumentsOf("skulltag", hosting(GameType::Coop, 40)), (Strings{"-host", "32"}));
    EXPECT_EQ(argumentsOf("gzdoom", hosting(GameType::Coop, 3)), (Strings{"-host", "3"}));

    MultiplayerSettings joiner = joining("10.0.0.9");

    EXPECT_EQ(Netgame::players(joiner, Dialect::of(std::filesystem::path("gzdoom"))), 0);
}

TEST(Netgame, SkulltagTakesTheColorWithoutAColorSet) {
    MultiplayerSettings mp = joining("10.0.0.7");

    mp.playerColor = "#ff8000";

    EXPECT_EQ(argumentsOf("skulltag", mp), (Strings{"-connect", "10.0.0.7", "+color", "#ff8000"}));
}

TEST(Netgame, TheNameAndColorGoWithTheColorSetCleared) {
    MultiplayerSettings mp = joining("10.0.0.7");

    mp.playerName = "Doom Guy";
    mp.playerColor = "#ff8000";

    const Strings args = argumentsOf("gzdoom", mp);

    EXPECT_NE(std::ranges::search(args, Strings{"+name", "Doom Guy", "+color", "#ff8000", "+colorset \"-1\""}).begin(),
              args.end());
}

TEST(Netgame, ANameStartingWithADashStaysOneArgument) {
    MultiplayerSettings mp = joining("10.0.0.7");

    mp.playerName = "-=X=-";

    const Strings args = argumentsOf("gzdoom", mp);

    EXPECT_NE(std::ranges::find(args, "+name \"-=X=-\""), args.end());
}

TEST(Netgame, OnlyTheZdoomFamilyIsHandedANameOrTeam) {
    MultiplayerSettings mp = joining("10.0.0.7");

    mp.playerName = "Doomguy";
    mp.playerColor = "#ff8000";
    mp.team = 1;

    for (const char *file : {"chocolate-doom", "woof", "prboom-plus"}) {
        const Strings args = argumentsOf(file, mp);

        EXPECT_EQ(std::ranges::find(args, "+name"), args.end()) << file;
        EXPECT_EQ(std::ranges::find(args, "+team"), args.end()) << file;
    }
}

TEST(Netgame, NoNetgameHandsOnNoName) {
    MultiplayerSettings mp;

    mp.playerName = "Doomguy";

    EXPECT_TRUE(argumentsOf("gzdoom", mp).empty());
}

TEST(Netgame, AJoinWithoutAnAddressHandsOnNoName) {
    MultiplayerSettings mp = joining("");

    mp.playerName = "Doomguy";
    mp.playerColor = "#ff8000";

    EXPECT_TRUE(argumentsOf("gzdoom", mp).empty());
}

TEST(Netgame, ANameStartingWithAPlusStaysOneArgumentWithItsQuotesEscaped) {
    MultiplayerSettings mp = joining("10.0.0.7");

    mp.playerName = "+say \"hi\"";

    const Strings args = argumentsOf("gzdoom", mp);

    EXPECT_NE(std::ranges::find(args, "+name \"+say \\\"hi\\\"\""), args.end());
}
