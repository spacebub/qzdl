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
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Json.h"

#include "Same.h"
#include "core/config/Config.h"
#include "core/config/GameplayKeys.h"
#include "core/config/Options.h"
#include "core/config/Schema.h"
#include "core/config/Versions.h"
#include "core/launch/KnownPorts.h"
#include "core/ports/Detect.h"
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

    EXPECT_EQ(Json::obj_get_int(doc.root(), ConfigKey::VERSION), Versions::CURRENT);
    EXPECT_EQ(Json::obj_get_string(doc.root(), ConfigKey::ENGINE), "ZDL4");
    EXPECT_EQ(Json::obj_get_string(doc.root(), ConfigKey::APP_VERSION), QZDL_VERSION);
}

TEST_F(Schema, ADefaultConfigIsTheStampAlone) {
    EXPECT_EQ(keysOf(saved(Config{}).root()), (Keys{ConfigKey::VERSION, ConfigKey::ENGINE, ConfigKey::APP_VERSION}));
}

TEST_F(Schema, AWindowWithNoGeometryIsLeftOut) {
    Config config;

    config.general.autoClose = true;

    const Json::Doc doc = saved(config);

    EXPECT_EQ(keysOf(Json::obj_get(doc.root(), ConfigKey::GENERAL)), (Keys{ConfigKey::AUTO_CLOSE}));
}

TEST_F(Schema, AWindowGeometryIsTwoPairs) {
    Config config;

    config.general.window = {
            .width = 1600,
            .height = 1000,
            .x = -8,
            .y = 80,
            .hasSize = true,
            .hasPosition = true,
            .maximized = false,
    };

    const Json::Doc doc = saved(config);
    const yyjson_val *window = Json::obj_get(Json::obj_get(doc.root(), ConfigKey::GENERAL), ConfigKey::WINDOW);
    int size[2] = {};
    int pos[2] = {};

    ASSERT_TRUE(Json::as_int_array(Json::obj_get(window, ConfigKey::SIZE), size, 2));
    ASSERT_TRUE(Json::as_int_array(Json::obj_get(window, ConfigKey::POS), pos, 2));
    EXPECT_EQ(size[0], 1600);
    EXPECT_EQ(size[1], 1000);
    EXPECT_EQ(pos[0], -8);
    EXPECT_EQ(pos[1], 80);
}

TEST_F(Schema, AMaximizedWindowSurvivesASave) {
    Config config;

    config.general.window.maximized = true;

    config.save(at("again.json"), nullptr);

    Config back;

    ASSERT_TRUE(back.load(at("again.json"), nullptr));
    EXPECT_TRUE(back.general.window.maximized);
}

TEST_F(Schema, AWindowIsNotMaximizedWithoutTheKey) {
    EXPECT_FALSE(loaded(R"({"general":{"window":{"size":[800,600]}}})").general.window.maximized);
}

TEST_F(Schema, AnEngineLeavesOutAnUnsetDosboxFlagAndPortAndManaged) {
    Config config;

    config.iwads.push_back({.name = "Doom", .file = "/g/doom.wad"});
    config.engines.push_back({.name = "Dos", .file = "/p/doom.exe", .dosbox = true});

    const Json::Doc doc = saved(config);

    EXPECT_EQ(keysOf(yyjson_arr_get_first(Json::obj_get(doc.root(), ConfigKey::IWADS))),
              (Keys{ConfigKey::NAME, ConfigKey::FILE}));
    EXPECT_EQ(keysOf(yyjson_arr_get_first(Json::obj_get(doc.root(), ConfigKey::PORTS))),
              (Keys{ConfigKey::NAME, ConfigKey::FILE, ConfigKey::DOSBOX}));
}

TEST_F(Schema, AManagedEngineSpellsItsPortAndTheFlag) {
    Config config;

    config.engines.push_back({
            .name = "Woof!",
            .file = "/p/woof",
            .port = Port::Port::woof,
            .managed = true,
    });

    const Json::Doc doc = saved(config);
    yyjson_val *entry = yyjson_arr_get_first(Json::obj_get(doc.root(), ConfigKey::PORTS));

    EXPECT_EQ(keysOf(entry), (Keys{ConfigKey::NAME, ConfigKey::FILE, ConfigKey::PROGRAM, ConfigKey::MANAGED}));
    EXPECT_EQ(Json::as_string(Json::obj_get(entry, ConfigKey::PROGRAM)), "woof");
    EXPECT_TRUE(Json::as_bool(Json::obj_get(entry, ConfigKey::MANAGED)));
}

