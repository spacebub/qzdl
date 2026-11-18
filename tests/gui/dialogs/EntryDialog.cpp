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
#include <tuple>
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
#include "core/launch/Dialect.h"
#include "core/launch/KnownPorts.h"
#include "core/util/FileVersion.h"
#include "core/wad/FileInfo.h"
#include "gui/dialogs/EntryDialog.h"
#include "gui/services/Filters.h"
#include "support/Corpus.h"
#include "support/FlatpakApp.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

using dialogs::EntryDialog;

constexpr std::string_view HINT = "Until a version is picked, ZDL4 treats this port as the latest release.";
constexpr std::string_view INSTALLED = " · Flatpak";

struct Answer {
    std::string name;
    std::string file;
    bool dos = false;
    Port::Port port = Port::Port::unknown;
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
    ttk::Select *port = nullptr;
    ttk::Select *version = nullptr;
    ttk::Label *versionHint = nullptr;
    ttk::Button *cancel = nullptr;
    ttk::Button *save = nullptr;

    void show(const std::string &named, const std::string &path) {
        mount(std::make_unique<EntryDialog>(
                "Edit Doom II", GameEntry{.name = named, .file = path}, files, [this](const GameEntry &game) {
                    answers.push_back({.name = game.name,
                                       .file = game.file,
                                       .dos = false,
                                       .port = Port::Port::unknown,
                                       .version = ""});
                }));
    }

    void showEngine(Engine engine, const bool adding = false, const Dialect::VersionReader read = FileVersion::of) {
        mount(std::make_unique<EntryDialog>(
                "Edit Doom II",
                std::move(engine),
                adding,
                files,
                [this](const Engine &edited) {
                    answers.push_back({
                            .name = edited.name,
                            .file = edited.file,
                            .dos = edited.dosbox,
                            .port = edited.port,
                            .version = edited.version,
                    });
                },
                read));
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
        port = selects[0];
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

    void pickNextPort() {
        canvas.ui().focus(port);
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

    [[nodiscard]] ttk::GlyphButton *browse() const { return modal::hinted<ttk::GlyphButton>(dialog, "Browse"); }

    [[nodiscard]] ttk::Label *installLine() const {
        for (ttk::Label *each : modal::all<ttk::Label>(dialog)) {
            if (each->text().ends_with(INSTALLED)) {
                return each;
            }
        }

        return nullptr;
    }
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

TEST_F(EntryDialogTest, AGameHasNoDosSwitchOrPortPick) {
    ASSERT_NO_FATAL_FAILURE(show("Doom II", "/games/doom2.wad"));

    EXPECT_FALSE(dos->visible());
    EXPECT_FALSE(port->visible());
}

TEST_F(EntryDialogTest, TheDosSwitchShowsWhatTheEntryIs) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Doom", .file = "/ports/doom.exe", .dosbox = true}));

    EXPECT_TRUE(dos->visible());
    EXPECT_TRUE(dos->checked);
}

TEST_F(EntryDialogTest, ADosEntryIsSavedAsOne) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Doom", .file = "/ports/doom.exe", .dosbox = true}));

    EXPECT_TRUE(saved().dos);
}

TEST_F(EntryDialogTest, FlippingTheDosSwitchIsSaved) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Doom", .file = "/ports/doom.exe"}));

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
    ASSERT_NO_FATAL_FAILURE(showEngine({}));

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
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = FileInfo::describePort("/opt/ports/woof"), .file = "/opt/ports/woof"}));

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

// The option that leaves the port to the file name.
constexpr int FROM_NAME = 0;

// Where the port select lists a port, with or without DOSBox.
int listed(const std::string_view id, const bool dos) {
    const std::span<const Port::Port> picks = Dialect::picks(dos);
    const auto found = std::ranges::find(picks, id, [](const Port::Port each) { return Port::known(each).id; });

    return found == picks.end() ? -1 : static_cast<int>(found - picks.begin()) + 1;
}


}

TEST_F(EntryDialogTest, AnEngineWithNoPickIsLeftToTheFileName) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom"}));

    EXPECT_TRUE(port->visible());
    EXPECT_EQ(port->current(), FROM_NAME);
    EXPECT_EQ(saved().port, Port::Port::unknown);
}

TEST_F(EntryDialogTest, AnUnknownNameIsLeftToTheZdoomGuess) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Mine", .file = "/ports/mystery-engine"}));

    EXPECT_EQ(port->current(), FROM_NAME);
    EXPECT_EQ(saved().port, Port::Port::unknown);
}

