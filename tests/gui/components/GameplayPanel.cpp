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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/Pill.h"
#include "ttk/toolkit/controls/Select.h"
#include "ttk/toolkit/controls/Toggle.h"

#include "Mount.h"
#include "core/config/Options.h"
#include "core/config/Session.h"
#include "core/launch/Flags.h"
#include "gui/components/GameplayPanel.h"
#include "gui/state/State.h"
#include "support/Rig.h"

namespace {

using parts::key;
namespace Code = ttk::Code;

constexpr BLRect SPOT{20.0, 20.0, 1000.0, 700.0};

Config gameplay(const std::string &program) {
    Config made = parts::onPort(program);

    made.profiles.front().set(Switch::gameplayOpen, true);
    made.profiles.front().flags = {};

    return made;
}

class GameplayPanelTest : public parts::Screened {
protected:
    components::GameplayPanel *make(Config what) {
        tests::Fixtures::install(std::move(what));
        rig().config().reload();

        _panel = parts::place(screen(), std::make_unique<components::GameplayPanel>(&rig().reach()), SPOT);

        sync();

        return _panel;
    }

    void sync() {
        rig().shell().advance(0.0);
        _panel->sync();
        screen().ui().relayout();
        screen().run(30);
    }

    // The select moves only once the panel syncs, so each key is answered before the next.
    void press(ttk::Widget &target, const int code, const int times) {
        for (int at = 0; at < times; ++at) {
            ui().focus(&target);
            ui().key(key(code));
            sync();
        }
    }

    void toggled(ttk::Widget &target) {
        parts::flip(ui(), target);
        sync();
    }

    static Profile &active() { return Session::get().config().activeProfile(); }

    ttk::Label &said() { return *static_cast<ttk::Label *>(_panel->pill()->parent()->children()[2].get()); }

    ttk::Toggle *pistolStart() {
        return parts::find<ttk::Toggle>(_panel, [](const ttk::Toggle &each) {
            return each.hint.starts_with("Starts every map with only the pistol");
        });
    }

    ttk::Toggle *cheats() {
        return parts::find<ttk::Toggle>(
                _panel, [](const ttk::Toggle &each) { return each.hint.starts_with("Turns on sv_cheats"); });
    }

    ttk::Field *word(const Flags::Word word) {
        return parts::captioned<ttk::Field>(_panel, std::string(Flags::name(word)));
    }

    ttk::Field *number(const Dialect::Feature option) {
        return parts::captioned<ttk::Field>(_panel, std::string(Options::ALL[Options::indexOf(option)].label));
    }

    // The switches come in table order, and only the rules live here.
    ttk::Toggle *toggle(const Dialect::Feature option) {
        size_t at = 0;

        for (const Options::Option &each : Options::ALL) {
            if (each.feature == option) {
                return parts::all<ttk::Toggle>(_panel).at(at);
            }

            at += each.kind == Options::Kind::toggle && each.group == Options::Group::rules ? 1 : 0;
        }

        return nullptr;
    }

    ttk::Select &level() { return *parts::find<ttk::Select>(_panel); }

    ttk::Label *startsWith(const std::string &text) {
        return parts::find<ttk::Label>(_panel,
                                       [&text](const ttk::Label &each) { return each.text().starts_with(text); });
    }

    std::vector<Flags::Word> shownWords() {
        std::vector<Flags::Word> out;

        for (const Flags::Word each : Flags::ALL) {
            if (parts::visibleToTheRoot(word(each))) {
                out.push_back(each);
            }
        }

        return out;
    }

    components::GameplayPanel *_panel = nullptr;
};

}

// The panel syncs on every change to the profile, and one that changes nothing here paints nothing.
TEST_F(GameplayPanelTest, ASyncThatChangesNothingPaintsNothing) {
    make(gameplay("gzdoom"));

    ASSERT_TRUE(_panel->visible());

    _panel->sync();

    EXPECT_EQ(screen().frame(), 0U);
}

TEST_F(GameplayPanelTest, PistolStartAndCheatsShowOnlyForPortsThatTakeThem) {
    make(gameplay("gzdoom"));

    EXPECT_TRUE(parts::visibleToTheRoot(pistolStart()));
    EXPECT_TRUE(parts::visibleToTheRoot(cheats()));

    make(gameplay("woof"));

    EXPECT_TRUE(parts::visibleToTheRoot(pistolStart()));
    EXPECT_FALSE(parts::visibleToTheRoot(cheats()));
    EXPECT_TRUE(pistolStart()->hint.contains("the port stops on it"));
}

