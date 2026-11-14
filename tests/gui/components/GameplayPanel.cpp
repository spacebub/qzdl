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

    made.profiles.front().gameplayOpen = true;
    made.profiles.front().flags.clear();

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

    ttk::Field *word(const std::string &name) { return parts::captioned<ttk::Field>(_panel, name); }

    ttk::Select &level() { return *parts::find<ttk::Select>(_panel); }

    ttk::Label *startsWith(const std::string &text) {
        return parts::find<ttk::Label>(_panel,
                                       [&text](const ttk::Label &each) { return each.text().starts_with(text); });
    }

    std::vector<std::string> shownWords() {
        std::vector<std::string> out;

        for (size_t at = 0; at < Flags::WORDS; ++at) {
            const std::string name(Flags::info(static_cast<Flags::Word>(at)).name);

            if (parts::visibleToTheRoot(word(name))) {
                out.push_back(name);
            }
        }

        return out;
    }

    components::GameplayPanel *_panel = nullptr;
};

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

TEST_F(GameplayPanelTest, ClickingPistolStartAndCheatsWritesThem) {
    make(gameplay("gzdoom"));
    toggled(*pistolStart());
    toggled(*cheats());

    EXPECT_TRUE(active().pistolStart);
    EXPECT_TRUE(active().cheats);
}

TEST_F(GameplayPanelTest, ChocolateHasNothingToSet) {
    make(gameplay("chocolate-doom"));

    EXPECT_FALSE(_panel->visible());
}

TEST_F(GameplayPanelTest, EachPortShowsTheWordsItReads) {
    make(gameplay("gzdoom"));

    EXPECT_EQ(shownWords(),
              (std::vector<std::string>{"dmflags", "dmflags2", "dmflags3", "compatflags", "compatflags2"}));

    make(gameplay("zdoom"));

    EXPECT_EQ(shownWords(), (std::vector<std::string>{"dmflags", "dmflags2", "compatflags", "compatflags2"}));

    make(gameplay("zandronum"));

    EXPECT_EQ(shownWords(),
              (std::vector<std::string>{"dmflags",
                                        "dmflags2",
                                        "zadmflags",
                                        "lmsallowedweapons",
                                        "lmsspectatorsettings",
                                        "compatflags",
                                        "compatflags2",
                                        "zacompatflags"}));

    make(gameplay("eternity"));

    EXPECT_EQ(shownWords(), (std::vector<std::string>{"dmflags"}));

    make(gameplay("dsda-doom"));

    EXPECT_TRUE(shownWords().empty());
}

TEST_F(GameplayPanelTest, TypingAWordWritesIt) {
    make(gameplay("gzdoom"));
    type(*word("dmflags2"), "64");
    sync();

    EXPECT_EQ(active().flag("dmflags2"), "64");
}

TEST_F(GameplayPanelTest, AWordThatIsNotANumberIsMarked) {
    make(gameplay("gzdoom"));
    type(*word("dmflags"), "4x");
    sync();

    auto *badge = parts::find<ttk::Pill>(word("dmflags"));

    ASSERT_NE(badge, nullptr);
    EXPECT_TRUE(parts::visibleToTheRoot(badge));
    EXPECT_EQ(said().text(), "dmflags");

    type(*word("dmflags"), "4");
    sync();

    EXPECT_FALSE(parts::visibleToTheRoot(badge));
}

TEST_F(GameplayPanelTest, ThePencilOpensTheDialogOfItsFamily) {
    make(gameplay("gzdoom"));
    parts::click(ui(), *parts::find<ttk::GlyphButton>(word("dmflags3")));
    parts::click(ui(), *parts::find<ttk::GlyphButton>(word("compatflags")));

    EXPECT_EQ(rig().flagEdits, (std::vector<Flags::Family>{Flags::Family::gameplay, Flags::Family::compatibility}));
}

TEST_F(GameplayPanelTest, PickingAModeFillsTheCompatibilityWords) {
    make(gameplay("gzdoom"));

    ASSERT_TRUE(parts::visibleToTheRoot(&level()));
    EXPECT_EQ(level().current(), 0);

    press(level(), Code::Down, 2);

    EXPECT_EQ(active().flag("compatflags"), "2820160259");
    EXPECT_EQ(word("compatflags")->text(), "2820160259");
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

    const BLRect first = word("dmflags")->box();
    const BLRect second = word("dmflags2")->box();
    const double compatibility = parts::labelled(_panel, "Compatibility")->box().y;

    EXPECT_DOUBLE_EQ(first.y, second.y);
    EXPECT_LT(first.x + first.w, second.x);
    EXPECT_LE(word("dmflags3")->box().y + word("dmflags3")->box().h, compatibility);
}