TEST_F(EntryDialogTest, PickingGzdoomForAnUnknownNameKeepsThePickAndListsVersions) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Mine", .file = "/ports/mystery-engine"}));

    EXPECT_FALSE(modal::seen(version));

    canvas.ui().focus(port);
    key(ttk::Code::Down);

    EXPECT_EQ(port->current(), listed("gzdoom", false));
    EXPECT_TRUE(modal::seen(version));

    const Answer kept = saved();

    EXPECT_EQ(kept.port, Port::Port::gzdoom);
    EXPECT_EQ(Dialect::versions({.name = kept.name, .file = kept.file, .port = kept.port}).size(), 6U);
}

TEST_F(EntryDialogTest, PickingGzdoomOverAStoredDsdaPickOnAnUnknownNameKeepsGzdoom) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Test", .file = "/ports/HARDDOOM.BAT", .port = Port::Port::dsda}));

    canvas.ui().focus(port);

    while (port->current() > listed("gzdoom", false)) {
        key(ttk::Code::Up);
    }

    EXPECT_EQ(port->current(), listed("gzdoom", false));
    EXPECT_TRUE(modal::seen(version));
    EXPECT_EQ(saved().port, Port::Port::gzdoom);
}

TEST_F(EntryDialogTest, AStoredPortIsShownAndKept) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Mine", .file = "/ports/engine", .port = Port::Port::woof}));

    EXPECT_EQ(port->current(), listed("woof", false));
    EXPECT_EQ(saved().port, Port::Port::woof);
}

TEST_F(EntryDialogTest, AStoredVersionIsKept) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Mine", .file = "/ports/dsda-doom", .version = "0.27.0"}));

    EXPECT_EQ(saved().version, "0.27.0");
}

namespace {

std::span<const Dialect::VersionRange> dsdaRanges() {
    return Dialect::versions({.name = "DSDA", .file = "/ports/dsda-doom"});
}

}

TEST_F(EntryDialogTest, APortWithReleasesOffersTheVersions) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom"}));

    EXPECT_TRUE(modal::seen(version));
    EXPECT_EQ(version->current(), -1);
}

TEST_F(EntryDialogTest, APortWithoutReleasesOffersNoVersion) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "LZ", .file = "/ports/lzdoom"}));

    EXPECT_FALSE(modal::seen(version));
}

TEST_F(EntryDialogTest, NoVersionIsOfferedBeforeAFileIsPicked) {
    ASSERT_NO_FATAL_FAILURE(showEngine({}, true));

    EXPECT_FALSE(modal::seen(version));
    EXPECT_FALSE(modal::seen(versionHint));
}

TEST_F(EntryDialogTest, ADosboxEngineIsNotAskedForAVersion) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom", .dosbox = true}, true));

    EXPECT_FALSE(modal::seen(version));
    EXPECT_TRUE(save->enabled());
}

TEST_F(EntryDialogTest, TheVersionListFollowsTheDosboxSwitch) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom", .version = "0.28.0"}));

    modal::click(canvas.ui(), dos->box().x + 12.0, modal::midY(dos->box()));

    EXPECT_FALSE(modal::seen(version));

    modal::click(canvas.ui(), dos->box().x + 12.0, modal::midY(dos->box()));

    EXPECT_TRUE(modal::seen(version));
    EXPECT_EQ(version->current(), -1);
}

TEST_F(EntryDialogTest, TheVersionListFollowsThePort) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Mine", .file = "/ports/lzdoom"}));

    EXPECT_FALSE(modal::seen(version));

    canvas.ui().focus(port);

    while (!modal::seen(version) && port->current() < static_cast<int>(Dialect::picks(false).size())) {
        key(ttk::Code::Down);
    }

    EXPECT_EQ(port->current(), listed("gzdoom", false));
    EXPECT_TRUE(modal::seen(version));
    EXPECT_EQ(saved().port, Port::Port::gzdoom);
}

TEST_F(EntryDialogTest, NuggetAndCherryAreListedWithVersionsOfTheirOwn) {
    for (const auto &[id, oldest] : {std::pair{"nugget-doom", "1.0.0"}, std::pair{"cherry-doom", "1.0.0"}}) {
        ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Mine", .file = "/ports/mystery-engine"}));
        ASSERT_GE(listed(id, false), 0) << id;

        canvas.ui().focus(port);

        while (port->current() < listed(id, false)) {
            key(ttk::Code::Down);
        }

        ASSERT_TRUE(modal::seen(version)) << id;

        canvas.ui().focus(version);

        for (int step = 0; step < 8; ++step) {
            key(ttk::Code::Down);
        }

        const Answer kept = saved();

        EXPECT_EQ(Port::known(kept.port).id, id);
        EXPECT_EQ(kept.version, oldest) << id;
    }
}

