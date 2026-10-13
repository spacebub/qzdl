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
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Json.h"

#include "Same.h"
#include "core/config/Config.h"
#include "core/config/Schema.h"
#include "support/Fixtures.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;
namespace Json = ttk::Json;

using Keys = std::vector<std::string>;

Keys keysOf(yyjson_val *obj) {
    Keys out;

    Json::each_field(obj, [&out](const std::string_view key, yyjson_val *) { out.emplace_back(key); });

    return out;
}

class Schema : public ::testing::Test {
protected:
    void SetUp() override { _dir = tests::Sandbox::scratch("schema"); }

    [[nodiscard]] fs::path at(const char *name) const { return _dir / name; }

    Json::Doc saved(const Config &config) {
        const fs::path path = at("saved.json");
        std::string error;

        EXPECT_TRUE(config.save(path, &error)) << error;

        Json::Doc doc = Json::read_file(path, &error);

        EXPECT_TRUE(doc.valid()) << error;

        return doc;
    }

    Config loaded(const std::string_view text) {
        const fs::path path = at("hand.json");

        same::spill(path, text);

        Config config;
        std::string error;

        EXPECT_TRUE(config.load(path, &error)) << error;

        return config;
    }

private:
    fs::path _dir;
};

}

TEST_F(Schema, TheRootHoldsItsKeysInOrder) {
    const Json::Doc doc = saved(tests::Fixtures::config(1, 1));

    EXPECT_EQ(keysOf(doc.root()),
              (Keys{ConfigKey::VERSION,
                    ConfigKey::ENGINE,
                    ConfigKey::APP_VERSION,
                    ConfigKey::GENERAL,
                    ConfigKey::IWADS,
                    ConfigKey::PORTS,
                    ConfigKey::ACTIVE_PROFILE,
                    ConfigKey::PROFILES}));
}

TEST_F(Schema, TheVersionEngineAndAppVersionAreStamped) {
    const Json::Doc doc = saved(Config{});

    EXPECT_EQ(Json::obj_get_int(doc.root(), ConfigKey::VERSION), Config::SCHEMA_VERSION);
    EXPECT_EQ(Json::obj_get_string(doc.root(), ConfigKey::ENGINE), "ZDL4");
    EXPECT_EQ(Json::obj_get_string(doc.root(), ConfigKey::APP_VERSION), QZDL_VERSION);
}

TEST_F(Schema, GeneralSpellsEverySetting) {
    const Json::Doc doc = saved(Config{});

    EXPECT_EQ(keysOf(Json::obj_get(doc.root(), ConfigKey::GENERAL)),
              (Keys{ConfigKey::ALWAYS_ADD,
                    ConfigKey::DOSBOX,
                    ConfigKey::AUTO_CLOSE,
                    ConfigKey::LAUNCH_ZDL_IMMEDIATELY,
                    ConfigKey::SHOW_PATHS,
                    ConfigKey::NO_USER_CONF,
                    ConfigKey::SHOW_HIDDEN,
                    ConfigKey::PROFILE_CONFIGS,
                    ConfigKey::START_VIEW,
                    ConfigKey::GAME_PORT,
                    ConfigKey::THEME,
                    ConfigKey::IS_IMPORTED,
                    ConfigKey::IMPORTED_FROM,
                    ConfigKey::IMPORT_DATE,
                    ConfigKey::WINDOW,
                    ConfigKey::LAST_DIRS,
                    ConfigKey::DETECTED}));
}

TEST_F(Schema, LastDirsSpellsEveryDirectory) {
    const Json::Doc doc = saved(Config{});
    yyjson_val *general = Json::obj_get(doc.root(), ConfigKey::GENERAL);

    EXPECT_EQ(keysOf(Json::obj_get(general, ConfigKey::LAST_DIRS)),
              (Keys{ConfigKey::GENERAL,
                    ConfigKey::WAD,
                    ConfigKey::SRC,
                    ConfigKey::SAVE,
                    ConfigKey::ZDL,
                    ConfigKey::CONFIG,
                    ConfigKey::REPLAY}));
}

TEST_F(Schema, AWindowWithNoGeometryIsAnEmptyObject) {
    const Json::Doc doc = saved(Config{});
    yyjson_val *window = Json::obj_get(Json::obj_get(doc.root(), ConfigKey::GENERAL), ConfigKey::WINDOW);

    ASSERT_TRUE(yyjson_is_obj(window));
    EXPECT_TRUE(keysOf(window).empty());
}

TEST_F(Schema, AWindowGeometryIsTwoPairs) {
    Config config;

    config.general.window = {.width = 1600, .height = 1000, .x = -8, .y = 80, .hasSize = true, .hasPosition = true};

    const Json::Doc doc = saved(config);
    yyjson_val *window = Json::obj_get(Json::obj_get(doc.root(), ConfigKey::GENERAL), ConfigKey::WINDOW);
    int size[2] = {};
    int pos[2] = {};

    ASSERT_TRUE(Json::as_int_array(Json::obj_get(window, ConfigKey::SIZE), size, 2));
    ASSERT_TRUE(Json::as_int_array(Json::obj_get(window, ConfigKey::POS), pos, 2));
    EXPECT_EQ(size[0], 1600);
    EXPECT_EQ(size[1], 1000);
    EXPECT_EQ(pos[0], -8);
    EXPECT_EQ(pos[1], 80);
}

