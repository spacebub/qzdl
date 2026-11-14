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
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/layout/Scroll.h"

#include "Dialogs.h"
#include "gui/dialogs/EnvironmentDialog.h"

namespace {

using dialogs::EnvironmentDialog;
using Variables = std::vector<EnvVariable>;

class EnvironmentDialogTest : public modal::Dialogued {
protected:
    std::vector<Variables> answers;

    EnvironmentDialog *dialog = nullptr;

    void show(Variables variables) {
        dialog = open(std::make_unique<EnvironmentDialog>(
                std::move(variables), [this](Variables edited) { answers.push_back(std::move(edited)); }));
    }

    [[nodiscard]] std::vector<ttk::Field *> fields() const { return modal::all<ttk::Field>(dialog); }

    [[nodiscard]] ttk::Button *button(const size_t at) const { return modal::all<ttk::Button>(dialog).at(at); }

    [[nodiscard]] ttk::Button *add() const { return button(0); }

    [[nodiscard]] ttk::Button *save() const { return button(2); }

    void write(const ttk::Field *field, const std::string &text) {
        canvas.ui().focus(field->input());
        canvas.ui().wrote(text);
    }

    [[nodiscard]] bool says(const std::string &text) const {
        return std::ranges::any_of(modal::all<ttk::Label>(dialog), [&text](const ttk::Label *label) {
            return label->text() == text && label->visible();
        });
    }

    void press(const ttk::Widget *widget) {
        turn();
        modal::click(canvas.ui(), widget);
        turn();
    }
};

}

TEST_F(EnvironmentDialogTest, ShowsTheVariablesItWasGiven) {
    show({{.name = "A", .value = "1"}, {.name = "B", .value = "two"}});

    ASSERT_EQ(fields().size(), 4U);
    EXPECT_EQ(fields()[0]->text(), "A");
    EXPECT_EQ(fields()[1]->text(), "1");
    EXPECT_EQ(fields()[2]->text(), "B");
    EXPECT_EQ(fields()[3]->text(), "two");
    EXPECT_FALSE(says("No variables yet."));
}

TEST_F(EnvironmentDialogTest, NoVariablesSaysSo) {
    show({});

    EXPECT_TRUE(fields().empty());
    EXPECT_TRUE(says("No variables yet."));
}

TEST_F(EnvironmentDialogTest, AnAddedRowIsSaved) {
    show({});
    press(add());

    ASSERT_EQ(fields().size(), 2U);
    EXPECT_EQ(canvas.ui().focused(), fields()[0]->input());

    write(fields()[0], "MANGOHUD");
    write(fields()[1], "1");
    press(save());

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), (Variables{{.name = "MANGOHUD", .value = "1"}}));
}

TEST_F(EnvironmentDialogTest, RemoveDropsItsRow) {
    show({{.name = "A", .value = "1"}, {.name = "B", .value = "2"}});
    press(modal::all<ttk::GlyphButton>(dialog).front());

    ASSERT_EQ(fields().size(), 2U);
    EXPECT_EQ(fields()[0]->text(), "B");

    press(save());

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), (Variables{{.name = "B", .value = "2"}}));
}

TEST_F(EnvironmentDialogTest, SavingTrimsNamesDropsEmptyOnesAndKeepsTheLastOfAName) {
    show({{.name = " A ", .value = "1"},
          {.name = "", .value = "lost"},
          {.name = "A", .value = "2"},
          {.name = "B", .value = "3"}});
    press(save());

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), (Variables{{.name = "A", .value = "2"}, {.name = "B", .value = "3"}}));
}

TEST_F(EnvironmentDialogTest, ANameThatCannotBeSetHoldsSaveBack) {
    show({{.name = "A=B", .value = "1"}});

    EXPECT_FALSE(save()->enabled());
    EXPECT_TRUE(says("A name cannot hold a space or an equals sign."));

    canvas.ui().focus(fields()[0]->input());
    canvas.ui().key(ttk::Key{.code = 'a', .ctrl = true});
    canvas.ui().wrote("AB");
    turn();

    EXPECT_TRUE(save()->enabled());
}

TEST_F(EnvironmentDialogTest, CancelSavesNothing) {
    show({{.name = "A", .value = "1"}});
    press(button(1));

    EXPECT_TRUE(answers.empty());
}

TEST_F(EnvironmentDialogTest, ReturnInAValueSaves) {
    show({{.name = "A", .value = "1"}});
    canvas.ui().focus(fields()[1]->input());
    key(ttk::Code::Return);

    ASSERT_EQ(answers.size(), 1U);
}

TEST_F(EnvironmentDialogTest, ARowAddedPastSixScrollsIntoView) {
    Variables many;

    for (int at = 0; at < 7; ++at) {
        many.push_back({.name = "V" + std::to_string(at), .value = ""});
    }

    show(std::move(many));
    press(add());
    turn();

    const ttk::Scroll *scroll = modal::all<ttk::Scroll>(dialog).front();
    const BLRect view = scroll->box();
    const BLRect last = fields().at(fields().size() - 2)->box();

    EXPECT_GE(last.y, view.y);
    EXPECT_LE(last.y + last.h, view.y + view.h + 0.5) << "the new row sits below the view";
}

TEST_F(EnvironmentDialogTest, TypingIntoTheRowsRepaintsLikeAFullFrame) {
    show({{.name = "A", .value = "1"}, {.name = "B", .value = "2"}});

    for (ttk::Field *field : fields()) {
        canvas.ui().focus(field->input());
        canvas.ui().wrote("xyz");
        turn();
    }

    EXPECT_TRUE(canvas.matchesFull());
}

TEST_F(EnvironmentDialogTest, AddingAndRemovingRowsRepaintsLikeAFullFrame) {
    show({{.name = "A", .value = "1"}});
    press(add());
    canvas.ui().wrote("NEW");
    turn();
    press(modal::all<ttk::GlyphButton>(dialog).front());
    turn();

    EXPECT_TRUE(canvas.matchesFull());
}

TEST_F(EnvironmentDialogTest, PastSixRowsTheListScrolls) {
    Variables many;

    for (int at = 0; at < 8; ++at) {
        many.push_back({.name = "V" + std::to_string(at), .value = ""});
    }

    show(std::move(many));

    const ttk::Scroll *scroll = modal::all<ttk::Scroll>(dialog).front();

    EXPECT_DOUBLE_EQ(scroll->box().h, (6.0 * ttk::Theme::control) + (5.0 * 8.0));
    EXPECT_TRUE(scroll->scrollable());
}
