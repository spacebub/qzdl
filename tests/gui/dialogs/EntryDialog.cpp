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
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/dialogs/FilePicker.h"
#include "ttk/notices/Notifier.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/Select.h"
#include "ttk/toolkit/controls/Toggle.h"

#include "Dialogs.h"
#include "core/wad/FileInfo.h"
#include "gui/dialogs/EntryDialog.h"
#include "gui/services/Filters.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

using dialogs::EntryDialog;

const std::string HINT = "Until a version is picked, ZDL4 treats this port as the latest release.";

struct Answer {
    std::string name;
    std::string file;
    bool dos = false;
    std::string dialect;
    std::string version;
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
    ttk::Select *dialect = nullptr;
    ttk::Select *version = nullptr;
    ttk::Label *versionHint = nullptr;
    ttk::Button *cancel = nullptr;
    ttk::Button *save = nullptr;

    void show(const std::string &named, const std::string &path) {
        mount(std::make_unique<EntryDialog>(
                "Edit Doom II", GameEntry{.name = named, .file = path}, files, [this](const GameEntry &game) {
                    answers.push_back(
                            {.name = game.name, .file = game.file, .dos = false, .dialect = "", .version = ""});
                }));
    }

    void showPort(PortEntry port, const bool adding = false) {
        mount(std::make_unique<EntryDialog>(
                "Edit Doom II", std::move(port), adding, files, [this](const PortEntry &edited) {
                    answers.push_back({.name = edited.name,
                                       .file = edited.file,
                                       .dos = edited.dosbox,
                                       .dialect = edited.dialect,
                                       .version = edited.version});
                }));
    }

    void mount(std::unique_ptr<EntryDialog> made) {
        dialog = open(std::move(made));

        const std::vector<ttk::Field *> fields = modal::all<ttk::Field>(dialog);
        const std::vector<ttk::Button *> buttons = modal::all<ttk::Button>(dialog);
        const std::vector<ttk::Toggle *> toggles = modal::all<ttk::Toggle>(dialog);
        const std::vector<ttk::Select *> selects = modal::all<ttk::Select>(dialog);

        ASSERT_EQ(fields.size(), 2U);
        ASSERT_GE(buttons.size(), 2U);
        ASSERT_EQ(toggles.size(), 1U);
        ASSERT_EQ(selects.size(), 2U);

        file = fields[0];
        name = fields[1];
        dos = toggles[0];
        dialect = selects[0];
        version = selects[1];

        for (ttk::Label *each : modal::all<ttk::Label>(dialog)) {
            if (each->text() == HINT) {
                versionHint = each;
            }
        }

        ASSERT_NE(versionHint, nullptr);
        cancel = buttons[buttons.size() - 2];
        save = buttons.back();
    }

    void pickNextDialect() {
        canvas.ui().focus(dialect);
        key(ttk::Code::Down);
    }

    void browseTo(const std::string &path) {
        modal::click(canvas.ui(), browse());
        files.choose({path});
        turn();
    }

    Answer saved() {
        turn();
        modal::click(canvas.ui(), save);

        return answers.empty() ? Answer{} : answers.back();
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
    ASSERT_NO_FATAL_FAILURE(show("Doom II", "x"));

    canvas.ui().focus(file->input());
    key(ttk::Code::End);
    key(ttk::Code::Backspace);

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

TEST_F(EntryDialogTest, AGameHasNoDosSwitchOrDialect) {
    ASSERT_NO_FATAL_FAILURE(show("Doom II", "/games/doom2.wad"));

    EXPECT_FALSE(dos->visible());
    EXPECT_FALSE(dialect->visible());
}

TEST_F(EntryDialogTest, TheDosSwitchShowsWhatTheEntryIs) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Doom", .file = "/ports/doom.exe", .dosbox = true}));

    EXPECT_TRUE(dos->visible());
    EXPECT_TRUE(dos->checked);
}

TEST_F(EntryDialogTest, ADosEntryIsSavedAsOne) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Doom", .file = "/ports/doom.exe", .dosbox = true}));