TEST_F(Schema, AManagedFlagWithoutAKnownPortIsDropped) {
    const Config config = loaded(R"({"version":9,"ports":[{"name":"Q","file":"/p/q","program":"quake","managed":true},)"
                                 R"({"name":"N","file":"/p/n","managed":true}]})");

    ASSERT_EQ(config.engines.size(), 2U);
    EXPECT_FALSE(config.engines[0].managed);
    EXPECT_FALSE(config.engines[1].managed);
}

TEST_F(Schema, AManagedFlagOnAPortOffTheCatalogIsDropped) {
    const Config config = loaded(
            R"({"version":9,"ports":[{"name":"Nyan","file":"/p/nyan-doom","program":"nyan-doom","managed":true},)"
            R"({"name":"Cherry","file":"/p/cherry-doom","program":"cherry-doom","managed":true},)"
            R"({"name":"Woof","file":"/p/woof","program":"woof","managed":true}]})");

    ASSERT_EQ(config.engines.size(), 3U);
    EXPECT_EQ(config.engines[0].port, Port::Port::nyan);
    EXPECT_FALSE(config.engines[0].managed);
    EXPECT_EQ(config.engines[1].port, Port::Port::cherry);
    EXPECT_FALSE(config.engines[1].managed);
    EXPECT_TRUE(config.engines[2].managed);
}

TEST_F(Schema, AnUnknownPortWordReadsAsNothingPicked) {
    const Config config = loaded(R"({"version":9,"ports":[{"name":"Mine","file":"/p/engine","program":"quake"}]})");

    ASSERT_EQ(config.engines.size(), 1U);
    EXPECT_EQ(config.engines.front().port, Port::Port::unknown);
    EXPECT_FALSE(config.engines.front().managed);
}

TEST_F(Schema, AnEngineSpellsAPickedPort) {
    Config config;

    config.engines.push_back({.name = "Mine", .file = "/p/engine", .port = Port::Port::woof});

    const Json::Doc doc = saved(config);
    yyjson_val *entry = yyjson_arr_get_first(Json::obj_get(doc.root(), ConfigKey::PORTS));

    EXPECT_EQ(keysOf(entry), (Keys{ConfigKey::NAME, ConfigKey::FILE, ConfigKey::PROGRAM}));
    EXPECT_EQ(Json::as_string(Json::obj_get(entry, ConfigKey::PROGRAM)), "woof");
}

TEST_F(Schema, APortSpellsAPickedVersion) {
    Config config;

    config.engines.push_back({.name = "Mine", .file = "/p/dsda-doom", .version = "0.25.0"});

    const Json::Doc doc = saved(config);
    yyjson_val *entry = yyjson_arr_get_first(Json::obj_get(doc.root(), ConfigKey::PORTS));

    EXPECT_EQ(keysOf(entry), (Keys{ConfigKey::NAME, ConfigKey::FILE, ConfigKey::PORT_VERSION}));
    EXPECT_EQ(Json::as_string(Json::obj_get(entry, ConfigKey::PORT_VERSION)), "0.25.0");
}

TEST_F(Schema, AVersionThatIsNotTextLoadsAsNone) {
    EXPECT_TRUE(loaded(R"({"ports":[{"name":"A","file":"/p/a","version":27}]})").engines.front().version.empty());
}

TEST_F(Schema, ADefaultProfileIsItsIdNameAndConfig) {
    Config config;

    config.addProfile("x");

    const Json::Doc doc = saved(config);

    EXPECT_EQ(keysOf(yyjson_arr_get_first(Json::obj_get(doc.root(), ConfigKey::PROFILES))),
              (Keys{ProfileKey::ID, ProfileKey::NAME, ProfileKey::CONFIG}));
}

TEST_F(Schema, AnEnabledFileIsItsPathAlone) {
    Config config;

    config.addProfile("x");
    config.profiles.front().files.push_back({.file = "/m/a.wad"});

    const Json::Doc doc = saved(config);
    const yyjson_val *profile = yyjson_arr_get_first(Json::obj_get(doc.root(), ConfigKey::PROFILES));

    EXPECT_EQ(keysOf(yyjson_arr_get_first(Json::obj_get(profile, ProfileKey::FILES))), (Keys{ProfileKey::FILE}));
}

