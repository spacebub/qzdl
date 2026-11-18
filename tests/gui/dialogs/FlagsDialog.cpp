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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/Event.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/Check.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/layout/Scroll.h"

#include "Dialogs.h"
#include "core/launch/Dialect.h"
#include "core/launch/Flags.h"
#include "core/util/Sparse.h"
#include "gui/dialogs/FlagsDialog.h"

namespace {

using dialogs::FlagsDialog;
using Typed = Sparse<Flags::Word>;

class FlagsDialogTest : public modal::Dialogued {
protected:
    std::vector<Flags::Edits> answers;

    FlagsDialog *dialog = nullptr;

    void show(const Flags::Family family, const char *file, const Typed &typed = {}) {
        show(family, Dialect::of(std::filesystem::path(file)), typed);
    }

    void show(const Flags::Family family, const Dialect::Traits &traits, const Typed &typed = {}) {
        dialog = open(std::make_unique<FlagsDialog>(
                family, traits, typed, [this](Flags::Edits edits) { answers.push_back(std::move(edits)); }));
    }

    static Typed typed(const Flags::Word word, const std::string &value) {
        Typed out;

        out.set(word, value);

        return out;
    }

    [[nodiscard]] ttk::Check *check(const std::string &text) const {
        for (ttk::Check *each : modal::all<ttk::Check>(dialog)) {
            if (each->text() == text) {
                return each;
            }
        }

        return nullptr;
    }

    // In the order the footer holds them.
    enum class Footer : std::uint8_t {
        clearAll,
        cancel,
        save,
    };

    [[nodiscard]] ttk::Button *button(const Footer which) const {
        return modal::all<ttk::Button>(dialog).at(static_cast<size_t>(which));
    }

    [[nodiscard]] bool visibleLabel(const std::string &text) const {
        for (const ttk::Label *each : modal::all<ttk::Label>(dialog)) {
            if (each->text() != text) {
                continue;
            }

            for (const ttk::Widget *up = each; up != nullptr; up = up->parent()) {
                if (!up->visible()) {
                    return false;
                }
            }

            return true;
        }

        return false;
    }

    [[nodiscard]] std::string footer() const {
        for (const ttk::Label *each : modal::all<ttk::Label>(dialog)) {
            if (each->text().starts_with(Flags::name(Flags::Word::dmflags))
                || each->text().starts_with(Flags::name(Flags::Word::compatflags))) {
                return each->text();
            }
        }

        return {};
    }

    void press(const ttk::Widget *widget) {
        turn();
        modal::click(canvas.ui(), widget);
        turn();
    }

    // On the square, since a row is wider than its text and a click past the text misses.
    void tick(const ttk::Check *box) {
        turn();
        modal::click(canvas.ui(), box->box().x + 9.0, box->box().y + (box->box().h / 2.0));
        turn();
    }
};

}

TEST_F(FlagsDialogTest, GzdoomShowsItsGameplayGroups) {
    show(Flags::Family::gameplay, "gzdoom");

    for (const char *group : {
                 "Monsters",
                 "Items",
                 "Weapons and ammo",
                 "Players",
                 "Death and respawn",
                 "Maps",
                 "Cooperative",
                 "Deathmatch and teams",
         }) {
        EXPECT_TRUE(visibleLabel(group)) << group;
    }

    EXPECT_FALSE(modal::shows(dialog, "Online play"));
    EXPECT_FALSE(modal::shows(dialog, "Last man standing"));
    EXPECT_NE(check("Pistol start every map"), nullptr);
    EXPECT_EQ(check("No runes"), nullptr) << "a Zandronum bit";
}

TEST_F(FlagsDialogTest, ZandronumShowsItsOwnRules) {
    show(Flags::Family::gameplay, "zandronum");

    EXPECT_TRUE(modal::shows(dialog, "Online play"));
    EXPECT_TRUE(modal::shows(dialog, "Last man standing weapons"));
    EXPECT_NE(check("No runes"), nullptr);
    EXPECT_NE(check("BFG aiming"), nullptr);
    EXPECT_EQ(check("No BFG aiming"), nullptr) << "the ZDoom sense of the bit";
    EXPECT_EQ(check("Pistol start every map"), nullptr);
}