    EXPECT_TRUE(saved().dos);
}

TEST_F(EntryDialogTest, FlippingTheDosSwitchIsSaved) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Doom", .file = "/ports/doom.exe"}));

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
    ASSERT_NO_FATAL_FAILURE(showPort({}));

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

TEST_F(EntryDialogTest, ANameThatOnlySaysWhatTheFileWouldFollowsANewPick) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = FileInfo::describePort("/opt/ports/woof"), .file = "/opt/ports/woof"}));

    modal::click(canvas.ui(), browse());
    files.choose({"/opt/ports/gzdoom"});

    EXPECT_EQ(name->text(), "GZDoom");
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
    turn();
    modal::click(canvas.ui(), save);

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front().file, "/games/doom2.wad");
}

}

namespace {

// Where the dialect select lists a dialect, with or without DOSBox.
int listed(const std::string_view id, const bool dos) {
    int at = 0;

    for (const Dialect::Choice &each : Dialect::choices()) {
        if (each.dos != dos) {
            continue;
        }

        if (each.word == id) {
            return at;
        }

        ++at;
    }

    return -1;
}

}

TEST_F(EntryDialogTest, APortWithNoDialectShowsTheOneItsNameSpeaks) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "DSDA", .file = "/ports/dsda-doom"}));

    EXPECT_TRUE(dialect->visible());
    EXPECT_EQ(dialect->current(), listed("dsda", false));
    EXPECT_EQ(saved().dialect, "");
}

TEST_F(EntryDialogTest, AnUnknownNameShowsTheZdoomGuess) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Mine", .file = "/ports/mystery-engine"}));

    EXPECT_EQ(dialect->current(), listed("gzdoom", false));
    EXPECT_EQ(saved().dialect, "");
}

TEST_F(EntryDialogTest, AStoredDialectIsShownAndKept) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Mine", .file = "/ports/engine", .dialect = "woof"}));

    EXPECT_EQ(dialect->current(), listed("woof", false));
    EXPECT_EQ(saved().dialect, "woof");
}

TEST_F(EntryDialogTest, AStoredVersionIsKept) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Mine", .file = "/ports/dsda-doom", .version = "0.27.0"}));

    EXPECT_EQ(saved().version, "0.27.0");
}

namespace {

std::span<const Dialect::VersionRange> dsdaRanges() {
    return Dialect::versions({.name = "DSDA", .file = "/ports/dsda-doom"});
}

}

TEST_F(EntryDialogTest, APortWithReleasesOffersTheVersions) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "DSDA", .file = "/ports/dsda-doom"}));

    EXPECT_TRUE(modal::seen(version));
    EXPECT_EQ(version->current(), -1);
}

TEST_F(EntryDialogTest, APortWithoutReleasesOffersNoVersion) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "GZ", .file = "/ports/gzdoom"}));

    EXPECT_FALSE(modal::seen(version));
}

TEST_F(EntryDialogTest, NoVersionIsOfferedBeforeAFileIsPicked) {
    ASSERT_NO_FATAL_FAILURE(showPort({}, true));

    EXPECT_FALSE(modal::seen(version));
    EXPECT_FALSE(modal::seen(versionHint));
}

TEST_F(EntryDialogTest, ADownloadedPortIsNotAskedForAVersion) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "DSDA", .file = "/data/ports/dsda/dsda-doom", .portId = "dsda"}, true));

    EXPECT_FALSE(modal::seen(version));
    EXPECT_FALSE(modal::seen(versionHint));
    EXPECT_TRUE(save->enabled());
}

TEST_F(EntryDialogTest, ADownloadedPortKeepsTheVersionItWasGiven) {
    ASSERT_NO_FATAL_FAILURE(
            showPort({.name = "DSDA", .file = "/data/ports/dsda/dsda-doom", .portId = "dsda", .version = "0.28.0"}));

    EXPECT_EQ(saved().version, "0.28.0");
}

