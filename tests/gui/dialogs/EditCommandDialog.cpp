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
#include <vector>

#include <gtest/gtest.h>

#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/Chip.h"
#include "ttk/toolkit/controls/TextEdit.h"

#include "Dialogs.h"
#include "core/launch/Command.h"
#include "gui/dialogs/EditCommandDialog.h"

namespace {

using dialogs::EditCommandDialog;

class EditCommandDialogTest : public modal::Dialogued {
protected:
    std::vector<std::string> answers;

    EditCommandDialog *dialog = nullptr;

    void show(const std::string &command) {
        dialog = open(std::make_unique<EditCommandDialog>(
                command, [this](const std::string &edited) { answers.push_back(edited); }));
    }

    [[nodiscard]] ttk::TextEdit *editor() const { return modal::all<ttk::TextEdit>(dialog).front(); }

    [[nodiscard]] ttk::Button *cancel() const { return modal::all<ttk::Button>(dialog).at(0); }

    [[nodiscard]] ttk::Button *save() const { return modal::all<ttk::Button>(dialog).at(1); }

    void press(const ttk::Widget *widget) {
        turn();
        modal::click(canvas.ui(), widget);
        turn();
    }
};

}

TEST_F(EditCommandDialogTest, TheEditorHoldsTheCommandAndHasTheFocus) {
    show("{source_port} -iwad {game}");

    EXPECT_EQ(editor()->text(), "{source_port} -iwad {game}");
    EXPECT_EQ(canvas.ui().focused(), editor());
}

TEST_F(EditCommandDialogTest, SaveHandsBackTheCommandExactlyAsTyped) {
    show("");
    canvas.ui().focus(editor());

    for (const char letter : std::string("{source_port}  'a  b' \"c\" ")) {
        if (letter == ' ') {
            canvas.ui().key(ttk::Key{.code = ttk::Code::Space});
        }

        canvas.ui().wrote(std::string(1, letter));
    }

    press(save());

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), "{source_port}  'a  b' \"c\" ");
}

TEST_F(EditCommandDialogTest, ReturnSavesAndTypesNoLineBreak) {
    show("{source_port} -fast");
    canvas.ui().focus(editor());
    key(ttk::Code::Return);

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), "{source_port} -fast");
}

TEST_F(EditCommandDialogTest, EveryPlaceholderIsOfferedWithItsExplanation) {
    show("");

    const std::vector<ttk::Chip *> chips = modal::all<ttk::Chip>(dialog);

    ASSERT_EQ(chips.size(), Command::tokens().size());

    for (size_t at = 0; at < chips.size(); ++at) {
        EXPECT_EQ(chips[at]->hint, Command::tokens()[at].about);
    }
}

TEST_F(EditCommandDialogTest, CancelSavesNothing) {
    show("{source_port}");
    press(cancel());

    EXPECT_TRUE(answers.empty());
}