TEST_F(GameplayPanelTest, TheFileAndSoundSwitchesLiveUnderGeneral) {
    make(gameplay("woof"));

    std::size_t rules = 0;

    for (const Options::Option &each : Options::ALL) {
        rules += each.kind == Options::Kind::toggle && each.group == Options::Group::rules ? 1 : 0;
    }

    EXPECT_EQ(parts::all<ttk::Toggle>(_panel).size(), rules);

    make(gameplay("boom"));

    EXPECT_FALSE(_panel->visible()) << "music and sound effects alone leave nothing to show here";
}

TEST_F(GameplayPanelTest, ClickingPistolStartAndCheatsWritesThem) {
    make(gameplay("gzdoom"));
    toggled(*pistolStart());
    toggled(*cheats());

    EXPECT_TRUE(active().on(Dialect::Feature::pistolStart));
    EXPECT_TRUE(active().on(Dialect::Feature::cheats));
}

TEST_F(GameplayPanelTest, ChocolateHasTurboAndAGameVersion) {
    make(gameplay("chocolate-doom"));

    EXPECT_TRUE(_panel->visible());
    EXPECT_TRUE(parts::visibleToTheRoot(number(Dialect::Feature::turbo)));
    EXPECT_TRUE(parts::visibleToTheRoot(&level()));
    EXPECT_FALSE(parts::visibleToTheRoot(pistolStart()));
    EXPECT_FALSE(parts::visibleToTheRoot(toggle(Dialect::Feature::coopSpawns)));
}

TEST_F(GameplayPanelTest, TheSwitchesShowForThePortsThatTakeThem) {
    make(gameplay("crispy-doom"));

    for (const Dialect::Feature shown : {
                 Dialect::Feature::coopSpawns,
                 Dialect::Feature::doubleAmmo,
                 Dialect::Feature::flipLevels,
                 Dialect::Feature::flipWeapons,
         }) {
        EXPECT_TRUE(parts::visibleToTheRoot(toggle(shown))) << Options::ALL[Options::indexOf(shown)].label;
    }

    EXPECT_FALSE(parts::visibleToTheRoot(toggle(Dialect::Feature::chainEpisodes)));
    EXPECT_FALSE(parts::visibleToTheRoot(number(Dialect::Feature::gameSpeed)));

    make(gameplay("dsda-doom"));

    EXPECT_TRUE(parts::visibleToTheRoot(toggle(Dialect::Feature::chainEpisodes)));
    EXPECT_TRUE(parts::visibleToTheRoot(number(Dialect::Feature::gameSpeed)));
    EXPECT_FALSE(parts::visibleToTheRoot(number(Dialect::Feature::dogs))) << "DSDA-Doom stops on -dogs";

    make(gameplay("gzdoom"));

    EXPECT_TRUE(parts::visibleToTheRoot(toggle(Dialect::Feature::alwaysApplyDmflags)));
    EXPECT_FALSE(parts::visibleToTheRoot(toggle(Dialect::Feature::coopPreset))) << "UZDoom only";
}

TEST_F(GameplayPanelTest, ASwitchTurnedOnIsKept) {
    make(gameplay("crispy-doom"));
    toggled(*toggle(Dialect::Feature::coopSpawns));

    EXPECT_TRUE(active().on(Dialect::Feature::coopSpawns));
    EXPECT_TRUE(toggle(Dialect::Feature::coopSpawns)->checked);
    EXPECT_EQ(said().text(), "Coop things");

    toggled(*toggle(Dialect::Feature::coopSpawns));

    EXPECT_FALSE(active().on(Dialect::Feature::coopSpawns));
}

TEST_F(GameplayPanelTest, TheSummaryNamesPistolStartAndCheatsLikeAnyOtherOption) {
    make(gameplay("gzdoom"));
    toggled(*pistolStart());
    toggled(*cheats());
    toggled(*toggle(Dialect::Feature::alwaysApplyDmflags));

    EXPECT_EQ(said().text(), "Pistol start · Allow cheats · Deathmatch flags everywhere");
}

TEST_F(GameplayPanelTest, ANumberOutsideThePortsRangeIsMarked) {
    make(gameplay("dsda-doom"));
    type(*number(Dialect::Feature::turbo), "300");
    sync();

    auto *badge = parts::find<ttk::Pill>(number(Dialect::Feature::turbo));

    ASSERT_NE(badge, nullptr);
    EXPECT_TRUE(parts::visibleToTheRoot(badge)) << "DSDA-Doom takes 10 to 255";
    EXPECT_EQ(active().number(Dialect::Feature::turbo), "300");

    type(*number(Dialect::Feature::turbo), "150");
    sync();

    EXPECT_FALSE(parts::visibleToTheRoot(badge));
    EXPECT_EQ(said().text(), "Turbo 150");
}