TEST_F(EntryDialogTest, VkdoomAndSkulltagAreListedWithVersionsOfTheirOwn) {
    for (const auto &[id, oldest] : {std::pair{"vkdoom", "0.9"}, std::pair{"skulltag", "0.97d"}}) {
        ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Mine", .file = "/ports/mystery-engine"}));
        ASSERT_GE(listed(id, false), 0) << id;

        canvas.ui().focus(port);

        while (port->current() < listed(id, false)) {
            key(ttk::Code::Down);
        }

        ASSERT_TRUE(modal::seen(version)) << id;

        canvas.ui().focus(version);

        for (int step = 0; step < 8; ++step) {
            key(ttk::Code::Down);
        }

        const Answer kept = saved();

        EXPECT_EQ(Port::known(kept.port).id, id);
        EXPECT_EQ(kept.version, oldest) << id;
    }
}

TEST_F(EntryDialogTest, NyanDoomPickedForAnUnknownNameIsStored) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Mine", .file = "/ports/mystery-engine"}));

    canvas.ui().focus(port);

    while (port->current() < listed("nyan-doom", false)) {
        key(ttk::Code::Down);
    }

    EXPECT_EQ(saved().port, Port::Port::nyan);
}

TEST_F(EntryDialogTest, TheVersionListFollowsTheFile) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Mine", .file = "/ports/lzdoom"}));

    browseTo("/opt/ports/dsda-doom");

    EXPECT_TRUE(modal::seen(version));

    browseTo("/opt/ports/lzdoom");

    EXPECT_FALSE(modal::seen(version));
}

TEST_F(EntryDialogTest, AStoredVersionIsShown) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom", .version = "0.27.0"}));

    const std::span<const Dialect::VersionRange> ranges = dsdaRanges();
    const auto at = std::ranges::find(ranges, std::string_view("0.27.0"), &Dialect::VersionRange::first);

    ASSERT_NE(at, ranges.end());
    EXPECT_EQ(version->current(), static_cast<int>(at - ranges.begin()));
}

TEST_F(EntryDialogTest, ThePickedVersionIsSavedAndTheNewestIsFirst) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom"}));

    canvas.ui().focus(version);
    key(ttk::Code::Down);

    const std::span<const Dialect::VersionRange> ranges = dsdaRanges();

    ASSERT_FALSE(ranges.empty());
    EXPECT_EQ(version->current(), 0);
    EXPECT_EQ(saved().version, std::string(ranges.front().first));
}

TEST_F(EntryDialogTest, AnotherVersionReplacesTheStoredOne) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom", .version = "0.28.0"}));

    canvas.ui().focus(version);
    key(ttk::Code::Up);

    EXPECT_EQ(saved().version, "0.30.0");
}

TEST_F(EntryDialogTest, TheHintShowsUntilAVersionIsPicked) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom"}));

    EXPECT_TRUE(modal::seen(versionHint));

    canvas.ui().focus(version);
    key(ttk::Code::Down);

    EXPECT_FALSE(modal::seen(versionHint));
}

TEST_F(EntryDialogTest, AddingAnEngineWithReleasesNeedsAVersionToSave) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom"}, true));

    EXPECT_FALSE(save->enabled());

    modal::click(canvas.ui(), save);

    EXPECT_TRUE(answers.empty());

    canvas.ui().focus(version);
    key(ttk::Code::Down);

    EXPECT_TRUE(save->enabled());
    EXPECT_FALSE(saved().version.empty());
}

TEST_F(EntryDialogTest, AddingAnEngineWithoutReleasesNeedsNoVersion) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "LZ", .file = "/ports/lzdoom"}, true));

    EXPECT_TRUE(save->enabled());
}

TEST_F(EntryDialogTest, APickedFileDecidesWhetherAddingNeedsAVersion) {
    ASSERT_NO_FATAL_FAILURE(showEngine({}, true));

    browseTo("/opt/ports/dsda-doom");

    EXPECT_FALSE(save->enabled());

    browseTo("/opt/ports/lzdoom");

    EXPECT_TRUE(save->enabled());
}

TEST_F(EntryDialogTest, ReturnWhileAddingWithoutAVersionMovesToTheVersion) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom"}, true));

    key(ttk::Code::Return);

    EXPECT_TRUE(answers.empty());
    EXPECT_TRUE(layer->covered());
    EXPECT_EQ(canvas.ui().focused(), version);
}