TEST_F(FlagsDialogTest, EternityShowsItsSevenBits) {
    show(Flags::Family::gameplay, "eternity");

    EXPECT_EQ(modal::all<ttk::Check>(dialog).size(), 7U);
    EXPECT_TRUE(modal::all<ttk::MultistateSwitch>(dialog).empty());
}

TEST_F(FlagsDialogTest, TheBoxesShowTheTypedWord) {
    show(Flags::Family::gameplay, "gzdoom", typed(Flags::Word::dmflags, "36864"));

    EXPECT_TRUE(check("No monsters")->checked());
    EXPECT_TRUE(check("Fast monsters")->checked());
    EXPECT_FALSE(check("Monsters respawn")->checked());
    EXPECT_EQ(footer(), "dmflags 36864  ·  dmflags2 not set  ·  dmflags3 not set");
}

TEST_F(FlagsDialogTest, AChoiceShowsItsValue) {
    show(Flags::Family::gameplay, "gzdoom", typed(Flags::Word::dmflags, "16"));

    const std::vector<ttk::MultistateSwitch *> choices = modal::all<ttk::MultistateSwitch>(dialog);

    ASSERT_FALSE(choices.empty());
    EXPECT_EQ(choices.front()->current(), 16) << "falling damage, Hexen";
}

TEST_F(FlagsDialogTest, TickingABoxSavesOnlyTheWordItChanged) {
    show(Flags::Family::gameplay, "gzdoom", typed(Flags::Word::dmflags2, "64"));
    tick(check("No monsters"));

    EXPECT_TRUE(check("No monsters")->checked());
    EXPECT_EQ(footer(), "dmflags 4096  ·  dmflags2 64  ·  dmflags3 not set");

    press(button(Footer::save));

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), (Flags::Edits{{Flags::Word::dmflags, "4096"}}));
}

TEST_F(FlagsDialogTest, BitsThePortGivesNoMeaningToAreKept) {
    show(Flags::Family::gameplay, "gzdoom", typed(Flags::Word::dmflags, "32"));

    EXPECT_TRUE(visibleLabel("This port gives no meaning to some bits set here. They are kept as they are."));

    tick(check("No monsters"));
    press(button(Footer::save));

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), (Flags::Edits{{Flags::Word::dmflags, "4128"}}));
}

TEST_F(FlagsDialogTest, Zdoom2_7GivesNoMeaningToDmflags2Bit27) {
    show(Flags::Family::gameplay,
         Dialect::of(Engine{.name = "ZDoom", .file = "/games/zdoom", .version = "2.7.1"}),
         typed(Flags::Word::dmflags2, "134217728"));

    EXPECT_TRUE(visibleLabel("This port gives no meaning to some bits set here. They are kept as they are."));
    ASSERT_NE(check("Big powerups respawn"), nullptr);
    EXPECT_FALSE(check("Big powerups respawn")->checked());
}

TEST_F(FlagsDialogTest, Zdoom2_8RespawnsBigPowerupsWithDmflags2Bit27) {
    show(Flags::Family::gameplay,
         Dialect::of(Engine{.name = "ZDoom", .file = "/games/zdoom", .version = "2.8.0"}),
         typed(Flags::Word::dmflags2, "134217728"));

    EXPECT_FALSE(visibleLabel("This port gives no meaning to some bits set here. They are kept as they are."));
    ASSERT_NE(check("Big powerups respawn"), nullptr);
    EXPECT_TRUE(check("Big powerups respawn")->checked());
}

TEST_F(FlagsDialogTest, ClearAllLeavesEveryWordToThePort) {
    Typed given;

    given.set(Flags::Word::dmflags, "4096");
    given.set(Flags::Word::dmflags3, "128");
    show(Flags::Family::gameplay, "gzdoom", given);
    press(button(Footer::clearAll));

    EXPECT_FALSE(check("No monsters")->checked());
    EXPECT_EQ(footer(), "dmflags not set  ·  dmflags2 not set  ·  dmflags3 not set");

    press(button(Footer::save));

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(),
              (Flags::Edits{{Flags::Word::dmflags, ""}, {Flags::Word::dmflags2, ""}, {Flags::Word::dmflags3, ""}}));
}