TEST_F(EntryDialogTest, ADosboxProgramIsNotAskedForAVersion) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "DSDA", .file = "/ports/dsda-doom", .dosbox = true}, true));

    EXPECT_FALSE(modal::seen(version));
    EXPECT_TRUE(save->enabled());
}

TEST_F(EntryDialogTest, TheVersionListFollowsTheDosboxSwitch) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "DSDA", .file = "/ports/dsda-doom", .version = "0.28.0"}));

    modal::click(canvas.ui(), dos->box().x + 12.0, modal::midY(dos->box()));

    EXPECT_FALSE(modal::seen(version));

    modal::click(canvas.ui(), dos->box().x + 12.0, modal::midY(dos->box()));

    EXPECT_TRUE(modal::seen(version));
    EXPECT_EQ(version->current(), -1);
}

TEST_F(EntryDialogTest, TheVersionListFollowsTheDialect) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Mine", .file = "/ports/gzdoom"}));

    EXPECT_FALSE(modal::seen(version));

    canvas.ui().focus(dialect);

    while (!modal::seen(version) && dialect->current() < static_cast<int>(Dialect::choices().size()) - 1) {
        key(ttk::Code::Down);
    }

    EXPECT_EQ(Dialect::choices()[static_cast<size_t>(dialect->current())].id, Dialect::Id::dsda);
    EXPECT_TRUE(modal::seen(version));
}

TEST_F(EntryDialogTest, TheVersionListFollowsTheFile) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Mine", .file = "/ports/gzdoom"}));

    browseTo("/opt/ports/dsda-doom");

    EXPECT_TRUE(modal::seen(version));

    browseTo("/opt/ports/gzdoom");

    EXPECT_FALSE(modal::seen(version));
}

TEST_F(EntryDialogTest, AStoredVersionIsShown) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "DSDA", .file = "/ports/dsda-doom", .version = "0.27.0"}));

    const std::span<const Dialect::VersionRange> ranges = dsdaRanges();
    const auto at = std::ranges::find(ranges, std::string_view("0.27.0"), &Dialect::VersionRange::first);

    ASSERT_NE(at, ranges.end());
    EXPECT_EQ(version->current(), static_cast<int>(at - ranges.begin()));
}

TEST_F(EntryDialogTest, ThePickedVersionIsSavedAndTheNewestIsFirst) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "DSDA", .file = "/ports/dsda-doom"}));

    canvas.ui().focus(version);
    key(ttk::Code::Down);

    const std::span<const Dialect::VersionRange> ranges = dsdaRanges();

    ASSERT_FALSE(ranges.empty());
    EXPECT_EQ(version->current(), 0);
    EXPECT_EQ(saved().version, std::string(ranges.front().first));
}

TEST_F(EntryDialogTest, AnotherVersionReplacesTheStoredOne) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "DSDA", .file = "/ports/dsda-doom", .version = "0.28.0"}));

    canvas.ui().focus(version);
    key(ttk::Code::Up);

    EXPECT_EQ(saved().version, "0.30.0");
}

TEST_F(EntryDialogTest, TheHintShowsUntilAVersionIsPicked) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "DSDA", .file = "/ports/dsda-doom"}));

    EXPECT_TRUE(modal::seen(versionHint));

    canvas.ui().focus(version);
    key(ttk::Code::Down);

    EXPECT_FALSE(modal::seen(versionHint));
}

TEST_F(EntryDialogTest, AddingAPortWithReleasesNeedsAVersionToSave) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "DSDA", .file = "/ports/dsda-doom"}, true));

    EXPECT_FALSE(save->enabled());

    modal::click(canvas.ui(), save);

    EXPECT_TRUE(answers.empty());

    canvas.ui().focus(version);
    key(ttk::Code::Down);

    EXPECT_TRUE(save->enabled());
    EXPECT_FALSE(saved().version.empty());
}

TEST_F(EntryDialogTest, AddingAPortWithoutReleasesNeedsNoVersion) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "GZ", .file = "/ports/gzdoom"}, true));

    EXPECT_TRUE(save->enabled());
}