TEST_F(EntryDialogTest, EditingAnEngineWithNoVersionCanStillSave) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom"}, false));

    EXPECT_TRUE(save->enabled());
    EXPECT_TRUE(modal::seen(versionHint));

    canvas.ui().wrote("2");
    key(ttk::Code::Return);

    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front().version, "");
}

TEST_F(EntryDialogTest, AVersionTheFileNoLongerOffersIsCleared) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom", .version = "0.28.0"}));

    browseTo("/opt/ports/gzdoom");

    EXPECT_EQ(saved().version, "");
}

TEST_F(EntryDialogTest, AVersionIsClearedWhenAnotherPortIsPicked) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom", .version = "0.28.0"}));

    canvas.ui().focus(port);

    while (port->current() < listed("gzdoom", false)) {
        key(ttk::Code::Down);
    }

    EXPECT_EQ(version->current(), -1);
    EXPECT_TRUE(modal::seen(versionHint));
    EXPECT_EQ(saved().version, "");
}

TEST_F(EntryDialogTest, AStoredReleaseShowsTheRangeItFallsInAndIsKept) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom", .version = "0.29.4"}, true));

    const std::span<const Dialect::VersionRange> ranges = dsdaRanges();
    const auto at = std::ranges::find(ranges, std::string_view("0.28.0"), &Dialect::VersionRange::first);

    ASSERT_NE(at, ranges.end());
    EXPECT_EQ(version->current(), static_cast<int>(at - ranges.begin()));
    EXPECT_FALSE(modal::seen(versionHint));
    EXPECT_TRUE(save->enabled());
    EXPECT_EQ(saved().version, "0.29.4");
}

TEST_F(EntryDialogTest, AVersionWithoutAReleaseIsClearedAndAsked) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom", .version = "garbage"}, true));

    EXPECT_EQ(version->current(), -1);
    EXPECT_TRUE(modal::seen(versionHint));
    EXPECT_FALSE(save->enabled());
}

#ifndef _WIN32
TEST_F(EntryDialogTest, NoVersionIsSuggestedForAPickedFileOffWindows) {
    ASSERT_NO_FATAL_FAILURE(showEngine({}, true));

    browseTo("/opt/ports/gzdoom");

    EXPECT_EQ(version->current(), -1);
    EXPECT_FALSE(save->enabled());
}
#endif

TEST_F(EntryDialogTest, APickedFileIsLeftToItsName) {
    ASSERT_NO_FATAL_FAILURE(showEngine({}));

    browseTo("/opt/ports/nugget-doom");

    EXPECT_EQ(port->current(), FROM_NAME);
    EXPECT_EQ(saved().port, Port::Port::unknown);
}

TEST_F(EntryDialogTest, ATypedFileIsLeftToItsName) {
    ASSERT_NO_FATAL_FAILURE(showEngine({}));

    canvas.ui().focus(file->input());
    canvas.ui().wrote("/opt/ports/helion");

    EXPECT_EQ(port->current(), FROM_NAME);
    EXPECT_EQ(saved().port, Port::Port::unknown);
}

TEST_F(EntryDialogTest, ANewFileIsLeftToItsNameWhenNoneWasPicked) {
    ASSERT_NO_FATAL_FAILURE(showEngine({}));

    browseTo("/opt/ports/woof");
    browseTo("/opt/ports/dsda-doom");

    EXPECT_EQ(port->current(), FROM_NAME);
    EXPECT_EQ(saved().port, Port::Port::unknown);
}

TEST_F(EntryDialogTest, APortPickedByHandSurvivesANewFile) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Mine", .file = "/ports/gzdoom"}));

    pickNextPort();
    browseTo("/opt/ports/woof");

    EXPECT_EQ(port->current(), listed("gzdoom", false));
    EXPECT_EQ(saved().port, Port::Port::gzdoom);
}

TEST_F(EntryDialogTest, AStoredPortSurvivesANewFile) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Mine", .file = "/ports/engine", .port = Port::Port::dsda}));

    browseTo("/opt/ports/woof");

    EXPECT_EQ(saved().port, Port::Port::dsda);
}

TEST_F(EntryDialogTest, AnUnknownDosFileIsLeftToVanilla) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Doom", .file = "/ports/game.exe", .dosbox = true}));

    EXPECT_EQ(port->current(), FROM_NAME);
}

TEST_F(EntryDialogTest, OnlyDosPortsAreOfferedUnderDosbox) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Doom", .file = "/ports/game.exe", .dosbox = true}));

    canvas.ui().focus(port);
    key(ttk::Code::Down);

    const Answer answer = saved();

    ASSERT_NE(answer.port, Port::Port::unknown);
    EXPECT_GT(listed(Port::known(answer.port).id, true), FROM_NAME);
}

