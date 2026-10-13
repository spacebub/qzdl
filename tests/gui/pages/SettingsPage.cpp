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

#include "ttk/notices/Notice.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/controls/Toggle.h"
#include "ttk/toolkit/layout/Scroll.h"

#include "Pages.h"
#include "core/config/ConfigPaths.h"
#include "core/config/Session.h"
#include "gui/pages/SettingsPage.h"
#include "gui/state/State.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

constexpr const char *CLOSING = "Quit ZDL4 as soon as";
constexpr const char *PATHS = "Show the directory a file came from";
constexpr const char *AT_ONCE = "A .zdl given on the command line";
constexpr const char *PER_PROFILE = "Each profile keeps the source port's settings";
constexpr const char *SKIP_USER = "ZDL4 loads a portable config";

class SettingsPageTest : public paging::Paged {
protected:
    void SetUp() override {
        paging::Paged::SetUp();

        std::error_code code;

        fs::remove_all(ConfigPaths::get().configPath(ConfigPaths::USER), code);
    }

    void TearDown() override {
        std::error_code code;

        fs::remove_all(ConfigPaths::get().configPath(ConfigPaths::USER), code);

        paging::Paged::TearDown();
    }

    pages::SettingsPage &page() { return rig().settings(); }

    void open(Config what) {
        load(std::move(what));
        show(State::Page::Settings);
    }

    ttk::Field *field(const size_t at) {
        const std::vector<ttk::Field *> fields = paging::all<ttk::Field>(&page());

        return at < fields.size() ? fields[at] : nullptr;
    }

    ttk::Field *always() { return field(0); }
    ttk::Field *dosbox() { return field(1); }

    ttk::Toggle *toggle(const std::string &hint) { return paging::hinted<ttk::Toggle>(&page(), hint); }

    void flip(const std::string &hint) {
        ttk::Toggle *found = toggle(hint);

        ASSERT_NE(found, nullptr) << hint;

        rig().ui().focus(found);
        rig().ui().key(ttk::Key{.code = ttk::Code::Space});
    }

    void type(ttk::Field *into, const std::string &text) {
        rig().ui().focus(into->input());
        rig().ui().wrote(text);
    }

    ttk::Button *button(const std::string &hint) { return paging::hinted<ttk::Button>(&page(), hint); }

    // Pressed through the keyboard, since lower panels may be scrolled out of reach.
    void press(ttk::Button *which) {
        ASSERT_NE(which, nullptr);

        rig().ui().focus(which);
        rig().ui().key(ttk::Key{.code = ttk::Code::Return});
    }

    ttk::Button *adopt() {
        for (ttk::Button *each : paging::all<ttk::Button>(&page())) {
            if (each->hint.ends_with("ZDL4 opens by default")) {
                return each;
            }
        }

        return nullptr;
    }

    ttk::Button *clearDownloads() {
        for (ttk::Button *each : paging::all<ttk::Button>(&page())) {
            if (each->hint == "There is nothing being kept" || each->hint.starts_with("Delete what was downloaded")) {
                return each;
            }
        }

        return nullptr;
    }
};

TEST_F(SettingsPageTest, ItIsNotBuiltBeforeItIsVisited) {
    load(paging::world(1));

    EXPECT_FALSE(rig().hasSettings());
}

TEST_F(SettingsPageTest, AVisitBuildsAndShowsIt) {
    open(paging::world(1));

    ASSERT_TRUE(rig().hasSettings());
    EXPECT_TRUE(page().visible());
    EXPECT_TRUE(paging::shows(&page(), "Settings"));
}

TEST_F(SettingsPageTest, TheArgumentsAlwaysAddedAreShown) {
    Config made = paging::world(1);

    made.general.alwaysAdd = "-nomusic";
    open(made);

    EXPECT_EQ(always()->text(), "-nomusic");
}

TEST_F(SettingsPageTest, TypingArgumentsWritesThem) {
    open(paging::world(1));

    type(always(), "-nosound");

    EXPECT_EQ(config().general.alwaysAdd, "-nosound");
}

TEST_F(SettingsPageTest, ArgumentsSetElsewhereAreShown) {
    open(paging::world(1));

    rig().config().settings().setAlwaysAdd("-fast");
    rig().turn();

    EXPECT_EQ(always()->text(), "-fast");
}

TEST_F(SettingsPageTest, TheDosboxSetIsShown) {
    Config made = paging::world(1);

    made.general.dosbox = "/opt/dosbox";
    open(made);

    EXPECT_EQ(dosbox()->text(), "/opt/dosbox");
}

TEST_F(SettingsPageTest, WithNoDosboxSetTheOneFoundIsShown) {
    open(paging::world(1));

    EXPECT_EQ(dosbox()->text(), cfg().systemDosbox);
}