TEST_F(Schema, AnEntryLeavesOutAnUnsetDosboxFlagAndPortId) {
    Config config;

    config.iwads.push_back({.name = "Doom", .file = "/g/doom.wad"});
    config.ports.push_back({.name = "Dos", .file = "/p/doom.exe", .dosbox = true, .portId = "doom-dos"});

    const Json::Doc doc = saved(config);

    EXPECT_EQ(keysOf(yyjson_arr_get_first(Json::obj_get(doc.root(), ConfigKey::IWADS))),
              (Keys{ConfigKey::NAME, ConfigKey::FILE}));
    EXPECT_EQ(keysOf(yyjson_arr_get_first(Json::obj_get(doc.root(), ConfigKey::PORTS))),
              (Keys{ConfigKey::NAME, ConfigKey::FILE, ConfigKey::DOSBOX, ConfigKey::PORT_ID}));
}

TEST_F(Schema, AProfileSpellsEveryField) {
    Config config;

    config.addProfile("x");

    const Json::Doc doc = saved(config);
    yyjson_val *profile = yyjson_arr_get_first(Json::obj_get(doc.root(), ConfigKey::PROFILES));

    EXPECT_EQ(keysOf(profile),
              (Keys{ProfileKey::ID,
                    ProfileKey::NAME,
                    ProfileKey::IWAD,
                    ProfileKey::PORT,
                    ProfileKey::FILES,
                    ProfileKey::SKILL,
                    ProfileKey::MONSTERS,
                    ProfileKey::WARP,
                    ProfileKey::EXTRA,
                    ProfileKey::DIALOG_OPEN,
                    ProfileKey::REPLAY_OPEN,
                    ProfileKey::SAVE_OPEN,
                    ProfileKey::CONFIG,
                    ProfileKey::SHARED_CONFIG,
                    ProfileKey::CUSTOM_COMMAND,
                    ProfileKey::COMMAND,
                    ProfileKey::DOS_FULLSCREEN,
                    ProfileKey::DOS_EXIT,
                    ProfileKey::CAPTURE_OUTPUT,
                    ProfileKey::LEVELSTAT,
                    ProfileKey::MULTIPLAYER,
                    ProfileKey::REPLAY,
                    ProfileKey::SAVE}));
    EXPECT_EQ(keysOf(Json::obj_get(profile, ProfileKey::MULTIPLAYER)),
              (Keys{ProfileKey::GAME_TYPE,
                    ProfileKey::PLAYERS,
                    ProfileKey::EXTRATIC,
                    ProfileKey::NETMODE,
                    ProfileKey::DUP,
                    ProfileKey::HOST,
                    ProfileKey::PORT,
                    ProfileKey::FRAG_LIMIT,
                    ProfileKey::TIME_LIMIT,
                    ProfileKey::DMFLAGS,
                    ProfileKey::DMFLAGS2,
                    ProfileKey::SAVEGAME,
                    ProfileKey::LISTED}));
    EXPECT_EQ(keysOf(Json::obj_get(profile, ProfileKey::REPLAY)),
              (Keys{ProfileKey::MODE,
                    ProfileKey::FILE,
                    ProfileKey::PLAYBACK,
                    ProfileKey::COMPATIBILITY,
                    ProfileKey::LONGTICS,
                    ProfileKey::SOLO_NET}));
    EXPECT_EQ(keysOf(Json::obj_get(profile, ProfileKey::SAVE)), (Keys{ProfileKey::ENABLED, ProfileKey::FILE}));
}

TEST_F(Schema, AFileIsAnObjectWithItsFlag) {
    Config config;

    config.addProfile("x");
    config.profiles.front().files.push_back({.file = "/m/a.wad", .enabled = false});

    const Json::Doc doc = saved(config);
    yyjson_val *profile = yyjson_arr_get_first(Json::obj_get(doc.root(), ConfigKey::PROFILES));
    yyjson_val *file = yyjson_arr_get_first(Json::obj_get(profile, ProfileKey::FILES));

    EXPECT_EQ(keysOf(file), (Keys{ProfileKey::FILE, ProfileKey::ENABLED}));
    EXPECT_TRUE(yyjson_is_false(Json::obj_get(file, ProfileKey::ENABLED)));
}

TEST_F(Schema, TheStartViewIsSpelledAsAWord) {
    Config config;

    EXPECT_EQ(Json::obj_get_string(Json::obj_get(saved(config).root(), ConfigKey::GENERAL), ConfigKey::START_VIEW),
              StartViewText::PROFILES);

    config.general.startView = StartView::Games;

    EXPECT_EQ(Json::obj_get_string(Json::obj_get(saved(config).root(), ConfigKey::GENERAL), ConfigKey::START_VIEW),
              StartViewText::GAMES);
}