TEST_F(EntryDialogTest, TurningOnDosboxDropsANativePort) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Mine", .file = "/ports/engine", .port = Port::Port::woof}));

    modal::click(canvas.ui(), dos->box().x + 12.0, modal::midY(dos->box()));

    EXPECT_EQ(port->current(), FROM_NAME);
    EXPECT_EQ(saved().port, Port::Port::unknown);
}

TEST_F(EntryDialogTest, ANewEngineShowsOnlyTheFile) {
    ASSERT_NO_FATAL_FAILURE(showEngine({}));

    EXPECT_TRUE(file->visible());
    EXPECT_TRUE(cancel->visible());
    EXPECT_FALSE(name->visible());
    EXPECT_FALSE(port->visible());
    EXPECT_FALSE(dos->visible());
    EXPECT_FALSE(save->visible());
    EXPECT_EQ(canvas.ui().focused(), file->input());
}

TEST_F(EntryDialogTest, APickedFileShowsTheRest) {
    ASSERT_NO_FATAL_FAILURE(showEngine({}));

    browseTo("/opt/ports/woof");

    EXPECT_TRUE(name->visible());
    EXPECT_TRUE(port->visible());
    EXPECT_TRUE(dos->visible());
    EXPECT_TRUE(save->visible());
    EXPECT_EQ(name->text(), "Woof!");
}

TEST_F(EntryDialogTest, ATypedPathShowsTheRestOnceItIsAFile) {
    ASSERT_NO_FATAL_FAILURE(showEngine({}));

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
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Gone", .file = "/no/such/port"}));

    EXPECT_TRUE(name->visible());
    EXPECT_TRUE(port->visible());
    EXPECT_TRUE(dos->visible());
    EXPECT_TRUE(save->visible());
}

#ifndef _WIN32
TEST_F(EntryDialogTest, APickedDosPortRunsUnderDosbox) {
    ASSERT_NO_FATAL_FAILURE(showEngine({}));

    browseTo("/opt/ports/mbf.exe");

    EXPECT_TRUE(dos->checked);
    EXPECT_EQ(port->current(), FROM_NAME);

    const Answer answer = saved();

    EXPECT_TRUE(answer.dos);
    EXPECT_EQ(answer.port, Port::Port::unknown);
}

TEST_F(EntryDialogTest, APickedNativePortTurnsDosboxOff) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Old", .file = "/opt/ports/doom.exe", .dosbox = true}));

    browseTo("/opt/ports/woof");

    EXPECT_FALSE(dos->checked);
    EXPECT_EQ(port->current(), FROM_NAME);
}

TEST_F(EntryDialogTest, ADosboxSwitchFlippedByHandSurvivesANewFile) {
    ASSERT_NO_FATAL_FAILURE(showEngine({}));

    browseTo("/opt/ports/doom.exe");
    modal::click(canvas.ui(), dos->box().x + 12.0, modal::midY(dos->box()));

    ASSERT_FALSE(dos->checked);

    browseTo("/opt/ports/doom2.exe");

    EXPECT_FALSE(dos->checked);
}
#endif

TEST_F(EntryDialogTest, ANativeDosPortIsLeftToItsName) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Legacy", .file = "/usr/bin/doomlegacy"}));

    EXPECT_EQ(port->current(), FROM_NAME);
    EXPECT_EQ(Dialect::of(Engine{.name = "Legacy", .file = "/usr/bin/doomlegacy"}).dialect, Dialect::Dialect::legacy);
}

TEST_F(EntryDialogTest, AnotherPickRenamesAnUntypedName) {
    ASSERT_NO_FATAL_FAILURE(showEngine({}));

    browseTo("/opt/ports/woof");
    browseTo("/opt/ports/dsda-doom");

    EXPECT_EQ(name->text(), "DSDA-Doom");
}

TEST_F(EntryDialogTest, ATypedNameSurvivesAnotherPick) {
    ASSERT_NO_FATAL_FAILURE(showEngine({}));

    browseTo("/opt/ports/woof");
    canvas.ui().focus(name->input());
    key(ttk::Code::End);
    canvas.ui().wrote("!");
    browseTo("/opt/ports/dsda-doom");

    EXPECT_EQ(name->text(), "Woof!!");
}

TEST_F(EntryDialogTest, AnEntryKeepsItsNameOnAnotherPick) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Mine", .file = "/ports/woof"}));

    browseTo("/opt/ports/dsda-doom");

    EXPECT_EQ(name->text(), "Mine");
}

