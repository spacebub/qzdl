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

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/toolkit/Event.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/Check.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/layout/Scroll.h"

#include "Dialogs.h"
#include "core/launch/Flags.h"
#include "gui/dialogs/FlagsDialog.h"

namespace {

using dialogs::FlagsDialog;
using Typed = std::array<std::string, Flags::WORDS>;

class FlagsDialogTest : public modal::Dialogued {
protected:
    std::vector<FlagsDialog::Edited> answers;

    FlagsDialog *dialog = nullptr;

    void show(const Flags::Family family, const Flags::Set set, const Typed &typed = {}) {
        dialog = open(std::make_unique<FlagsDialog>(
                family, set, typed, [this](FlagsDialog::Edited edited) { answers.push_back(std::move(edited)); }));
    }

    static Typed typed(const Flags::Word word, const std::string &value) {
        Typed out;

        out[static_cast<size_t>(word)] = value;

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

    // Clear all, Cancel and Save, in that order.
    [[nodiscard]] ttk::Button *button(const std::string &text) const {
        const size_t at = text == "Clear all" ? 0 : text == "Cancel" ? 1 : 2;

        return modal::all<ttk::Button>(dialog).at(at);
    }

    [[nodiscard]] bool visibleLabel(const std::string &text) const {
        for (const ttk::Label *each : modal::all<ttk::Label>(dialog)) {
            if (each->text() == text) {
                for (const ttk::Widget *up = each; up != nullptr; up = up->parent()) {
                    if (!up->visible()) {
                        return false;
                    }
                }

                return true;
            }
        }

        return false;
    }

    [[nodiscard]] std::string footer() const {
        for (const ttk::Label *each : modal::all<ttk::Label>(dialog)) {
            if (each->text().starts_with(Flags::info(Flags::Word::dmflags).name)
                || each->text().starts_with(Flags::info(Flags::Word::compatflags).name)) {
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
    show(Flags::Family::gameplay, Flags::Set::gzdoom);

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
    show(Flags::Family::gameplay, Flags::Set::zandronum);

    EXPECT_TRUE(modal::shows(dialog, "Online play"));
    EXPECT_TRUE(modal::shows(dialog, "Last man standing weapons"));
    EXPECT_NE(check("No runes"), nullptr);
    EXPECT_NE(check("BFG aiming"), nullptr);
    EXPECT_EQ(check("No BFG aiming"), nullptr) << "the ZDoom sense of the bit";
    EXPECT_EQ(check("Pistol start every map"), nullptr);
}

TEST_F(FlagsDialogTest, EternityShowsItsSevenBits) {
    show(Flags::Family::gameplay, Flags::Set::eternity);

    EXPECT_EQ(modal::all<ttk::Check>(dialog).size(), 7U);
    EXPECT_TRUE(modal::all<ttk::MultistateSwitch>(dialog).empty());
}

TEST_F(FlagsDialogTest, TheBoxesShowTheTypedWord) {
    show(Flags::Family::gameplay, Flags::Set::gzdoom, typed(Flags::Word::dmflags, "36864"));

    EXPECT_TRUE(check("No monsters")->checked());
    EXPECT_TRUE(check("Fast monsters")->checked());
    EXPECT_FALSE(check("Monsters respawn")->checked());
    EXPECT_EQ(footer(), "dmflags 36864  ·  dmflags2 not set  ·  dmflags3 not set");
}

TEST_F(FlagsDialogTest, AChoiceShowsItsValue) {
    show(Flags::Family::gameplay, Flags::Set::gzdoom, typed(Flags::Word::dmflags, "16"));

    const std::vector<ttk::MultistateSwitch *> choices = modal::all<ttk::MultistateSwitch>(dialog);

    ASSERT_FALSE(choices.empty());
    EXPECT_EQ(choices.front()->current(), 16) << "falling damage, Hexen";
}

TEST_F(FlagsDialogTest, TickingABoxSavesOnlyTheWordItChanged) {
    show(Flags::Family::gameplay, Flags::Set::gzdoom, typed(Flags::Word::dmflags2, "64"));
    tick(check("No monsters"));

    EXPECT_TRUE(check("No monsters")->checked());
    EXPECT_EQ(footer(), "dmflags 4096  ·  dmflags2 64  ·  dmflags3 not set");

    press(button("Save"));

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), (FlagsDialog::Edited{{Flags::Word::dmflags, "4096"}}));
}

TEST_F(FlagsDialogTest, BitsThePortGivesNoMeaningToAreKept) {
    show(Flags::Family::gameplay, Flags::Set::gzdoom, typed(Flags::Word::dmflags, "32"));

    EXPECT_TRUE(visibleLabel("This port gives no meaning to some bits set here. They are kept as they are."));

    tick(check("No monsters"));
    press(button("Save"));

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), (FlagsDialog::Edited{{Flags::Word::dmflags, "4128"}}));
}

TEST_F(FlagsDialogTest, ClearAllLeavesEveryWordToThePort) {
    Typed given;

    given[static_cast<size_t>(Flags::Word::dmflags)] = "4096";
    given[static_cast<size_t>(Flags::Word::dmflags3)] = "128";
    show(Flags::Family::gameplay, Flags::Set::gzdoom, given);
    press(button("Clear all"));

    EXPECT_FALSE(check("No monsters")->checked());
    EXPECT_EQ(footer(), "dmflags not set  ·  dmflags2 not set  ·  dmflags3 not set");

    press(button("Save"));

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(),
              (FlagsDialog::Edited{
                      {Flags::Word::dmflags, ""}, {Flags::Word::dmflags2, ""}, {Flags::Word::dmflags3, ""}}));
}

