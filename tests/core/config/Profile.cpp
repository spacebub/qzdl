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
#include <cstdlib>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include "ttk/system/Json.h"

#include "Same.h"
#include "core/config/Profile.h"

namespace {

namespace Json = ttk::Json;

constexpr int MANY = 1000;

Profile parsed(const std::string &text) {
    std::string error;
    const Json::Doc doc = Json::read_data(text, &error);

    EXPECT_TRUE(doc.valid()) << text << ": " << error;

    return Profile::fromJson(doc.root());
}

std::string written(const Profile &profile) {
    const Json::Builder builder;
    size_t length = 0;
    char *text = yyjson_mut_val_write(profile.toJson(builder), YYJSON_WRITE_NOFLAG, &length);
    std::string out(text, length);

    free(text); // NOLINT(cppcoreguidelines-no-malloc): yyjson hands out malloc'd text.

    return out;
}

Profile full() {
    Profile made;

    made.id = "9b3f1c2e-4d5a-4f6b-8c7d-000000000042";
    made.name = "Sunlust \"final\" \xC3\xA9"
                "dition \xE6\x97\xA5";
    made.iwad = "Doom II";
    made.port = "GZDoom 4.14";
    made.files = {{.file = "C:\\Games\\Doom\\sunlust.wad", .enabled = true},
                  {.file = "/home/user/mods/brutal v22.pk3", .enabled = false}};
    made.skill = 4;
    made.monsters = 2;
    made.warp = "MAP29";
    made.extra = "-fast +set sv_cheats 1";
    made.dialogOpen = true;
    made.replayOpen = true;
    made.saveOpen = true;
    made.multiplayer = {.gameType = GameType::AltDeathmatch,
                        .players = 4,
                        .extratic = true,
                        .netmode = 1,
                        .dup = 2,
                        .host = "10.0.0.7",
                        .port = "5030",
                        .fragLimit = "20",
                        .timeLimit = "15",
                        .dmflags = "1024",
                        .dmflags2 = "2048",
                        .savegame = "net.zds",
                        .listed = true};
    made.replay = {.mode = ReplayMode::Play,
                   .file = "run.lmp",
                   .playback = Playback::Fast,
                   .compatibility = 9,
                   .longtics = true,
                   .soloNet = true};
    made.save = {.enabled = true, .file = "save3.zds"};
    made.config = "sunlust.cfg";
    made.sharedConfig = true;
    made.customCommand = true;
    made.command = "{source_port} -iwad {game} -file {addon_0}";
    made.dosFullscreen = false;
    made.dosExit = false;
    made.captureOutput = true;
    made.levelstat = true;

    return made;
}

bool isHex(const char each) {
    return (each >= '0' && each <= '9') || (each >= 'a' && each <= 'f');
}

}

TEST(ProfileId, IsAVersionFourUuid) {
    const std::string id = Profile::newId();

    ASSERT_EQ(id.size(), 36U);

    for (size_t at = 0; at < id.size(); ++at) {
        if (at == 8 || at == 13 || at == 18 || at == 23) {
            EXPECT_EQ(id[at], '-') << id;
        } else {
            EXPECT_TRUE(isHex(id[at])) << id;
        }
    }

    EXPECT_EQ(id[14], '4') << id;
    EXPECT_NE(std::string("89ab").find(id[19]), std::string::npos) << id;
}

TEST(ProfileId, IsNewEveryTime) {
    std::set<std::string> seen;

    for (int at = 0; at < MANY; ++at) {
        seen.insert(Profile::newId());
    }

    EXPECT_EQ(seen.size(), static_cast<size_t>(MANY));
}

TEST(ProfileFromJson, AnEmptyObjectGivesTheDefaultsAndAnId) {
    const Profile profile = parsed("{}");
    Profile want;

    want.id = profile.id;

    EXPECT_EQ(profile.id.size(), 36U);
    same::profile(profile, want);
    EXPECT_TRUE(profile.dosFullscreen);
    EXPECT_TRUE(profile.dosExit);
    EXPECT_EQ(profile.multiplayer.netmode, -1);
    EXPECT_EQ(profile.replay.compatibility, -1);
}

TEST(ProfileFromJson, KeepsTheIdItWasGiven) {
    EXPECT_EQ(parsed(R"({"id":"kept"})").id, "kept");
}