TEST_F(SettingsPageTest, TypingADosboxWritesIt) {
    open(paging::world(1));

    rig().ui().focus(dosbox()->input());
    rig().ui().key(ttk::Key{.code = ttk::Code::Home, .ctrl = false, .shift = false});
    rig().ui().wrote("/opt/dosbox");

    EXPECT_TRUE(config().general.dosbox.starts_with("/opt/dosbox"));
}

TEST_F(SettingsPageTest, BrowsingForDosboxOpensThePicker) {
    open(paging::world(1));

    ttk::Widget *browse = nullptr;

    for (ttk::Widget *each : paging::all<ttk::Widget>(dosbox())) {
        if (each->hint == "Browse") {
            browse = each;
        }
    }

    ASSERT_NE(browse, nullptr);

    paging::click(rig().ui(), browse);

    EXPECT_TRUE(rig().files().state().open);
    EXPECT_EQ(rig().files().state().title, "Select DOSBox");

    rig().files().choose({"/opt/dosbox-staging"});

    EXPECT_EQ(config().general.dosbox, "/opt/dosbox-staging");
}

TEST_F(SettingsPageTest, TheSwitchesShowTheSettings) {
    Config made = paging::world(1);

    made.general.autoClose = true;
    made.general.showPaths = false;
    made.general.launchZdlImmediately = true;
    made.general.profileConfigs = true;
    open(made);

    EXPECT_TRUE(toggle(CLOSING)->checked);
    EXPECT_FALSE(toggle(PATHS)->checked);
    EXPECT_TRUE(toggle(AT_ONCE)->checked);
    EXPECT_TRUE(toggle(PER_PROFILE)->checked);
    EXPECT_FALSE(toggle(SKIP_USER)->checked);
}

TEST_F(SettingsPageTest, FlippingCloseOnLaunchWritesIt) {
    open(paging::world(1));

    flip(CLOSING);
    rig().turn();

    EXPECT_TRUE(config().general.autoClose);
    EXPECT_TRUE(toggle(CLOSING)->checked);
}

TEST_F(SettingsPageTest, FlippingShowPathsWritesIt) {
    Config made = paging::world(1);

    made.general.showPaths = true;
    open(made);

    flip(PATHS);

    EXPECT_FALSE(config().general.showPaths);
}

TEST_F(SettingsPageTest, FlippingLaunchAtOnceWritesIt) {
    open(paging::world(1));

    flip(AT_ONCE);

    EXPECT_TRUE(config().general.launchZdlImmediately);
}

TEST_F(SettingsPageTest, FlippingPerProfileConfigsWritesIt) {
    open(paging::world(1));

    flip(PER_PROFILE);

    EXPECT_TRUE(config().general.profileConfigs);
}

TEST_F(SettingsPageTest, FlippingSkipTheUserConfigWritesTheUserConfig) {
    open(paging::world(1));

    flip(SKIP_USER);
    rig().turn();

    EXPECT_TRUE(cfg().ignoreUserConfig);
    EXPECT_TRUE(toggle(SKIP_USER)->checked);
    EXPECT_TRUE(fs::exists(ConfigPaths::get().configPath(ConfigPaths::USER)));
}

TEST_F(SettingsPageTest, TheStartViewSwitchShowsTheSetting) {
    Config made = paging::world(1);

    made.general.startView = StartView::Games;
    open(made);

    const std::vector<ttk::MultistateSwitch *> switches = paging::all<ttk::MultistateSwitch>(&page());

    ASSERT_EQ(switches.size(), 1U);
    EXPECT_EQ(switches.front()->current(), static_cast<int>(StartView::Games));
}

TEST_F(SettingsPageTest, TheStartViewSwitchWritesTheSetting) {
    open(paging::world(1));

    paging::all<ttk::MultistateSwitch>(&page()).front()->key(ttk::Key{.code = ttk::Code::Right});

    EXPECT_EQ(config().general.startView, StartView::Games);
}

TEST_F(SettingsPageTest, TheConfigFileIsNamed) {
    open(paging::world(1));

    EXPECT_TRUE(paging::shows(&page(), cfg().path));
}

TEST_F(SettingsPageTest, AConfigElsewhereCanBeAdopted) {
    open(paging::world(1));

    ASSERT_NE(adopt(), nullptr);
    EXPECT_TRUE(adopt()->enabled());
    EXPECT_EQ(adopt()->hint, "Make this the one ZDL4 opens by default");
}

TEST_F(SettingsPageTest, AdoptingAsksFirstNamingThePath) {
    open(paging::world(1));

    press(adopt());

    ASSERT_EQ(rig().asks.size(), 1U);
    EXPECT_EQ(rig().asks.front().title, "Use this as the user config?");
    EXPECT_FALSE(rig().asks.front().danger);
}

TEST_F(SettingsPageTest, AnsweringAdoptMakesItTheUserConfig) {
    open(paging::world(1));

    press(adopt());
    rig().asks.front().accepted();
    rig().turn();

    EXPECT_TRUE(cfg().userConfig);
    EXPECT_FALSE(adopt()->enabled());
    EXPECT_EQ(adopt()->hint, "This is already the one ZDL4 opens by default");
}