TEST_F(EntryDialogTest, APickedFileDecidesWhetherAddingNeedsAVersion) {
    ASSERT_NO_FATAL_FAILURE(showPort({}, true));

    browseTo("/opt/ports/dsda-doom");

    EXPECT_FALSE(save->enabled());

    browseTo("/opt/ports/gzdoom");

    EXPECT_TRUE(save->enabled());
}

TEST_F(EntryDialogTest, ReturnWhileAddingWithoutAVersionMovesToTheVersion) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "DSDA", .file = "/ports/dsda-doom"}, true));

    key(ttk::Code::Return);

    EXPECT_TRUE(answers.empty());
    EXPECT_TRUE(layer->covered());
    EXPECT_EQ(canvas.ui().focused(), version);
}

TEST_F(EntryDialogTest, EditingAPortWithNoVersionCanStillSave) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "DSDA", .file = "/ports/dsda-doom"}, false));

    EXPECT_TRUE(save->enabled());
    EXPECT_TRUE(modal::seen(versionHint));

    canvas.ui().wrote("2");
    key(ttk::Code::Return);

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front().version, "");
}

TEST_F(EntryDialogTest, AVersionTheFileNoLongerOffersIsCleared) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "DSDA", .file = "/ports/dsda-doom", .version = "0.28.0"}));

    browseTo("/opt/ports/gzdoom");

    EXPECT_EQ(saved().version, "");
}

TEST_F(EntryDialogTest, AVersionOnAFileOfAnotherPortIsNotKept) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "GZ", .file = "/ports/gzdoom", .version = "0.28.0"}));

    EXPECT_EQ(saved().version, "");
}

TEST_F(EntryDialogTest, AVersionNoRangeNamesIsClearedAndAsked) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "DSDA", .file = "/ports/dsda-doom", .version = "9.9.9"}, true));

    EXPECT_EQ(version->current(), -1);
    EXPECT_TRUE(modal::seen(versionHint));
    EXPECT_FALSE(save->enabled());
}

TEST_F(EntryDialogTest, APickedFileShowsItsDialect) {
    ASSERT_NO_FATAL_FAILURE(showPort({}));

    browseTo("/opt/ports/nugget-doom");

    EXPECT_EQ(dialect->current(), listed("woof", false));
    EXPECT_EQ(saved().dialect, "");
}

TEST_F(EntryDialogTest, ATypedFileShowsItsDialect) {
    ASSERT_NO_FATAL_FAILURE(showPort({}));

    canvas.ui().focus(file->input());
    canvas.ui().wrote("/opt/ports/helion");

    EXPECT_EQ(dialect->current(), listed("helion", false));
}

TEST_F(EntryDialogTest, ANewFileShowsItsDialectWhenNoneWasPicked) {
    ASSERT_NO_FATAL_FAILURE(showPort({}));

    browseTo("/opt/ports/woof");
    browseTo("/opt/ports/dsda-doom");

    EXPECT_EQ(dialect->current(), listed("dsda", false));
}

TEST_F(EntryDialogTest, ADialectPickedByHandSurvivesANewFile) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Mine", .file = "/ports/gzdoom"}));

    pickNextDialect();
    browseTo("/opt/ports/woof");

    EXPECT_EQ(dialect->current(), listed("gzdoom", false) + 1);
    EXPECT_EQ(saved().dialect, Dialect::choices()[1].word);
}

TEST_F(EntryDialogTest, AStoredDialectSurvivesANewFile) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Mine", .file = "/ports/engine", .dialect = "dsda"}));

    browseTo("/opt/ports/woof");

    EXPECT_EQ(saved().dialect, "dsda");
}

TEST_F(EntryDialogTest, AnUnknownDosProgramShowsVanilla) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Doom", .file = "/ports/game.exe", .dosbox = true}));

    EXPECT_EQ(dialect->current(), listed("doom", true));
}