TEST_F(EntryDialogTest, PickingThePortTheNameGivesStoresIt) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Mine", .file = "/ports/woof", .port = Port::Port::dsda}));

    canvas.ui().focus(port);

    while (port->current() < listed("woof", false)) {
        key(ttk::Code::Down);
    }

    EXPECT_EQ(port->current(), listed("woof", false));
    EXPECT_EQ(saved().port, Port::Port::woof);
}

TEST_F(EntryDialogTest, APickIsStoredForAForkOrAnUnknownName) {
    for (const char *path : {"/ports/lzdoom", "/ports/mystery-engine"}) {
        ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Mine", .file = path}));

        canvas.ui().focus(port);
        key(ttk::Code::Down);

        const Answer kept = saved();

        EXPECT_EQ(kept.port, Port::Port::gzdoom) << path;
        EXPECT_EQ(Dialect::versions({.name = kept.name, .file = kept.file, .port = kept.port}).size(), 6U) << path;
    }
}

TEST_F(EntryDialogTest, FromTheFileNameTakesAVariantBackToItsOwnPort) {
    for (const auto &[path, picked, own] :
         {std::tuple{"/ports/crispy-heretic", Port::Port::chocolate, Port::Port::crispyOther},
          std::tuple{"/ports/glboom-plus", Port::Port::prboomPlus, Port::Port::glboomPlus}}) {
        ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Mine", .file = path, .port = picked}));
        ASSERT_EQ(port->current(), listed(Port::known(picked).id, false)) << path;

        canvas.ui().focus(port);

        while (port->current() > FROM_NAME) {
            key(ttk::Code::Up);
        }

        const Answer kept = saved();

        EXPECT_EQ(kept.port, Port::Port::unknown) << path;
        EXPECT_EQ(Dialect::of(Engine{.name = kept.name, .file = kept.file}).port, own) << path;
    }
}

#ifndef _WIN32
TEST_F(EntryDialogTest, ATypedPathIsTrimmedBeforeItIsRead) {
    const fs::path folder = tests::Sandbox::scratch("entry-trim");
    std::ofstream(folder / "mbf.exe") << "x";

    ASSERT_NO_FATAL_FAILURE(showEngine({}));

    canvas.ui().focus(file->input());
    canvas.ui().wrote((folder / "mbf.exe").string() + " ");

    EXPECT_TRUE(dos->checked);
    EXPECT_EQ(port->current(), FROM_NAME);
}

TEST_F(EntryDialogTest, ATypedPathFillsAgainPastAShorterFile) {
    const fs::path folder = tests::Sandbox::scratch("entry-prefix");
    std::ofstream(folder / "doom") << "x";
    std::ofstream(folder / "doom.exe") << "x";

    ASSERT_NO_FATAL_FAILURE(showEngine({}));

    canvas.ui().focus(file->input());

    for (const char each : (folder / "doom.exe").string()) {
        canvas.ui().wrote(std::string(1, each));
    }

    EXPECT_TRUE(dos->checked);
}
#endif

namespace {

// The version text three test programs carry. Any other file has none.
std::string versionText(const std::filesystem::path &file) {
    const std::string path = file.generic_string();

    if (path == "/opt/ports/gzdoom" || path == "/opt/ports/mystery-engine") {
        return "g4.10.0-24-gfe3c1d0";
    }

    return path == "/opt/ports/dsda-doom" ? "0.27.3" : "";
}

int shownRange(const std::span<const Dialect::VersionRange> ranges, const std::string_view first) {
    const auto at = std::ranges::find(ranges, first, &Dialect::VersionRange::first);

    return at == ranges.end() ? -2 : static_cast<int>(at - ranges.begin());
}

}

TEST_F(EntryDialogTest, AnEngineOpenedWithAFileTakesTheReleaseTheFileNames) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "GZ", .file = "/opt/ports/gzdoom"}, true, versionText));

    EXPECT_EQ(version->current(), shownRange(Dialect::versions({.name = "GZ", .file = "/opt/ports/gzdoom"}), "4.9.0"));
    EXPECT_FALSE(modal::seen(versionHint));
    EXPECT_TRUE(save->enabled());
    EXPECT_EQ(saved().version, "4.10.0");
}

TEST_F(EntryDialogTest, ABrowsedFileTakesTheReleaseItNames) {
    ASSERT_NO_FATAL_FAILURE(showEngine({}, true, versionText));

    browseTo("/opt/ports/dsda-doom");

    EXPECT_EQ(version->current(), shownRange(dsdaRanges(), "0.27.0"));
    EXPECT_EQ(saved().version, "0.27.3");
}