TEST(ProfileFromJson, AnEmptyIdIsIssuedAFreshOne) {
    EXPECT_EQ(parsed(R"({"id":""})").id.size(), 36U);
}

TEST(ProfileFromJson, ANonObjectGivesABlankProfileWithAnId) {
    const Profile fromNull = Profile::fromJson(nullptr);
    const Profile fromArray = parsed(R"(["name"])");

    EXPECT_EQ(fromNull.id.size(), 36U);
    EXPECT_TRUE(fromNull.name.empty());
    EXPECT_EQ(fromArray.id.size(), 36U);
    EXPECT_TRUE(fromArray.name.empty());
}

TEST(ProfileFromJson, ABareStringIsAnEnabledFile) {
    const Profile profile = parsed(R"({"files":["/mods/a.wad",{"file":"/mods/b.wad","enabled":false}]})");

    same::files(profile.files, {{.file = "/mods/a.wad", .enabled = true}, {.file = "/mods/b.wad", .enabled = false}});
}

TEST(ProfileFromJson, AFileWithoutEnabledIsEnabled) {
    const Profile profile = parsed(R"({"files":[{"file":"/mods/a.wad"}]})");

    same::files(profile.files, {{.file = "/mods/a.wad", .enabled = true}});
}

TEST(ProfileFromJson, AFileEntryWithoutAPathIsDropped) {
    const Profile profile = parsed(R"({"files":[{"enabled":true},{"file":""},{"file":"/mods/a.wad"},42,null]})");

    same::files(profile.files, {{.file = "/mods/a.wad", .enabled = true}});
}

TEST(ProfileFromJson, FilesThatAreNotAnArrayAreNone) {
    EXPECT_TRUE(parsed(R"({"files":"/mods/a.wad"})").files.empty());
}

TEST(ProfileFromJson, NumbersWrittenAsTextAreRead) {
    const Profile profile = parsed(R"({"skill":"3","monsters":"2","multiplayer":{"players":"4","gameType":"1"}})");

    EXPECT_EQ(profile.skill, 3);
    EXPECT_EQ(profile.monsters, 2);
    EXPECT_EQ(profile.multiplayer.players, 4);
    EXPECT_EQ(profile.multiplayer.gameType, GameType::Coop);
}

TEST(ProfileFromJson, RealsAreTruncated) {
    EXPECT_EQ(parsed(R"({"skill":3.9})").skill, 3);
}

TEST(ProfileFromJson, AValueOfTheWrongTypeFallsBackToItsDefault) {
    const Profile profile = parsed(R"({"name":7,"skill":[1],"dosFullscreen":null,"dosExit":{},)"
                                   R"("multiplayer":{"netmode":"x"},"replay":{"compatibility":null}})");

    EXPECT_TRUE(profile.name.empty());
    EXPECT_EQ(profile.skill, 0);
    EXPECT_TRUE(profile.dosFullscreen);
    EXPECT_TRUE(profile.dosExit);
    EXPECT_EQ(profile.multiplayer.netmode, -1);
    EXPECT_EQ(profile.replay.compatibility, -1);
}

TEST(ProfileFromJson, BooleansAcceptNumbersAndText) {
    const Profile profile = parsed(R"({"dialogOpen":1,"replayOpen":"true","saveOpen":"TRUE","levelstat":0,)"
                                   R"("dosFullscreen":"no"})");

    EXPECT_TRUE(profile.dialogOpen);
    EXPECT_TRUE(profile.replayOpen);
    EXPECT_TRUE(profile.saveOpen);
    EXPECT_FALSE(profile.levelstat);
    EXPECT_FALSE(profile.dosFullscreen);
}

TEST(ProfileFromJson, AnUnknownGameTypeIsNone) {
    EXPECT_EQ(parsed(R"({"multiplayer":{"gameType":7}})").multiplayer.gameType, GameType::None);
    EXPECT_EQ(parsed(R"({"multiplayer":{"gameType":-1}})").multiplayer.gameType, GameType::None);
}

TEST(ProfileFromJson, AnUnknownReplayModeIsOff) {
    EXPECT_EQ(parsed(R"({"replay":{"mode":3}})").replay.mode, ReplayMode::Off);
}