TEST_F(Schema, AFileIsAnObjectWithItsFlag) {
    Config config;

    config.addProfile("x");
    config.profiles.front().files.push_back({.file = "/m/a.wad", .enabled = false});

    const Json::Doc doc = saved(config);
    const yyjson_val *profile = yyjson_arr_get_first(Json::obj_get(doc.root(), ConfigKey::PROFILES));
    yyjson_val *file = yyjson_arr_get_first(Json::obj_get(profile, ProfileKey::FILES));

    EXPECT_EQ(keysOf(file), (Keys{ProfileKey::FILE, ProfileKey::ENABLED}));
    EXPECT_TRUE(yyjson_is_false(Json::obj_get(file, ProfileKey::ENABLED)));
}

TEST_F(Schema, TheStartViewIsSpelledAsAWord) {
    Config config;

    config.general.startView = StartView::Games;

    EXPECT_EQ(Json::obj_get_string(Json::obj_get(saved(config).root(), ConfigKey::GENERAL), ConfigKey::START_VIEW),
              StartViewText::GAMES);
}

TEST_F(Schema, TheFileIsIndentedByTwoSpacesAndEndsInANewline) {
    Config config;

    config.general.alwaysAdd = "-x";
    config.general.autoClose = true;

    ASSERT_TRUE(config.save(at("pretty.json")));

    const std::string text = same::slurp(at("pretty.json"));

    EXPECT_TRUE(
            text.starts_with("{\n  \"version\": " + std::to_string(Versions::CURRENT) + ",\n  \"engine\": \"ZDL4\",\n"))
            << text;
    EXPECT_NE(text.find("\n    \"alwaysAdd\": \"-x\",\n"), std::string::npos) << text;
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

TEST_F(Schema, AMoveAtTheNextStartIsWrittenOnlyWhenSet) {
    Config config;

    EXPECT_EQ(Json::obj_get(Json::obj_get(saved(config).root(), ConfigKey::GENERAL), ConfigKey::MOVE_AT_NEXT_START),
              nullptr);

    config.general.moveAtNextStart = true;

    EXPECT_TRUE(Json::as_bool(
            Json::obj_get(Json::obj_get(saved(config).root(), ConfigKey::GENERAL), ConfigKey::MOVE_AT_NEXT_START)));
    EXPECT_TRUE(loaded(R"({"general":{"moveAtNextStart":true}})").general.moveAtNextStart);
}

TEST_F(Schema, WrongTypesInGeneralTakeTheirDefaults) {
    const Config config = loaded(R"({"general":{"showPaths":null,"theme":5,"startView":3,)"
                                 R"("lastDirs":[],"window":"big","alwaysAdd":false}})");

    EXPECT_TRUE(config.general.showPaths);
    EXPECT_EQ(config.general.theme, ThemeMode::SYSTEM);
    EXPECT_EQ(config.general.startView, StartView::Profiles);
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

TEST_F(Schema, ADetectedEngineSpellsTheFlag) {
    Config config;

    config.engines.push_back(
            Detect::engine({.port = Port::Port::woof, .name = "Woof!", .file = "/p/woof", .dos = false}));

    const Json::Doc doc = saved(config);
    yyjson_val *entry = yyjson_arr_get_first(Json::obj_get(doc.root(), ConfigKey::PORTS));

    EXPECT_EQ(keysOf(entry), (Keys{ConfigKey::NAME, ConfigKey::FILE, ConfigKey::PROGRAM, ConfigKey::DETECTED}));
    EXPECT_TRUE(Json::as_bool(Json::obj_get(entry, ConfigKey::DETECTED)));
}

TEST_F(Schema, ADetectedEngineReadsBackDetected) {
    const Config config = loaded(R"({"ports":[{"name":"W","file":"/p/woof","detected":true},)"
                                 R"({"name":"M","file":"/p/mine"},{"name":"F","file":"/p/f","detected":false}]})");

    ASSERT_EQ(config.engines.size(), 3U);
    EXPECT_TRUE(config.engines[0].detected);
    EXPECT_FALSE(config.engines[1].detected);
    EXPECT_FALSE(config.engines[2].detected);
}

TEST_F(Schema, GeneralHoldsNoDetectedList) {
    const Json::Doc doc = saved(tests::Fixtures::config(4, 4));
    yyjson_val *general = Json::obj_get(doc.root(), ConfigKey::GENERAL);

    ASSERT_NE(general, nullptr);
    EXPECT_EQ(Json::obj_get(general, ConfigKey::DETECTED), nullptr);
}

TEST_F(Schema, AnEntryWithoutAFileIsDropped) {
    const Config config = loaded(R"({"iwads":[{"name":"Nameless file"},{"file":"/g/x.wad"},7],)"
                                 R"("ports":[{"name":"P","file":""},{"name":"Q","file":"/p/q","dosbox":1}]})");

    ASSERT_EQ(config.iwads.size(), 1U);
    EXPECT_EQ(config.iwads.front().file, "/g/x.wad");
    EXPECT_TRUE(config.iwads.front().name.empty());
    ASSERT_EQ(config.engines.size(), 1U);
    EXPECT_EQ(config.engines.front().name, "Q");
    EXPECT_TRUE(config.engines.front().dosbox);
}

TEST_F(Schema, ListsOfTheWrongTypeAreEmpty) {
    const Config config = loaded(R"({"iwads":{"a":1},"ports":"x","profiles":{"id":"a"},"general":[]})");

    EXPECT_TRUE(config.iwads.empty());
    EXPECT_TRUE(config.engines.empty());
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

// The disk spelling of each key, so a renamed or swapped one shows here before a config reads wrong.
TEST(SchemaKeys, EveryFlagWordAndRuleKeepsItsKey) {
    const std::vector<std::pair<Flags::Word, std::string_view>> words = {
            {Flags::Word::dmflags, "dmflags"},
            {Flags::Word::dmflags2, "dmflags2"},
            {Flags::Word::dmflags3, "dmflags3"},
            {Flags::Word::zadmflags, "zadmflags"},
            {Flags::Word::lmsWeapons, "lmsallowedweapons"},
            {Flags::Word::lmsSpectators, "lmsspectatorsettings"},
            {Flags::Word::compatflags, "compatflags"},
            {Flags::Word::compatflags2, "compatflags2"},
            {Flags::Word::zacompatflags, "zacompatflags"},
    };
    const std::vector<std::pair<Dialect::Feature, std::string_view>> rules = {
            {Dialect::Feature::coopSpawns, "coop_spawns"},
            {Dialect::Feature::coopPreset, "coop"},
            {Dialect::Feature::alwaysApplyDmflags, "always_apply_dmflags"},
            {Dialect::Feature::chainEpisodes, "chain_episodes"},
            {Dialect::Feature::doubleAmmo, "double_ammo"},
            {Dialect::Feature::noCheats, "no_cheats"},
            {Dialect::Feature::noOptions, "no_options"},
            {Dialect::Feature::limitRemoving, "limit_removing"},
            {Dialect::Feature::vanillaMode, "vanilla"},
            {Dialect::Feature::mbfBeta, "beta"},
            {Dialect::Feature::stroller, "stroller"},
            {Dialect::Feature::flipLevels, "flip_levels"},
            {Dialect::Feature::flipWeapons, "flip_weapons"},
            {Dialect::Feature::turbo, "turbo"},
            {Dialect::Feature::gameSpeed, "speed"},
            {Dialect::Feature::dogs, "dogs"},
    };

    std::size_t kept = 0;

    for (const Options::Option &each : Options::ALL) {
        kept += each.place == Options::Place::rules ? 1 : 0;
    }

    ASSERT_EQ(words.size(), Flags::WORDS);
    ASSERT_EQ(rules.size(), kept);

    for (const auto &[word, spelled] : words) {
        EXPECT_EQ(Flags::name(word), spelled);
    }

    for (const auto &[feature, spelled] : rules) {
        const std::size_t at = Options::indexOf(feature);

        ASSERT_LT(at, Options::ALL.size()) << spelled;
        ASSERT_NE(Options::ALL[at].key, nullptr) << spelled;
        EXPECT_EQ(std::string_view(Options::ALL[at].key), spelled);
        EXPECT_EQ(std::string_view(Options::ALL[at].iniKey), spelled);
    }
}