TEST_F(GameplayPanelTest, EachPortShowsTheWordsItReads) {
    make(gameplay("gzdoom"));

    EXPECT_EQ(shownWords(),
              (std::vector<Flags::Word>{Flags::Word::dmflags,
                                        Flags::Word::dmflags2,
                                        Flags::Word::dmflags3,
                                        Flags::Word::compatflags,
                                        Flags::Word::compatflags2}));

    make(gameplay("zdoom"));

    EXPECT_EQ(
            shownWords(),
            (std::vector<Flags::Word>{
                    Flags::Word::dmflags, Flags::Word::dmflags2, Flags::Word::compatflags, Flags::Word::compatflags2}));

    make(gameplay("zandronum"));

    EXPECT_EQ(shownWords(),
              (std::vector<Flags::Word>{Flags::Word::dmflags,
                                        Flags::Word::dmflags2,
                                        Flags::Word::zadmflags,
                                        Flags::Word::lmsWeapons,
                                        Flags::Word::lmsSpectators,
                                        Flags::Word::compatflags,
                                        Flags::Word::compatflags2,
                                        Flags::Word::zacompatflags}));

    make(gameplay("eternity"));

    EXPECT_EQ(shownWords(), (std::vector<Flags::Word>{Flags::Word::dmflags}));

    make(gameplay("dsda-doom"));

    EXPECT_TRUE(shownWords().empty());
}

TEST_F(GameplayPanelTest, TypingAWordWritesIt) {
    make(gameplay("gzdoom"));
    type(*word(Flags::Word::dmflags2), "64");
    sync();

    EXPECT_EQ(active().flag(Flags::Word::dmflags2), "64");
}

TEST_F(GameplayPanelTest, AWordThatIsNotANumberIsMarked) {
    make(gameplay("gzdoom"));
    type(*word(Flags::Word::dmflags), "4x");
    sync();

    auto *badge = parts::find<ttk::Pill>(word(Flags::Word::dmflags));

    ASSERT_NE(badge, nullptr);
    EXPECT_TRUE(parts::visibleToTheRoot(badge));
    EXPECT_EQ(said().text(), "dmflags");

    type(*word(Flags::Word::dmflags), "4");
    sync();

    EXPECT_FALSE(parts::visibleToTheRoot(badge));
}

TEST_F(GameplayPanelTest, ThePencilOpensTheDialogOfItsFamily) {
    make(gameplay("gzdoom"));
    parts::click(ui(), *parts::find<ttk::GlyphButton>(word(Flags::Word::dmflags3)));
    parts::click(ui(), *parts::find<ttk::GlyphButton>(word(Flags::Word::compatflags)));

    EXPECT_EQ(rig().flagEdits, (std::vector<Flags::Family>{Flags::Family::gameplay, Flags::Family::compatibility}));
}

TEST_F(GameplayPanelTest, PickingAModeFillsTheCompatibilityWords) {
    make(gameplay("gzdoom"));

    ASSERT_TRUE(parts::visibleToTheRoot(&level()));
    EXPECT_EQ(level().current(), 0);

    press(level(), Code::Down, 2);

    EXPECT_EQ(active().flag(Flags::Word::compatflags), "2820160259");
    EXPECT_EQ(word(Flags::Word::compatflags)->text(), "2820160259");
    EXPECT_EQ(said().text(), "Doom");
}

TEST_F(GameplayPanelTest, ADsdaRecordingWithoutAComplevelWarns) {
    Config what = gameplay("dsda-doom");

    what.profiles.front().replay = {.mode = ReplayMode::Record, .file = "run"};
    make(std::move(what));

    EXPECT_TRUE(parts::visibleToTheRoot(startsWith("The profile records a demo")));

    press(level(), Code::Down, 19);

    EXPECT_EQ(active().complevel, 21);
    EXPECT_FALSE(parts::visibleToTheRoot(startsWith("The profile records a demo")));
}

TEST_F(GameplayPanelTest, TheKeptNoteShowsWhereThePortSavesTheWords) {
    make(gameplay("gzdoom"));

    EXPECT_TRUE(parts::visibleToTheRoot(startsWith("The port keeps both compatibility words")));

    make(gameplay("zandronum"));

    EXPECT_FALSE(parts::visibleToTheRoot(startsWith("The port keeps both compatibility words")));
}

TEST_F(GameplayPanelTest, TheWordsSitSideBySideAboveTheNextGroup) {
    make(gameplay("gzdoom"));

    const BLRect first = word(Flags::Word::dmflags)->box();
    const BLRect second = word(Flags::Word::dmflags2)->box();
    const double compatibility = parts::labelled(_panel, "Compatibility")->box().y;

    EXPECT_DOUBLE_EQ(first.y, second.y);
    EXPECT_LT(first.x + first.w, second.x);
    EXPECT_LE(word(Flags::Word::dmflags3)->box().y + word(Flags::Word::dmflags3)->box().h, compatibility);
}