TEST_F(EntryDialogTest, ASuggestedReleaseGoesWithTheFile) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/opt/ports/dsda-doom"}, true, versionText));

    browseTo("/opt/other/dsda-doom");

    EXPECT_EQ(version->current(), -1);
    EXPECT_TRUE(modal::seen(versionHint));
    EXPECT_FALSE(save->enabled());
}

TEST_F(EntryDialogTest, AStoredVersionGoesWithTheFileUnlessPicked) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom", .version = "0.28.0"}));

    browseTo("/opt/other/dsda-doom");

    EXPECT_EQ(saved().version, "");
}

TEST_F(EntryDialogTest, APickedVersionStaysWhenTheFileChanges) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom"}, false, versionText));

    canvas.ui().focus(version);
    key(ttk::Code::Down);
    browseTo("/opt/ports/dsda-doom");

    EXPECT_EQ(saved().version, std::string(dsdaRanges().front().first));
}

TEST_F(EntryDialogTest, ADosboxEngineTakesNoRelease) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "GZ", .file = "/opt/ports/gzdoom", .dosbox = true}, true, versionText));

    EXPECT_EQ(saved().version, "");
}

TEST_F(EntryDialogTest, PickingAPortWithReleasesTakesTheReleaseTheFileNames) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "Mine", .file = "/opt/ports/mystery-engine"}, true, versionText));

    EXPECT_FALSE(modal::seen(version));

    canvas.ui().focus(port);

    while (port->current() < listed("gzdoom", false)) {
        key(ttk::Code::Down);
    }

    const Answer kept = saved();

    EXPECT_EQ(kept.port, Port::Port::gzdoom);
    EXPECT_EQ(kept.version, "4.10.0");
}

TEST_F(EntryDialogTest, TurningDosboxOffAgainTakesTheReleaseBack) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "GZ", .file = "/opt/ports/gzdoom"}, true, versionText));

    modal::click(canvas.ui(), dos->box().x + 12.0, modal::midY(dos->box()));

    EXPECT_FALSE(modal::seen(version));

    modal::click(canvas.ui(), dos->box().x + 12.0, modal::midY(dos->box()));

    EXPECT_EQ(saved().version, "4.10.0");
}

TEST_F(EntryDialogTest, BrowsingTheSameFileAgainKeepsTheStoredVersion) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/opt/other/dsda-doom", .version = "0.28.0"}));

    browseTo("/opt/other/dsda-doom");

    EXPECT_EQ(saved().version, "0.28.0");
}

TEST_F(EntryDialogTest, TypingTheSameFileAgainKeepsTheStoredVersion) {
    const std::string path = (tests::Sandbox::scratch("entry-same") / "dsda-doom").generic_string();

    std::ofstream(path) << "x";

    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = path, .version = "0.28.0"}));

    file->take_focus();
    canvas.ui().wrote(path);

    EXPECT_EQ(file->text(), path);
    EXPECT_EQ(saved().version, "0.28.0");
}

TEST_F(EntryDialogTest, TypingAnotherFileClearsTheStoredVersion) {
    const std::string path = (tests::Sandbox::scratch("entry-other") / "dsda-doom").generic_string();

    std::ofstream(path) << "x";

    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/opt/other/dsda-doom", .version = "0.28.0"}));

    file->take_focus();
    canvas.ui().wrote(path);

    EXPECT_EQ(file->text(), path);
    EXPECT_EQ(saved().version, "");
}

#ifdef __linux__
TEST_F(EntryDialogTest, AFlatpakShowsTheReleaseItsInstallNamesInPlaceOfTheSelect) {
    const tests::FlatpakApp deployed =
            tests::deployFlatpak(tests::Sandbox::scratch("entry-flatpak") / "flatpak", "org.zdoom.UZDoom");

    tests::nameRelease(deployed, "4.14.3");

    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "UZDoom", .file = deployed.exported.string()}, true));

    ASSERT_NE(installLine(), nullptr);
    EXPECT_TRUE(modal::seen(installLine()));
    EXPECT_EQ(installLine()->text(), "4.14.3 · Flatpak");
    EXPECT_FALSE(modal::seen(version));
    EXPECT_FALSE(modal::seen(versionHint));
    EXPECT_TRUE(save->enabled());
    EXPECT_EQ(saved().version, "4.14.3");
}

TEST_F(EntryDialogTest, TheInstalledReleaseReplacesAStoredVersion) {
    const tests::FlatpakApp deployed =
            tests::deployFlatpak(tests::Sandbox::scratch("entry-flatpak-stored") / "flatpak", "org.zdoom.UZDoom");

    tests::nameRelease(deployed, "5.0.3");

    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "UZDoom", .file = deployed.exported.string(), .version = "4.14.3"}));

    EXPECT_EQ(installLine()->text(), "5.0.3 · Flatpak");
    EXPECT_EQ(saved().version, "5.0.3");
}