TEST_F(EntryDialogTest, OnlyDosDialectsAreOfferedUnderDosbox) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Doom", .file = "/ports/game.exe", .dosbox = true}));

    canvas.ui().focus(dialect);
    key(ttk::Code::Up);

    const Answer answer = saved();

    ASSERT_NE(Dialect::choice(answer.dialect), nullptr);
    EXPECT_TRUE(Dialect::choice(answer.dialect)->dos);
}

TEST_F(EntryDialogTest, TurningOnDosboxDropsANativeDialect) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Mine", .file = "/ports/engine", .dialect = "woof"}));

    modal::click(canvas.ui(), dos->box().x + 12.0, modal::midY(dos->box()));

    EXPECT_EQ(dialect->current(), listed("doom", true));
    EXPECT_EQ(saved().dialect, "");
}

TEST_F(EntryDialogTest, ADownloadedPortShowsItsDialectLocked) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Woof!", .file = "/data/ports/woof/woof", .portId = "woof"}));

    EXPECT_FALSE(dialect->enabled());
    EXPECT_EQ(dialect->current(), listed("woof", false));
    EXPECT_EQ(saved().dialect, "");
}

TEST_F(EntryDialogTest, ANewPortShowsOnlyTheFile) {
    ASSERT_NO_FATAL_FAILURE(showPort({}));

    EXPECT_TRUE(file->visible());
    EXPECT_TRUE(cancel->visible());
    EXPECT_FALSE(name->visible());
    EXPECT_FALSE(dialect->visible());
    EXPECT_FALSE(dos->visible());
    EXPECT_FALSE(save->visible());
    EXPECT_EQ(canvas.ui().focused(), file->input());
}

TEST_F(EntryDialogTest, APickedFileShowsTheRest) {
    ASSERT_NO_FATAL_FAILURE(showPort({}));

    browseTo("/opt/ports/woof");

    EXPECT_TRUE(name->visible());
    EXPECT_TRUE(dialect->visible());
    EXPECT_TRUE(dos->visible());
    EXPECT_TRUE(save->visible());
    EXPECT_EQ(name->text(), "woof");
}

TEST_F(EntryDialogTest, ATypedPathShowsTheRestOnceItIsAFile) {
    ASSERT_NO_FATAL_FAILURE(showPort({}));

    canvas.ui().focus(file->input());
    canvas.ui().wrote("/no/such/port");

    EXPECT_FALSE(name->visible());

    key(ttk::Code::End);

    for (size_t at = 0; at < std::string("/no/such/port").size(); ++at) {
        key(ttk::Code::Backspace);
    }

    canvas.ui().wrote(tests::Corpus::iwad().string());

    EXPECT_TRUE(name->visible());
    EXPECT_TRUE(save->visible());
    EXPECT_FALSE(name->text().empty());
}

TEST_F(EntryDialogTest, AnEntryWithAFileShowsEveryRow) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Gone", .file = "/no/such/port"}));

    EXPECT_TRUE(name->visible());
    EXPECT_TRUE(dialect->visible());
    EXPECT_TRUE(dos->visible());
    EXPECT_TRUE(save->visible());
}

#ifndef _WIN32
TEST_F(EntryDialogTest, APickedDosProgramRunsUnderDosbox) {
    ASSERT_NO_FATAL_FAILURE(showPort({}));

    browseTo("/opt/ports/mbf.exe");

    EXPECT_TRUE(dos->checked);
    EXPECT_EQ(dialect->current(), listed("boom", true));

    const Answer answer = saved();

    EXPECT_TRUE(answer.dos);
    EXPECT_EQ(answer.dialect, "");
}

TEST_F(EntryDialogTest, APickedNativeProgramTurnsDosboxOff) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Old", .file = "/opt/ports/doom.exe", .dosbox = true}));

    browseTo("/opt/ports/woof");

    EXPECT_FALSE(dos->checked);
    EXPECT_EQ(dialect->current(), listed("woof", false));
}