TEST_F(FlagsDialogTest, AWordThatIsNotANumberStaysAsTypedUntilABoxChanges) {
    show(Flags::Family::gameplay, "gzdoom", typed(Flags::Word::dmflags, "abc"));

    EXPECT_EQ(footer(), "dmflags is not a number  ·  dmflags2 not set  ·  dmflags3 not set");

    press(button(Footer::save));

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_TRUE(answers.front().empty());
}

TEST_F(FlagsDialogTest, CancelAnswersNothing) {
    show(Flags::Family::gameplay, "gzdoom");
    tick(check("No monsters"));
    press(button(Footer::cancel));

    EXPECT_TRUE(answers.empty());
}

TEST_F(FlagsDialogTest, ATooltipSaysWhatTheBoxDoesAndWhichBit) {
    show(Flags::Family::gameplay, "gzdoom");

    EXPECT_EQ(check("No monsters")->hint,
              "Monsters are left out of the map, as with -nomonsters. Sets 4096 in dmflags, sv_nomonsters.");
}

TEST_F(FlagsDialogTest, TheCompatibilityGroupsFollowThePortMenu) {
    show(Flags::Family::compatibility, "gzdoom");

    for (const char *group : {"Actors", "DeHackEd", "Maps and actions", "Physics", "Rendering", "Sound", "Scripts"}) {
        EXPECT_TRUE(visibleLabel(group)) << group;
    }

    EXPECT_EQ(footer(), "compatflags not set  ·  compatflags2 not set");
    EXPECT_TRUE(modal::shows(dialog,
                             "The port keeps these words in its own settings. A word left empty runs as it ran last "
                             "time, and 0 clears it."));
}

TEST_F(FlagsDialogTest, TheLatestGzdoomOffersArchVileGhostsForBit25) {
    show(Flags::Family::compatibility, "gzdoom");

    EXPECT_NE(check("Arch-vile ghosts"), nullptr);
    EXPECT_EQ(check("Crushed monsters turn to gibs"), nullptr);
}

TEST_F(FlagsDialogTest, Gzdoom4_4OffersCorpseGibsForBit25) {
    show(Flags::Family::compatibility,
         Dialect::of(Engine{.name = "GZDoom", .file = "/games/gzdoom", .version = "4.4.0"}));

    EXPECT_NE(check("Crushed monsters turn to gibs"), nullptr);
    EXPECT_EQ(check("Arch-vile ghosts"), nullptr);
}

TEST_F(FlagsDialogTest, ZandronumKeepsNoCompatibilityWord) {
    show(Flags::Family::compatibility, "zandronum");

    EXPECT_FALSE(modal::shows(dialog,
                              "The port keeps these words in its own settings. A word left empty runs as it ran last "
                              "time, and 0 clears it."));
    EXPECT_EQ(footer(), "compatflags not set  ·  compatflags2 not set  ·  zacompatflags not set");
}

TEST_F(FlagsDialogTest, AClickOnTheTextTicksAndOnePastItDoesNot) {
    show(Flags::Family::gameplay, "gzdoom");

    const ttk::Check *box = check("No monsters");
    const double y = box->box().y + (box->box().h / 2.0);

    turn();
    modal::click(canvas.ui(), box->box().x + 40.0, y);
    turn();

    EXPECT_TRUE(box->checked());

    modal::click(canvas.ui(), box->box().x + box->box().w - 4.0, y);
    turn();

    EXPECT_TRUE(box->checked());
}

TEST_F(FlagsDialogTest, TheCardFitsAFewBoxesAndCapsMany) {
    show(Flags::Family::gameplay, "eternity");

    const double few = dialog->card()->box().h;

    show(Flags::Family::gameplay, "gzdoom");

    const double many = dialog->card()->box().h;

    EXPECT_LT(few, many);
    EXPECT_LE(many, 720.0);
}