TEST_F(EntryDialogTest, AnotherEngineShowsTheSelectAndNoInstallLine) {
    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom"}));

    EXPECT_TRUE(modal::seen(version));
    EXPECT_FALSE(modal::seen(installLine()));
}

TEST_F(EntryDialogTest, PickingAFlatpakFileSwitchesToTheInstallLineAndBack) {
    const tests::FlatpakApp deployed =
            tests::deployFlatpak(tests::Sandbox::scratch("entry-flatpak-switch") / "flatpak", "org.zdoom.UZDoom");

    tests::nameRelease(deployed, "4.14.3");

    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "DSDA", .file = "/ports/dsda-doom"}));

    browseTo(deployed.exported.string());

    EXPECT_TRUE(modal::seen(installLine()));
    EXPECT_FALSE(modal::seen(version));

    browseTo("/ports/dsda-doom");

    EXPECT_FALSE(modal::seen(installLine()));
    EXPECT_TRUE(modal::seen(version));
    EXPECT_EQ(saved().version, "");
}

TEST_F(EntryDialogTest, RunningAFlatpakUnderDosboxShowsNoInstallLine) {
    const tests::FlatpakApp deployed =
            tests::deployFlatpak(tests::Sandbox::scratch("entry-flatpak-dos") / "flatpak", "org.zdoom.UZDoom");

    tests::nameRelease(deployed, "4.14.3");

    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "UZDoom", .file = deployed.exported.string(), .dosbox = true}));

    EXPECT_FALSE(modal::seen(installLine()));
    EXPECT_EQ(saved().version, "");
}

TEST_F(EntryDialogTest, AFlatpakWithoutAReleaseAsksForAVersion) {
    const tests::FlatpakApp deployed =
            tests::deployFlatpak(tests::Sandbox::scratch("entry-flatpak-nightly") / "flatpak", "org.zdoom.UZDoom");

    tests::nameRelease(deployed, "nightly");

    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "UZDoom", .file = deployed.exported.string()}, true));

    EXPECT_FALSE(modal::seen(installLine()));
    EXPECT_TRUE(modal::seen(version));
    EXPECT_TRUE(modal::seen(versionHint));
    EXPECT_FALSE(save->enabled());

    canvas.ui().focus(version);
    key(ttk::Code::Down);

    EXPECT_TRUE(save->enabled());
    EXPECT_FALSE(modal::seen(versionHint));

    const Answer kept = saved();

    EXPECT_FALSE(kept.version.empty());
    EXPECT_NE(kept.version, "nightly");
}

TEST_F(EntryDialogTest, TheInstallIsReadOncePerFile) {
    const fs::path root = tests::Sandbox::scratch("entry-flatpak-once");
    const tests::FlatpakApp first = tests::deployFlatpak(root / "first" / "flatpak", "org.zdoom.UZDoom");
    const tests::FlatpakApp second = tests::deployFlatpak(root / "second" / "flatpak", "org.zdoom.UZDoom");

    tests::nameRelease(first, "4.14.3");

    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "UZDoom", .file = first.exported.string()}));

    tests::nameRelease(first, "5.0.3");

    // The trailing space changes the text but trims to the same file.
    file->take_focus();
    canvas.ui().wrote(first.exported.string() + " ");

    EXPECT_EQ(file->text(), first.exported.string() + " ");
    EXPECT_EQ(installLine()->text(), "4.14.3 · Flatpak");

    tests::nameRelease(second, "5.1.0");

    file->take_focus();
    canvas.ui().wrote(second.exported.string());

    EXPECT_EQ(installLine()->text(), "5.1.0 · Flatpak");
    EXPECT_EQ(saved().version, "5.1.0");
}

TEST_F(EntryDialogTest, ReturnWithTheInstallLineSavesWithoutMovingToTheSelect) {
    const tests::FlatpakApp deployed =
            tests::deployFlatpak(tests::Sandbox::scratch("entry-flatpak-return") / "flatpak", "org.zdoom.UZDoom");

    tests::nameRelease(deployed, "4.14.3");

    ASSERT_NO_FATAL_FAILURE(showEngine({.name = "UZDoom", .file = deployed.exported.string()}, true));

    ASSERT_TRUE(modal::seen(installLine()));

    key(ttk::Code::Return);

    EXPECT_NE(canvas.ui().focused(), version);
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front().version, "4.14.3");
}
#endif
