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

#include "ttk/dialogs/FilePicker.h"
#include "ttk/notices/Notifier.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/Toggle.h"

#include "Dialogs.h"
#include "gui/dialogs/EntryDialog.h"
#include "gui/services/Filters.h"
#include "support/Corpus.h"

namespace {

using dialogs::EntryDialog;

struct Answer {
    std::string name;
    std::string file;
    bool dos = false;
};

class EntryDialogTest : public modal::Dialogued {
protected:
    ttk::Notifier notifier;
    ttk::FilePicker files{&notifier};

    std::vector<Answer> answers;

    EntryDialog *dialog = nullptr;
    ttk::Field *file = nullptr;
    ttk::Field *name = nullptr;
    ttk::Toggle *dos = nullptr;
    ttk::Button *cancel = nullptr;
    ttk::Button *save = nullptr;

    void show(const std::string &named,
              const std::string &path,
              const EntryDialog::Kind kind = EntryDialog::Kind::Game,
              const bool offerDos = false,
              const bool dosbox = false) {
        dialog = open(std::make_unique<EntryDialog>("Edit Doom II",
                                                    kind,
                                                    Filters::wad(),
                                                    LastDir::WAD,
                                                    named,
                                                    path,
                                                    offerDos,
                                                    dosbox,
                                                    files,
                                                    [this](const std::string &n, const std::string &f, const bool d) {
                                                        answers.push_back({.name = n, .file = f, .dos = d});
                                                    }));

        const std::vector<ttk::Field *> fields = modal::all<ttk::Field>(dialog);
        const std::vector<ttk::Button *> buttons = modal::all<ttk::Button>(dialog);
        const std::vector<ttk::Toggle *> toggles = modal::all<ttk::Toggle>(dialog);

        ASSERT_EQ(fields.size(), 2U);
        ASSERT_GE(buttons.size(), 2U);
        ASSERT_EQ(toggles.size(), 1U);

        file = fields[0];
        name = fields[1];
        dos = toggles[0];
        cancel = buttons[buttons.size() - 2];
        save = buttons.back();
    }

    ttk::GlyphButton *browse() const { return modal::hinted<ttk::GlyphButton>(dialog, "Browse"); }
};

TEST_F(EntryDialogTest, TheTitleIsTheHeading) {
    ASSERT_NO_FATAL_FAILURE(show("Doom II", "/games/doom2.wad"));

    EXPECT_TRUE(modal::shows(dialog, "Edit Doom II"));
}

TEST_F(EntryDialogTest, TheFieldsHoldWhatItWasOpenedWith) {
    ASSERT_NO_FATAL_FAILURE(show("Doom II", "/games/doom2.wad"));

    EXPECT_EQ(file->text(), "/games/doom2.wad");
    EXPECT_EQ(name->text(), "Doom II");
}

TEST_F(EntryDialogTest, TheNameHasFocusOnceOpen) {
    ASSERT_NO_FATAL_FAILURE(show("Doom II", "/games/doom2.wad"));

    EXPECT_EQ(canvas.ui().focused(), name->input());
}

TEST_F(EntryDialogTest, SaveIsOnWithAFile) {
    ASSERT_NO_FATAL_FAILURE(show("Doom II", "/games/doom2.wad"));

    EXPECT_TRUE(save->enabled());
    EXPECT_TRUE(cancel->enabled());
}

TEST_F(EntryDialogTest, SaveIsOffWithoutAFile) {
    ASSERT_NO_FATAL_FAILURE(show("Doom II", ""));

    EXPECT_FALSE(save->enabled());
}

TEST_F(EntryDialogTest, SaveIsOffForAFileOfSpaces) {
    ASSERT_NO_FATAL_FAILURE(show("Doom II", "   "));

    EXPECT_FALSE(save->enabled());
}

TEST_F(EntryDialogTest, TypingAFileTurnsSaveOn) {
    ASSERT_NO_FATAL_FAILURE(show("", ""));

    canvas.ui().focus(file->input());
    canvas.ui().wrote("/games/tnt.wad");

    EXPECT_TRUE(save->enabled());
}

TEST_F(EntryDialogTest, ErasingTheFileTurnsSaveOff) {
    ASSERT_NO_FATAL_FAILURE(show("", "x"));

    canvas.ui().focus(file->input());
    key(ttk::Code::End);
    key(ttk::Code::Backspace);

    EXPECT_EQ(file->text(), "");
    EXPECT_FALSE(save->enabled());
}

TEST_F(EntryDialogTest, SaveAnswersTheNameAndTheFile) {
    ASSERT_NO_FATAL_FAILURE(show("Doom II", "/games/doom2.wad"));

    modal::click(canvas.ui(), save);

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front().name, "Doom II");
    EXPECT_EQ(answers.front().file, "/games/doom2.wad");
    EXPECT_FALSE(answers.front().dos);
}

TEST_F(EntryDialogTest, SaveTakesTheDialogDown) {
    ASSERT_NO_FATAL_FAILURE(show("Doom II", "/games/doom2.wad"));

    modal::click(canvas.ui(), save);

    EXPECT_FALSE(layer->covered());
}

TEST_F(EntryDialogTest, WhatIsSavedIsTrimmed) {
    ASSERT_NO_FATAL_FAILURE(show("  Doom II ", " /games/doom2.wad  "));

    modal::click(canvas.ui(), save);

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front().name, "Doom II");
    EXPECT_EQ(answers.front().file, "/games/doom2.wad");
}

TEST_F(EntryDialogTest, WhatWasTypedIsWhatIsSaved) {
    ASSERT_NO_FATAL_FAILURE(show("Doom II", "/games/doom2.wad"));

    canvas.ui().wrote("Hell on Earth");
    modal::click(canvas.ui(), save);

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front().name, "Hell on Earth");
}