namespace {

class RevealedFlagsDialogTest : public FlagsDialogTest {
protected:
    // Scrolled so the widget is at the top of the view.
    void reveal(const ttk::Widget *widget) {
        auto *scroll = modal::all<ttk::Scroll>(dialog).front();

        scroll->scroll_to(scroll->offset() + widget->box().y - scroll->box().y);
        turn();
    }
};

}

TEST_F(RevealedFlagsDialogTest, AClickOnAChoiceSetsItsBits) {
    show(Flags::Family::gameplay, "gzdoom", typed(Flags::Word::dmflags, "4096"));

    ttk::MultistateSwitch *falling = modal::all<ttk::MultistateSwitch>(dialog).front();

    reveal(falling);

    // Strife, the last of Map, ZDoom, Hexen and Strife.
    canvas.ui().focus(falling);
    canvas.ui().key(ttk::Key{.code = ttk::Code::End});
    turn();

    EXPECT_EQ(falling->current(), 24);

    press(button(Footer::save));

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), (Flags::Edits{{Flags::Word::dmflags, "4120"}}));
}

TEST_F(RevealedFlagsDialogTest, AWordThatStartsFullLosesWhatIsUnticked) {
    show(Flags::Family::gameplay, "zandronum");

    ttk::Check *chainsaw = check("Chainsaw");

    ASSERT_NE(chainsaw, nullptr);
    EXPECT_TRUE(chainsaw->checked()) << "every weapon is allowed until the word says otherwise";

    reveal(chainsaw);
    tick(chainsaw);
    press(button(Footer::save));

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), (Flags::Edits{{Flags::Word::lmsWeapons, "511"}}));
}

TEST_F(FlagsDialogTest, ATickAfterClearAllWritesOnlyThatWord) {
    show(Flags::Family::gameplay, "gzdoom", typed(Flags::Word::dmflags2, "64"));
    press(button(Footer::clearAll));
    tick(check("No monsters"));
    press(button(Footer::save));

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(),
              (Flags::Edits{{Flags::Word::dmflags, "4096"}, {Flags::Word::dmflags2, ""}, {Flags::Word::dmflags3, ""}}));
}

TEST_F(FlagsDialogTest, ZandronumNamesItsOwnCoopCvars) {
    show(Flags::Family::gameplay, "zandronum");

    EXPECT_TRUE(check("Lose keys")->hint.ends_with("sv_coop_losekeys."));
}

TEST_F(FlagsDialogTest, EveryChoiceHasRoomForAllItsWordsOnALineOfItsOwn) {
    show(Flags::Family::gameplay, "gzdoom");

    const std::vector<ttk::MultistateSwitch *> choices = modal::all<ttk::MultistateSwitch>(dialog);

    ASSERT_EQ(choices.size(), 4U) << "falling damage, jumping, crouching and mouse look";

    for (ttk::MultistateSwitch *each : choices) {
        EXPECT_GE(each->box().w + 0.5, each->natural_width(canvas.type()));

        // Nothing else shares the line, so the next row starts below it.
        const ttk::Widget *line = each->parent();

        for (const ttk::Widget::Ptr &other : line->parent()->children()) {
            if (other.get() != line && other->visible()) {
                const bool apart = other->box().y >= line->box().y + line->box().h
                                   || other->box().y + other->box().h <= line->box().y;

                EXPECT_TRUE(apart);
            }
        }
    }
}

TEST_F(FlagsDialogTest, ATypedWordIsDrawnTickedWhenTheDialogOpens) {
    show(Flags::Family::gameplay, "gzdoom", typed(Flags::Word::dmflags, "4096"));
    canvas.full();

    const ttk::Check *box = check("No monsters");
    const int x = static_cast<int>(box->box().x + 3.0);
    const int y = static_cast<int>(box->box().y + (box->box().h / 2.0));

    EXPECT_EQ(canvas.at(x, y), ttk::Theme::palette().accent.value) << "the box is drawn empty";
}