TEST(ProfileFromJson, AnUnknownPlaybackIsAsRecorded) {
    EXPECT_EQ(parsed(R"({"replay":{"playback":9}})").replay.playback, Playback::AsRecorded);
}

TEST(ProfileFromJson, UnknownKeysAreIgnored) {
    const Profile profile = parsed(R"({"id":"a","future":{"x":1},"name":"kept","multiplayer":{"later":true}})");

    EXPECT_EQ(profile.id, "a");
    EXPECT_EQ(profile.name, "kept");
}

TEST(ProfileFromJson, SectionsOfTheWrongTypeLeaveTheirDefaults) {
    const Profile profile = parsed(R"({"multiplayer":[],"replay":"x","save":5})");

    EXPECT_EQ(profile.multiplayer, MultiplayerSettings());
    EXPECT_EQ(profile.replay, ReplaySettings());
    EXPECT_EQ(profile.save, SaveSettings());
}

TEST(ProfileToJson, EveryFieldReadsBackTheSame) {
    const Profile made = full();

    same::profile(parsed(written(made)), made);
}

TEST(ProfileToJson, TheDefaultsReadBackTheSame) {
    Profile made;

    made.id = "defaults";

    same::profile(parsed(written(made)), made);
}

TEST(ProfileToJson, EveryGameTypeReplayModeAndPlaybackReadsBack) {
    for (int stored = 0; stored <= 3; ++stored) {
        Profile made;

        made.id = "x";
        made.multiplayer.gameType = gameTypeOf(stored);
        made.replay.mode = replayModeOf(stored % 3);
        made.replay.playback = playbackOf(stored % 3);

        const Profile back = parsed(written(made));

        EXPECT_EQ(back.multiplayer.gameType, made.multiplayer.gameType) << stored;
        EXPECT_EQ(back.replay.mode, made.replay.mode) << stored;
        EXPECT_EQ(back.replay.playback, made.replay.playback) << stored;
    }
}

TEST(ProfileToJson, ControlCharactersInTextReadBack) {
    Profile made;

    made.id = "x";
    made.extra = "line\nbreak\ttab \\ slash \"quote\"";

    EXPECT_EQ(parsed(written(made)).extra, made.extra);
}

TEST(ProfileClearSettings, KeepsTheIdNameAndConfig) {
    Profile profile = full();

    profile.clearSettings();

    EXPECT_EQ(profile.id, full().id);
    EXPECT_EQ(profile.name, full().name);
    EXPECT_EQ(profile.config, full().config);
}

TEST(ProfileClearSettings, ResetsEverythingElseToTheDefaults) {
    Profile profile = full();

    profile.clearSettings();

    Profile want;

    want.id = profile.id;
    want.name = profile.name;
    want.config = profile.config;

    same::profile(profile, want);
}

TEST(ProfileEnums, StoredValuesMapToTheirEnumerators) {
    EXPECT_EQ(gameTypeOf(0), GameType::None);
    EXPECT_EQ(gameTypeOf(1), GameType::Coop);
    EXPECT_EQ(gameTypeOf(2), GameType::Deathmatch);
    EXPECT_EQ(gameTypeOf(3), GameType::AltDeathmatch);
    EXPECT_EQ(replayModeOf(0), ReplayMode::Off);
    EXPECT_EQ(replayModeOf(1), ReplayMode::Record);
    EXPECT_EQ(replayModeOf(2), ReplayMode::Play);
    EXPECT_EQ(playbackOf(0), Playback::AsRecorded);
    EXPECT_EQ(playbackOf(1), Playback::Timed);
    EXPECT_EQ(playbackOf(2), Playback::Fast);
}

TEST(ProfileEnums, StoredValuesOutOfRangeMapToTheFirst) {
    EXPECT_EQ(gameTypeOf(-1), GameType::None);
    EXPECT_EQ(gameTypeOf(4), GameType::None);
    EXPECT_EQ(replayModeOf(-1), ReplayMode::Off);
    EXPECT_EQ(replayModeOf(3), ReplayMode::Off);
    EXPECT_EQ(playbackOf(-1), Playback::AsRecorded);
    EXPECT_EQ(playbackOf(3), Playback::AsRecorded);
}