TEST_F(FlagsDialogTest, AWordThatIsNotANumberStaysAsTypedUntilABoxChanges) {
    show(Flags::Family::gameplay, Flags::Set::gzdoom, typed(Flags::Word::dmflags, "abc"));

    EXPECT_EQ(footer(), "dmflags is not a number  ·  dmflags2 not set  ·  dmflags3 not set");

    press(button("Save"));

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_TRUE(answers.front().empty());
}

TEST_F(FlagsDialogTest, CancelAnswersNothing) {
    show(Flags::Family::gameplay, Flags::Set::gzdoom);
    tick(check("No monsters"));
    press(button("Cancel"));

    EXPECT_TRUE(answers.empty());
}

TEST_F(FlagsDialogTest, ATooltipSaysWhatTheBoxDoesAndWhichBit) {
    show(Flags::Family::gameplay, Flags::Set::gzdoom);

    EXPECT_EQ(check("No monsters")->hint,
              "Monsters are left out of the map, as with -nomonsters. Sets 4096 in dmflags, sv_nomonsters.");
}

TEST_F(FlagsDialogTest, TheCompatibilityGroupsFollowThePortMenu) {
    show(Flags::Family::compatibility, Flags::Set::gzdoom);

    for (const char *group : {"Actors", "DeHackEd", "Maps and actions", "Physics", "Rendering", "Sound", "Scripts"}) {
        EXPECT_TRUE(visibleLabel(group)) << group;
    }

    EXPECT_EQ(footer(), "compatflags not set  ·  compatflags2 not set");
    EXPECT_TRUE(modal::shows(dialog,
                             "The port keeps these words in its own settings. A word left empty runs as it ran last "
                             "time, and 0 clears it."));
}

TEST_F(FlagsDialogTest, ZandronumKeepsNoCompatibilityWord) {
    show(Flags::Family::compatibility, Flags::Set::zandronum);

    EXPECT_FALSE(modal::shows(dialog,
                              "The port keeps these words in its own settings. A word left empty runs as it ran last "
                              "time, and 0 clears it."));
    EXPECT_EQ(footer(), "compatflags not set  ·  compatflags2 not set  ·  zacompatflags not set");
}

TEST_F(FlagsDialogTest, AClickOnTheTextTicksAndOnePastItDoesNot) {
    show(Flags::Family::gameplay, Flags::Set::gzdoom);

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
    show(Flags::Family::gameplay, Flags::Set::eternity);

    const double few = dialog->card()->box().h;

    show(Flags::Family::gameplay, Flags::Set::gzdoom);

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
    show(Flags::Family::gameplay, Flags::Set::gzdoom, typed(Flags::Word::dmflags, "4096"));

    ttk::MultistateSwitch *falling = modal::all<ttk::MultistateSwitch>(dialog).front();

    reveal(falling);

    // Strife, the last of Map, ZDoom, Hexen and Strife.
    canvas.ui().focus(falling);
    canvas.ui().key(ttk::Key{.code = ttk::Code::End});
    turn();

    EXPECT_EQ(falling->current(), 24);

    press(button("Save"));

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), (FlagsDialog::Edited{{Flags::Word::dmflags, "4120"}}));
}

TEST_F(RevealedFlagsDialogTest, AWordThatStartsFullLosesWhatIsUnticked) {
    show(Flags::Family::gameplay, Flags::Set::zandronum);

    ttk::Check *chainsaw = check("Chainsaw");

    ASSERT_NE(chainsaw, nullptr);
    EXPECT_TRUE(chainsaw->checked()) << "every weapon is allowed until the word says otherwise";

    reveal(chainsaw);
    tick(chainsaw);
    press(button("Save"));

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), (FlagsDialog::Edited{{Flags::Word::lmsWeapons, "511"}}));
}

TEST_F(FlagsDialogTest, ATickAfterClearAllWritesOnlyThatWord) {
    show(Flags::Family::gameplay, Flags::Set::gzdoom, typed(Flags::Word::dmflags2, "64"));
    press(button("Clear all"));
    tick(check("No monsters"));
    press(button("Save"));

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(),
              (FlagsDialog::Edited{
                      {Flags::Word::dmflags, "4096"}, {Flags::Word::dmflags2, ""}, {Flags::Word::dmflags3, ""}}));
}

TEST_F(FlagsDialogTest, ZandronumNamesItsOwnCoopCvars) {
    show(Flags::Family::gameplay, Flags::Set::zandronum);

    EXPECT_TRUE(check("Lose keys")->hint.ends_with("sv_coop_losekeys."));
}