TEST_F(SettingsPageTest, ClearingAsksFirstAsADanger) {
    open(paging::world(2));

    press(button("Empties this config"));

    ASSERT_EQ(rig().asks.size(), 1U);
    EXPECT_EQ(rig().asks.front().title, "Clear everything?");
    EXPECT_TRUE(rig().asks.front().danger);
    EXPECT_EQ(config().profiles.size(), 2U);
}

TEST_F(SettingsPageTest, AnsweringClearEmptiesTheConfig) {
    open(paging::world(2));

    press(button("Empties this config"));
    rig().asks.front().accepted();

    EXPECT_TRUE(config().profiles.empty());
    EXPECT_TRUE(config().ports.empty());
}

TEST_F(SettingsPageTest, OpeningAConfigOpensThePicker) {
    open(paging::world(1));

    press(button("Work on a different config file"));

    EXPECT_TRUE(rig().files().state().open);
    EXPECT_EQ(rig().files().state().title, "Open a config file");
}

TEST_F(SettingsPageTest, AConfigPickedIsLoaded) {
    open(paging::world(1));

    const fs::path other = tests::Sandbox::scratch("settings-page") / "other.json";

    ASSERT_TRUE(paging::world(4).save(other));

    press(button("Work on a different config file"));
    rig().files().choose({other.string()});
    rig().turn();

    EXPECT_EQ(Session::get().path(), other);
    EXPECT_EQ(config().profiles.size(), 4U);
    EXPECT_TRUE(paging::shows(&page(), other.string()));
}

TEST_F(SettingsPageTest, SavingAsOffersTheCurrentName) {
    open(paging::world(1));

    press(button("Write this config somewhere else"));

    EXPECT_TRUE(rig().files().state().open);
    EXPECT_TRUE(rig().files().state().saving);
    EXPECT_EQ(rig().files().state().title, "Save the config as");
}

TEST_F(SettingsPageTest, NothingDownloadedCannotBeCleared) {
    open(paging::world(1));

    State::get().ports.cached = false;
    State::get().touch();
    rig().turn();

    ASSERT_NE(clearDownloads(), nullptr);
    EXPECT_FALSE(clearDownloads()->enabled());
}

TEST_F(SettingsPageTest, DownloadsKeptCanBeClearedAfterAsking) {
    open(paging::world(1));

    State::get().ports.cached = true;
    State::get().touch();
    rig().turn();

    ASSERT_TRUE(clearDownloads()->enabled());

    press(clearDownloads());

    ASSERT_EQ(rig().asks.size(), 1U);
    EXPECT_EQ(rig().asks.front().title, "Clear the downloads?");
    EXPECT_TRUE(rig().asks.front().danger);

    rig().asks.front().accepted();

    ASSERT_FALSE(rig().notify().messages().empty());
    EXPECT_EQ(rig().notify().messages().back().title, "The downloads are empty");
}

TEST_F(SettingsPageTest, WhatIsKeptIsSaid) {
    open(paging::world(1));

    State::get().ports.cachedText = "12 MB";
    State::get().touch();
    rig().turn();

    EXPECT_TRUE(paging::shows(
            &page(),
            "12 MB kept · what a port was fetched from stays here, so fetching it again does not bring it down twice"));
}

TEST_F(SettingsPageTest, TheFooterNamesTheVersionAndTheToolkit) {
    State::get().sys.version = "4.4.14 (abc)";
    State::get().sys.runtime = "tinytk 1.5.1";

    open(paging::world(1));

    EXPECT_TRUE(paging::shows(&page(), "ZDL4 4.4.14 (abc)"));
    EXPECT_TRUE(paging::shows(&page(), "A launcher for Doom engine source ports · tinytk 1.5.1"));
}

TEST_F(SettingsPageTest, AboutShowsTheAboutDialog) {
    open(paging::world(1));

    ttk::Button *about = nullptr;

    for (ttk::Button *each : paging::all<ttk::Button>(&page())) {
        if (each->hint.empty()) {
            about = each;
        }
    }

    press(about);

    EXPECT_EQ(rig().aboutShown, 1);
}

TEST_F(SettingsPageTest, AShortWindowScrollsThePage) {
    canvas().resize(1280, 420);
    rig().ui().relayout();

    open(paging::world(1));
    rig().ready();

    const std::vector<ttk::Scroll *> scrolls = paging::all<ttk::Scroll>(&page());

    ASSERT_EQ(scrolls.size(), 1U);
    ASSERT_TRUE(scrolls.front()->scrollable());

    const BLRect box = scrolls.front()->box();

    rig().ui().wheel(-3.0, box.x + (box.w / 2.0), box.y + (box.h / 2.0));
    canvas().run(40);

    EXPECT_GT(scrolls.front()->offset(), 0.0);
}

}