TEST_F(Schema, TheFileIsIndentedByTwoSpacesAndEndsInANewline) {
    ASSERT_TRUE(Config{}.save(at("pretty.json")));

    const std::string text = same::slurp(at("pretty.json"));

    EXPECT_TRUE(text.starts_with("{\n  \"version\": 1,\n  \"engine\": \"ZDL4\",\n")) << text;
    EXPECT_NE(text.find("\n    \"alwaysAdd\": \"\",\n"), std::string::npos) << text;
    EXPECT_TRUE(text.ends_with("}\n")) << text;
}

TEST_F(Schema, AConfigWithoutAVersionLoads) {
    const Config config =
            loaded(R"({"iwads":[{"name":"Doom","file":"/g/doom.wad"}],"profiles":[{"id":"a","name":"A"}]})");

    ASSERT_EQ(config.iwads.size(), 1U);
    ASSERT_EQ(config.profiles.size(), 1U);
    EXPECT_EQ(config.activeProfileId, "a");
}

TEST_F(Schema, AnEmptyObjectLoadsAsTheDefaults) {
    const Config config = loaded("{}");

    same::config(config, Config{});
}

TEST_F(Schema, MissingGeneralKeysTakeTheirDefaults) {
    const Config config = loaded(R"({"general":{"alwaysAdd":"-x"}})");

    EXPECT_EQ(config.general.alwaysAdd, "-x");
    EXPECT_TRUE(config.general.showPaths);
    EXPECT_EQ(config.general.theme, ThemeMode::SYSTEM);
    EXPECT_EQ(config.general.startView, StartView::Profiles);
    EXPECT_FALSE(config.general.noUserConf);
    EXPECT_FALSE(config.general.window.hasSize);
}

TEST_F(Schema, WrongTypesInGeneralTakeTheirDefaults) {
    const Config config = loaded(R"({"general":{"showPaths":null,"theme":5,"startView":3,"detected":"x",)"
                                 R"("lastDirs":[],"window":"big","alwaysAdd":false}})");

    EXPECT_TRUE(config.general.showPaths);
    EXPECT_EQ(config.general.theme, ThemeMode::SYSTEM);
    EXPECT_EQ(config.general.startView, StartView::Profiles);
    EXPECT_TRUE(config.general.detected.empty());
    EXPECT_TRUE(config.general.lastDirs.general.empty());
    EXPECT_FALSE(config.general.window.hasSize);
    EXPECT_TRUE(config.general.alwaysAdd.empty());
}

TEST_F(Schema, AnUnknownStartViewIsProfiles) {
    EXPECT_EQ(loaded(R"({"general":{"startView":"shelf"}})").general.startView, StartView::Profiles);
    EXPECT_EQ(loaded(R"({"general":{"startView":"games"}})").general.startView, StartView::Games);
}

TEST_F(Schema, AWindowPairTooShortOrNotNumbersIsIgnored) {
    const Config shortPair = loaded(R"({"general":{"window":{"size":[800],"pos":["a","b"]}}})");

    EXPECT_FALSE(shortPair.general.window.hasSize);
    EXPECT_FALSE(shortPair.general.window.hasPosition);
}

TEST_F(Schema, DetectedKeepsOnlyText) {
    EXPECT_EQ(loaded(R"({"general":{"detected":["/a",1,null,"/b"]}})").general.detected,
              (std::vector<std::string>{"/a", "/b"}));
}

TEST_F(Schema, AnEntryWithoutAFileIsDropped) {
    const Config config = loaded(R"({"iwads":[{"name":"Nameless file"},{"file":"/g/x.wad"},7],)"
                                 R"("ports":[{"name":"P","file":""},{"name":"Q","file":"/p/q","dosbox":1}]})");

    ASSERT_EQ(config.iwads.size(), 1U);
    EXPECT_EQ(config.iwads.front().file, "/g/x.wad");
    EXPECT_TRUE(config.iwads.front().name.empty());
    ASSERT_EQ(config.ports.size(), 1U);
    EXPECT_EQ(config.ports.front().name, "Q");
    EXPECT_TRUE(config.ports.front().dosbox);
}

TEST_F(Schema, ListsOfTheWrongTypeAreEmpty) {
    const Config config = loaded(R"({"iwads":{"a":1},"ports":"x","profiles":{"id":"a"},"general":[]})");

    EXPECT_TRUE(config.iwads.empty());
    EXPECT_TRUE(config.ports.empty());
    EXPECT_TRUE(config.profiles.empty());
    EXPECT_TRUE(config.activeProfileId.empty());
}

TEST_F(Schema, UnknownKeysAreIgnored) {
    const Config config = loaded(R"({"version":99,"future":{"nested":[1,2]},"profiles":[{"id":"a"}]})");

    ASSERT_EQ(config.profiles.size(), 1U);
    EXPECT_EQ(config.profiles.front().id, "a");
}

TEST_F(Schema, AnActiveProfileThatIsNotTextFallsToTheFirst) {
    EXPECT_EQ(loaded(R"({"activeProfile":3,"profiles":[{"id":"a"},{"id":"b"}]})").activeProfileId, "a");
}