TEST_F(EntryDialogTest, AnEmptyNameIsSavedForTheFileToNameIt) {
    ASSERT_NO_FATAL_FAILURE(show("", "/games/doom2.wad"));

    modal::click(canvas.ui(), save);

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front().name, "");
}

TEST_F(EntryDialogTest, ReturnInTheNameSaves) {
    ASSERT_NO_FATAL_FAILURE(show("Doom II", "/games/doom2.wad"));

    key(ttk::Code::Return);

    EXPECT_EQ(answers.size(), 1U);
    EXPECT_FALSE(layer->covered());
}

TEST_F(EntryDialogTest, ReturnWithNoFileIsRefused) {
    ASSERT_NO_FATAL_FAILURE(show("Doom II", ""));

    key(ttk::Code::Return);

    EXPECT_TRUE(answers.empty());
    EXPECT_TRUE(layer->covered());
}

TEST_F(EntryDialogTest, ADisabledSaveIgnoresAClick) {
    ASSERT_NO_FATAL_FAILURE(show("Doom II", ""));

    modal::click(canvas.ui(), save);

    EXPECT_TRUE(answers.empty());
    EXPECT_TRUE(layer->covered());
}

TEST_F(EntryDialogTest, CancelTakesTheDialogDownWithoutAnAnswer) {
    ASSERT_NO_FATAL_FAILURE(show("Doom II", "/games/doom2.wad"));

    modal::click(canvas.ui(), cancel);

    EXPECT_TRUE(answers.empty());
    EXPECT_FALSE(layer->covered());
}

TEST_F(EntryDialogTest, TheDosSwitchIsHiddenUnlessOffered) {
    ASSERT_NO_FATAL_FAILURE(show("GZDoom", "/ports/gzdoom", EntryDialog::Kind::Port));

    EXPECT_FALSE(dos->visible());
}

TEST_F(EntryDialogTest, TheDosSwitchShowsWhatTheEntryIs) {
    ASSERT_NO_FATAL_FAILURE(show("Doom", "/ports/doom.exe", EntryDialog::Kind::Port, true, true));

    EXPECT_TRUE(dos->visible());
    EXPECT_TRUE(dos->checked);
}

TEST_F(EntryDialogTest, ADosEntryIsSavedAsOne) {
    ASSERT_NO_FATAL_FAILURE(show("Doom", "/ports/doom.exe", EntryDialog::Kind::Port, true, true));

    modal::click(canvas.ui(), save);

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_TRUE(answers.front().dos);
}

TEST_F(EntryDialogTest, DosNotOfferedIsNeverSaved) {
    ASSERT_NO_FATAL_FAILURE(show("Doom", "/ports/doom.exe", EntryDialog::Kind::Port, false, true));

    modal::click(canvas.ui(), save);

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_FALSE(answers.front().dos);
}

TEST_F(EntryDialogTest, FlippingTheDosSwitchIsSaved) {
    ASSERT_NO_FATAL_FAILURE(show("Doom", "/ports/doom.exe", EntryDialog::Kind::Port, true, false));

    // The switch answers on its track and label, which start at the left of its row.
    modal::click(canvas.ui(), dos->box().x + 12.0, modal::midY(dos->box()));

    EXPECT_TRUE(dos->checked);

    modal::click(canvas.ui(), save);

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_TRUE(answers.front().dos);
}

TEST_F(EntryDialogTest, BrowseOpensThePickerOnTheDialogsTitle) {
    ASSERT_NO_FATAL_FAILURE(show("", ""));
    ASSERT_NE(browse(), nullptr);

    modal::click(canvas.ui(), browse());

    EXPECT_TRUE(files.state().open);
    EXPECT_EQ(files.state().title, "Edit Doom II");
}

TEST_F(EntryDialogTest, APickedGameIsNamedFromTheFile) {
    ASSERT_NO_FATAL_FAILURE(show("", ""));

    modal::click(canvas.ui(), browse());
    files.choose({tests::Corpus::iwad().string()});

    EXPECT_EQ(file->text(), tests::Corpus::iwad().string());
    EXPECT_EQ(name->text(), "Test Doom");
    EXPECT_TRUE(save->enabled());
}

TEST_F(EntryDialogTest, APickedPortIsNamedFromTheFile) {
    ASSERT_NO_FATAL_FAILURE(show("", "", EntryDialog::Kind::Port, true));

    modal::click(canvas.ui(), browse());
    files.choose({"/opt/ports/gzdoom"});

    EXPECT_EQ(name->text(), "GZDoom");
}

TEST_F(EntryDialogTest, APickKeepsANameAlreadyGiven) {
    ASSERT_NO_FATAL_FAILURE(show("Mine", ""));

    modal::click(canvas.ui(), browse());
    files.choose({tests::Corpus::iwad().string()});

    EXPECT_EQ(name->text(), "Mine");
}

TEST_F(EntryDialogTest, APickArrivingAfterTheDialogClosedIsDropped) {
    ASSERT_NO_FATAL_FAILURE(show("", ""));

    modal::click(canvas.ui(), browse());
    layer->dismiss();

    files.choose({tests::Corpus::iwad().string()});

    EXPECT_FALSE(layer->covered());
    EXPECT_TRUE(answers.empty());
}

TEST_F(EntryDialogTest, SetFileSavesThatFile) {
    ASSERT_NO_FATAL_FAILURE(show("Doom II", ""));

    dialog->setFile("/games/doom2.wad");
    modal::click(canvas.ui(), save);

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front().file, "/games/doom2.wad");
}

}