TEST_F(EntryDialogTest, ADosboxSwitchFlippedByHandSurvivesANewFile) {
    ASSERT_NO_FATAL_FAILURE(showPort({}));

    browseTo("/opt/ports/doom.exe");
    modal::click(canvas.ui(), dos->box().x + 12.0, modal::midY(dos->box()));

    ASSERT_FALSE(dos->checked);

    browseTo("/opt/ports/doom2.exe");

    EXPECT_FALSE(dos->checked);
}
#endif

TEST_F(EntryDialogTest, ANativeProgramShowsTheDosDialectItSpeaks) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Legacy", .file = "/usr/bin/doomlegacy"}));

    const auto natives =
            std::ranges::count_if(Dialect::choices(), [](const Dialect::Choice &each) { return !each.dos; });

    EXPECT_EQ(dialect->current(), static_cast<int>(natives));
    EXPECT_EQ(Dialect::of(PortEntry{.name = "Legacy", .file = "/usr/bin/doomlegacy"}).id, Dialect::Id::legacy);
}

TEST_F(EntryDialogTest, AnotherPickRenamesAnUntypedName) {
    ASSERT_NO_FATAL_FAILURE(showPort({}));

    browseTo("/opt/ports/woof");
    browseTo("/opt/ports/dsda-doom");

    EXPECT_EQ(name->text(), "dsda-doom");
}

TEST_F(EntryDialogTest, ATypedNameSurvivesAnotherPick) {
    ASSERT_NO_FATAL_FAILURE(showPort({}));

    browseTo("/opt/ports/woof");
    canvas.ui().focus(name->input());
    key(ttk::Code::End);
    canvas.ui().wrote("!");
    browseTo("/opt/ports/dsda-doom");

    EXPECT_EQ(name->text(), "woof!");
}

TEST_F(EntryDialogTest, AnEntryKeepsItsNameOnAnotherPick) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Mine", .file = "/ports/woof"}));

    browseTo("/opt/ports/dsda-doom");

    EXPECT_EQ(name->text(), "Mine");
}

TEST_F(EntryDialogTest, PickingTheDialectTheNameSaysStoresNothing) {
    ASSERT_NO_FATAL_FAILURE(showPort({.name = "Mine", .file = "/ports/woof", .dialect = "dsda"}));

    canvas.ui().focus(dialect);

    while (dialect->current() < listed("woof", false)) {
        key(ttk::Code::Down);
    }

    EXPECT_EQ(dialect->current(), listed("woof", false));
    EXPECT_EQ(saved().dialect, "");
}

TEST_F(EntryDialogTest, ADownloadedPortDropsAStrayDialect) {
    ASSERT_NO_FATAL_FAILURE(
            showPort({.name = "Woof!", .file = "/data/ports/woof/woof", .portId = "woof", .dialect = "dsda"}));

    EXPECT_EQ(dialect->current(), listed("woof", false));
    EXPECT_EQ(saved().dialect, "");
}

#ifndef _WIN32
TEST_F(EntryDialogTest, ATypedPathIsTrimmedBeforeItIsRead) {
    const fs::path folder = tests::Sandbox::scratch("entry-trim");
    std::ofstream(folder / "mbf.exe") << "x";

    ASSERT_NO_FATAL_FAILURE(showPort({}));

    canvas.ui().focus(file->input());
    canvas.ui().wrote((folder / "mbf.exe").string() + " ");

    EXPECT_TRUE(dos->checked);
    EXPECT_EQ(dialect->current(), listed("boom", true));
}

TEST_F(EntryDialogTest, ATypedPathFillsAgainPastAShorterFile) {
    const fs::path folder = tests::Sandbox::scratch("entry-prefix");
    std::ofstream(folder / "doom") << "x";
    std::ofstream(folder / "doom.exe") << "x";

    ASSERT_NO_FATAL_FAILURE(showPort({}));

    canvas.ui().focus(file->input());

    for (const char each : (folder / "doom.exe").string()) {
        canvas.ui().wrote(std::string(1, each));
    }

    EXPECT_TRUE(dos->checked);
}
#endif
