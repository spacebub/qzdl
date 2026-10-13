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
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/notices/Notifier.h"
#include "ttk/system/Paths.h"
#include "ttk/util/Format.h"

#include "Spawn.h"
#include "core/config/Session.h"
#include "core/ports/Catalog.h"
#include "core/ports/Install.h"
#include "core/ports/Releases.h"
#include "core/wad/Bytes.h"
#include "gui/services/Engines.h"
#include "gui/state/State.h"
#include "support/Sandbox.h"
#include "support/Shell.h"

namespace {

namespace fs = std::filesystem;
namespace Bytes = tests::Bytes;
namespace Spawn = tests::Spawn;

using ttk::Format::from_path;

// How often the service sweeps its fetches and unpacking.
constexpr double TICK = 0.08;

// Detection walks the machine's shelves on a thread of its own.
constexpr double PATIENCE = 10.0;

// One fetched from GitHub, one a fixed DOS zip, and one put on PATH for detection to find.
constexpr std::string_view RELEASED = "dsda-doom";
constexpr std::string_view FIXED = "boom";
constexpr std::string_view FOUND = "woof";

int indexOf(const std::string_view id) {
    const auto ports = Catalog::ports();

    for (size_t at = 0; at < ports.size(); ++at) {
        if (ports[at].id == id) {
            return static_cast<int>(at);
        }
    }

    return -1;
}

const Catalog::Port &portOf(const std::string_view id) {
    return Catalog::ports()[static_cast<size_t>(indexOf(id))];
}

const State::EngineRow &rowOf(const std::string_view id) {
    return State::get().ports.rows.at(static_cast<size_t>(indexOf(id)));
}

std::vector<NameEntry> &listed() {
    return Session::get().config().ports;
}

fs::path onPath() {
    return tests::Sandbox::root() / "bin" / std::string(portOf(FOUND).program);
}

// Detection is looked up once a process, so the port is on PATH before the first case builds the service.
void putOnPath() {
    if (!fs::exists(onPath())) {
        Bytes::write(onPath(), "#!/bin/sh\n");
        fs::permissions(onPath(), fs::perms::owner_all);
    }
}

struct Added {
    std::string file;
    std::string name;
    bool dos;
    std::string portId;
};

struct Updated {
    int at;
    std::string name;
    std::string file;
    bool dos;
};

class EnginesTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_GE(indexOf(RELEASED), 0);
        ASSERT_GE(indexOf(FIXED), 0);
        ASSERT_GE(indexOf(FOUND), 0);

        State::get() = State::All{};
        Session::get().config() = Config{};

        std::error_code code;

        fs::remove_all(Catalog::directory(), code);
        fs::remove_all(Catalog::downloads(), code);
        fs::remove(ttk::Paths::data_directory() / ".releases", code);

        putOnPath();
    }

    void TearDown() override { engines.reset(); }

    // Wired as the window wires it: what is added lands in the config's list.
    Engines &make() {
        engines = std::make_unique<Engines>(&shell, &notifier);
        engines->setOffline(true);

        engines->addPort =
                [this](const std::string &file, const std::string &name, const bool dos, const std::string &id) {
                    added.push_back({.file = file, .name = name, .dos = dos, .portId = id});
                    listed().push_back({.name = name, .file = file, .dosbox = dos, .portId = id});

                    return name;
                };

        engines->updatePort = [this](const int at, const std::string &name, const std::string &file, const bool dos) {
            updated.push_back({.at = at, .name = name, .file = file, .dos = dos});
            listed().at(static_cast<size_t>(at)).file = file;
        };

        engines->removePort = [this](const int at) {
            removed.push_back(at);
            listed().erase(listed().begin() + at);
        };

        engines->scheduleSave = [this] { ++saves; };

        return *engines;
    }

    void primed() { ASSERT_TRUE(shell.await(PATIENCE)); }

    // As the service leaves a port it fetched: the program, and the version stamped beside it.
    static fs::path unpack(const std::string_view id, const std::string &version = "1.2.3") {
        const Catalog::Port &known = portOf(id);
        const fs::path where = Catalog::directory(known);
        const fs::path program = where / (std::string(known.program) + (known.dos ? ".exe" : ""));

        Bytes::write(program, "#!/bin/sh\n");

        if (!version.empty()) {
            Bytes::write(where / ".zdl-version", version);
        }

        return program;
    }

    static fs::path download(const std::string &bytes) {
        const Catalog::Port &known = portOf(FIXED);
        const fs::path at = Catalog::downloads() / Install::downloadName(known, Install::fileNameOf(known.file));

        Bytes::write(at, bytes);

        return at;
    }

    [[nodiscard]] std::vector<Added> addedFor(const std::string_view id) const {
        std::vector<Added> out;

        std::ranges::copy_if(added, std::back_inserter(out), [id](const Added &each) { return each.portId == id; });

        return out;
    }

    [[nodiscard]] bool addedFile(const fs::path &file) const {
        return std::ranges::any_of(added, [&file](const Added &each) { return each.file == from_path(file); });
    }

    [[nodiscard]] bool noticed(const std::string &body) const {
        return std::ranges::any_of(notifier.messages(), [&body](const ttk::Notice &each) { return each.body == body; });
    }

    bool installs() {
        engines->install(indexOf(FIXED));

        return Spawn::until(shell, TICK, [] { return rowOf(FIXED).status != State::EngineState::Unpacking; });
    }

    tests::Shell shell;
    ttk::Notifier notifier;
    std::unique_ptr<Engines> engines;

    std::vector<Added> added;
    std::vector<Updated> updated;
    std::vector<int> removed;
    int saves = 0;
};

TEST_F(EnginesTest, EveryCataloguedPortHasARow) {
    make();

    const std::vector<State::EngineRow> &rows = State::get().ports.rows;

    ASSERT_EQ(rows.size(), Catalog::ports().size());

    for (size_t at = 0; at < rows.size(); ++at) {
        EXPECT_EQ(rows[at].index, static_cast<int>(at));
        EXPECT_EQ(rows[at].name, Catalog::ports()[at].name);
        EXPECT_EQ(rows[at].dos, Catalog::ports()[at].dos);
    }
}

TEST_F(EnginesTest, EachRowStartsFromWhatTheCatalogueSays) {
    make();

    for (const Catalog::Port &known : Catalog::ports()) {
        const State::EngineRow &row = rowOf(known.id);

        if (known.repository.empty() && known.file.empty()) {
            EXPECT_EQ(row.status, State::EngineState::Elsewhere) << known.id;
        } else if (Catalog::pattern(known).empty()) {
            EXPECT_EQ(row.status, State::EngineState::Unavailable) << known.id;
            EXPECT_EQ(row.error, "There is no build of this one for this system") << known.id;
        } else if (!known.file.empty()) {
            EXPECT_EQ(row.status, State::EngineState::Ready) << known.id;
            EXPECT_EQ(row.version, known.version) << known.id;
        } else {
            EXPECT_EQ(row.status, State::EngineState::Waiting) << known.id;
            EXPECT_EQ(row.error, "") << known.id;
        }
    }
}

TEST_F(EnginesTest, TheStateNamesWherePortsGo) {
    make();

    EXPECT_EQ(State::get().ports.directory, from_path(Catalog::directory()));
    EXPECT_EQ(State::get().ports.downloads, from_path(Catalog::downloads()));
    EXPECT_FALSE(State::get().ports.checking);
}

TEST_F(EnginesTest, AnUnpackedPortIsInstalledWithItsStampedVersion) {
    const fs::path program = unpack(RELEASED, "0.29.3");

    make();

    EXPECT_EQ(rowOf(RELEASED).status, State::EngineState::Installed);
    EXPECT_EQ(rowOf(RELEASED).have, "0.29.3");
    EXPECT_EQ(rowOf(RELEASED).file, from_path(program));
}

TEST_F(EnginesTest, AnUnpackedPortWithoutAStampHasNoVersion) {
    unpack(RELEASED, "");

    make();

    EXPECT_EQ(rowOf(RELEASED).status, State::EngineState::Installed);
    EXPECT_EQ(rowOf(RELEASED).have, "");
}

TEST_F(EnginesTest, AnUnpackedPortMissingFromTheConfigIsReported) {
    unpack(RELEASED);

    make();

    EXPECT_TRUE(noticed("A source port ZDL4 had fetched was missing from this config."));
}

TEST_F(EnginesTest, SeveralMissingPortsAreReportedTogether) {
    unpack(RELEASED);
    unpack(FIXED);

    make();

    EXPECT_TRUE(noticed("2 source ports ZDL4 had fetched were missing from this config."));
}

TEST_F(EnginesTest, RelistPutsAnUnpackedPortBackInTheList) {
    const fs::path program = unpack(RELEASED);

    make().relist();

    const std::vector<Added> back = addedFor(RELEASED);

    ASSERT_EQ(back.size(), 1U);
    EXPECT_EQ(back.front().file, from_path(program));
    EXPECT_EQ(back.front().name, portOf(RELEASED).name);
    EXPECT_FALSE(back.front().dos);
}

TEST_F(EnginesTest, RelistAddsAPortOnce) {
    unpack(RELEASED);

    make().relist();
    engines->relist();

    EXPECT_EQ(addedFor(RELEASED).size(), 1U);
}

TEST_F(EnginesTest, APortWithARowIsNotRelisted) {
    unpack(RELEASED);

    listed().push_back({.name = "Mine", .file = "/somewhere/else", .dosbox = false, .portId = std::string(RELEASED)});

    make().relist();

    EXPECT_TRUE(addedFor(RELEASED).empty());
    EXPECT_FALSE(noticed("A source port ZDL4 had fetched was missing from this config."));
}

TEST_F(EnginesTest, ARowInsideAPortsDirectoryIsAdoptedAsThatPort) {
    const fs::path program = unpack(RELEASED);

    make();

    listed().push_back({.name = "Old", .file = from_path(program), .dosbox = false, .portId = ""});

    engines->relist();

    EXPECT_EQ(listed().front().portId, RELEASED);
    EXPECT_EQ(saves, 1);
    EXPECT_TRUE(addedFor(RELEASED).empty());
}

TEST_F(EnginesTest, DetectionAddsAPortFoundOnThePath) {
    make();
    primed();

    EXPECT_TRUE(addedFile(onPath()));
    EXPECT_TRUE(std::ranges::contains(Session::get().config().general.detected, std::string(FOUND)));
    EXPECT_GE(saves, 1);
}

TEST_F(EnginesTest, ADetectedPortIsAddedWithoutAnId) {
    make();
    primed();

    const auto found = std::ranges::find_if(added, [](const Added &each) { return each.file == from_path(onPath()); });

    ASSERT_NE(found, added.end());
    EXPECT_EQ(found->portId, "");
    EXPECT_EQ(found->name, portOf(FOUND).name);
}

TEST_F(EnginesTest, DiscoverOffersEachPortOnce) {
    make();
    primed();

    added.clear();
    saves = 0;

    engines->discover();

    EXPECT_TRUE(added.empty());
    EXPECT_EQ(saves, 0);
}

TEST_F(EnginesTest, AFoundPortAlreadyInTheListIsNotAddedAgain) {
    listed().push_back({.name = "Mine", .file = from_path(onPath()), .dosbox = false, .portId = ""});

    make();
    primed();

    EXPECT_FALSE(addedFile(onPath()));
    EXPECT_TRUE(std::ranges::contains(Session::get().config().general.detected, std::string(FOUND)));
}

TEST_F(EnginesTest, APortOfferedBeforeIsNotOfferedAgain) {
    Session::get().config().general.detected.emplace_back(FOUND);

    make();
    primed();

    EXPECT_FALSE(addedFile(onPath()));
}

TEST_F(EnginesTest, ForgetRemovesTheRowAndTheFilesOfItsPort) {
    const fs::path program = unpack(RELEASED);

    listed().push_back({.name = "Dsda", .file = from_path(program), .dosbox = false, .portId = std::string(RELEASED)});

    make().forget(0);

    EXPECT_EQ(removed, (std::vector<int>{0}));
    EXPECT_FALSE(fs::exists(Catalog::directory(portOf(RELEASED))));
    EXPECT_NE(rowOf(RELEASED).status, State::EngineState::Installed);
    EXPECT_EQ(rowOf(RELEASED).file, "");
    EXPECT_EQ(saves, 1);
}

TEST_F(EnginesTest, ForgetOfARowOutsideEveryPortOnlyRemovesTheRow) {
    const fs::path program = unpack(RELEASED);

    listed().push_back({.name = "Dsda", .file = from_path(program), .dosbox = false, .portId = std::string(RELEASED)});
    listed().push_back({.name = "Other", .file = "/usr/games/other", .dosbox = false, .portId = ""});

    make().forget(1);

    EXPECT_EQ(removed, (std::vector<int>{1}));
    EXPECT_TRUE(fs::exists(program));
    EXPECT_EQ(rowOf(RELEASED).status, State::EngineState::Installed);
}

TEST_F(EnginesTest, ForgetOutOfRangeDoesNothing) {
    make();

    engines->forget(-1);
    engines->forget(0);

    EXPECT_TRUE(removed.empty());
}

TEST_F(EnginesTest, RemoveTakesThePortsFilesAndEveryRowInsideIt) {
    const fs::path program = unpack(RELEASED);
    const std::string inside = from_path(program.parent_path() / "other-build");

    listed().push_back({.name = "Dsda", .file = from_path(program), .dosbox = false, .portId = std::string(RELEASED)});
    listed().push_back({.name = "Keep", .file = "/usr/games/keep", .dosbox = false, .portId = ""});
    listed().push_back({.name = "Older", .file = inside, .dosbox = false, .portId = ""});

    make().remove(indexOf(RELEASED));

    EXPECT_EQ(removed, (std::vector<int>{2, 0}));
    ASSERT_EQ(listed().size(), 1U);
    EXPECT_EQ(listed().front().name, "Keep");
    EXPECT_FALSE(fs::exists(program));
    EXPECT_EQ(rowOf(RELEASED).status, State::EngineState::Waiting);
}

TEST_F(EnginesTest, RemoveForgetsThePortIdOfARowItLeaves) {
    unpack(RELEASED);

    listed().push_back({.name = "Moved", .file = "/usr/games/moved", .dosbox = false, .portId = std::string(RELEASED)});

    make().remove(indexOf(RELEASED));

    ASSERT_EQ(listed().size(), 1U);
    EXPECT_EQ(listed().front().portId, "");
    EXPECT_GE(saves, 1);
}

TEST_F(EnginesTest, OutOfRangeRowsAreIgnored) {
    make();

    const std::vector<State::EngineRow> before = State::get().ports.rows;
    const int past = static_cast<int>(Catalog::ports().size());

    engines->install(-1);
    engines->install(past);
    engines->cancel(-1);
    engines->cancel(past);
    engines->remove(-1);
    engines->remove(past);

    EXPECT_EQ(State::get().ports.rows, before);
    EXPECT_EQ(shell.pending(), 0U);
}

TEST_F(EnginesTest, AnEmptyShelfIsNothing) {
    make();
    primed();

    EXPECT_FALSE(State::get().ports.cached);
    EXPECT_EQ(State::get().ports.cachedText, "Nothing");
}

TEST_F(EnginesTest, MeasureCountsTheDownloads) {
    make();
    primed();

    Bytes::write(Catalog::downloads() / "a.zip", std::string(2 * 1024 * 1024, 'a'));
    Bytes::write(Catalog::downloads() / "b.zip", std::string(1024 * 1024 / 2, 'b'));

    engines->measure();

    EXPECT_TRUE(State::get().ports.cached);
    EXPECT_EQ(State::get().ports.cachedText, "2.5 MB");
}

TEST_F(EnginesTest, ASmallShelfIsMeasuredInKilobytes) {
    make();
    primed();

    Bytes::write(Catalog::downloads() / "a.zip", std::string(3000, 'a'));

    engines->measure();

    EXPECT_EQ(State::get().ports.cachedText, "3 KB");
}

TEST_F(EnginesTest, AFewBytesAreAKilobyte) {
    make();
    primed();

    Bytes::write(Catalog::downloads() / "a.zip", "abc");

    engines->measure();

    EXPECT_EQ(State::get().ports.cachedText, "1 KB");
}

TEST_F(EnginesTest, TheShelfIsMeasuredAtStartUp) {
    Bytes::write(Catalog::downloads() / "a.zip", std::string(3000, 'a'));

    make();
    primed();

    EXPECT_TRUE(State::get().ports.cached);
    EXPECT_EQ(State::get().ports.cachedText, "3 KB");
}

TEST_F(EnginesTest, ClearDownloadsEmptiesTheShelf) {
    Bytes::write(Catalog::downloads() / "a.zip", std::string(3000, 'a'));
    Bytes::write(Catalog::downloads() / "nested" / "b.zip", "b");

    make();
    primed();

    engines->clearDownloads();

    EXPECT_TRUE(fs::is_empty(Catalog::downloads()));
    EXPECT_FALSE(State::get().ports.cached);
    EXPECT_EQ(State::get().ports.cachedText, "Nothing");
    EXPECT_EQ(notifier.messages().back().title, "The downloads are empty");
}

TEST_F(EnginesTest, ACachedReleaseFillsTheRow) {
    const Releases::Answer answer{
            .portId = std::string(RELEASED),
            .version = "0.29.3",
            .url = "https://example.invalid/dsda-doom-0.29.3-linux-x86_64.appimage",
            .asset = "dsda-doom-0.29.3-linux-x86_64.appimage",
            .verdict = static_cast<std::uint8_t>(State::EngineState::Waiting),
            .note = "",
            .checked = Releases::now(),
            .size = 3 * 1024 * 1024,
    };

    Releases::write({&answer, 1});

    make();

    EXPECT_EQ(rowOf(RELEASED).status, State::EngineState::Ready);
    EXPECT_EQ(rowOf(RELEASED).version, "0.29.3");
    EXPECT_EQ(rowOf(RELEASED).sizeText, "3.0 MB");
}

TEST_F(EnginesTest, ACachedVerdictWithoutABuildIsTheRowsState) {
    const Releases::Answer answer{
            .portId = std::string(RELEASED),
            .version = "0.29.3",
            .url = "",
            .asset = "",
            .verdict = static_cast<std::uint8_t>(State::EngineState::Unavailable),
            .note = "The latest release has no build for this system",
            .checked = Releases::now(),
            .size = 0,
    };

    Releases::write({&answer, 1});

    make();

    EXPECT_EQ(rowOf(RELEASED).status, State::EngineState::Unavailable);
    EXPECT_EQ(rowOf(RELEASED).error, "The latest release has no build for this system");
    EXPECT_EQ(rowOf(RELEASED).sizeText, "");
}

TEST_F(EnginesTest, ACachedVerdictOutOfRangeIsWaiting) {
    const Releases::Answer answer{
            .portId = std::string(RELEASED),
            .version = "",
            .url = "",
            .asset = "",
            .verdict = 200,
            .note = "",
            .checked = Releases::now(),
            .size = 0,
    };

    Releases::write({&answer, 1});

    make();

    EXPECT_EQ(rowOf(RELEASED).status, State::EngineState::Waiting);
}

TEST_F(EnginesTest, ACorruptReleaseCacheIsIgnoredAndRemoved) {
    const fs::path cache = ttk::Paths::data_directory() / ".releases";

    Bytes::write(cache, "not a release cache");

    make();

    EXPECT_EQ(rowOf(RELEASED).status, State::EngineState::Waiting);
    EXPECT_FALSE(fs::exists(cache));
}

TEST_F(EnginesTest, RefreshAsksNothingOffline) {
    make();

    engines->refresh(true);

    EXPECT_FALSE(State::get().ports.checking);
    EXPECT_EQ(rowOf(RELEASED).status, State::EngineState::Waiting);
    EXPECT_EQ(shell.pending(), 0U);
}

TEST_F(EnginesTest, InstallAsksNothingOfTheNetworkWhileOffline) {
    make();

    engines->install(indexOf(RELEASED));

    EXPECT_FALSE(rowOf(RELEASED).asking);
    EXPECT_EQ(shell.pending(), 0U);
}

TEST_F(EnginesTest, InstallUnpacksADownloadAlreadyOnTheShelf) {
    download(Bytes::zip({{.name = "boom.exe", .bytes = "MZ"}, {.name = "readme.txt", .bytes = "Boom"}}));

    make();
    primed();

    ASSERT_TRUE(installs());

    const fs::path program = Catalog::directory(portOf(FIXED)) / "boom.exe";

    EXPECT_EQ(rowOf(FIXED).status, State::EngineState::Installed);
    EXPECT_EQ(rowOf(FIXED).file, from_path(program));
    EXPECT_EQ(rowOf(FIXED).have, portOf(FIXED).version);
    EXPECT_EQ(Install::stampedVersion(portOf(FIXED)), portOf(FIXED).version);
    EXPECT_EQ(shell.pending(), 0U);
}

TEST_F(EnginesTest, AnInstalledPortIsPutInTheList) {
    download(Bytes::zip({{.name = "boom.exe", .bytes = "MZ"}}));

    make();
    primed();

    ASSERT_TRUE(installs());

    const std::vector<Added> put = addedFor(FIXED);

    ASSERT_EQ(put.size(), 1U);
    EXPECT_TRUE(put.front().dos);
    EXPECT_EQ(notifier.messages().back().title, "Fetched");
    EXPECT_EQ(notifier.messages().back().body,
              std::string(portOf(FIXED).name) + " " + std::string(portOf(FIXED).version) + " is ready to use");
}

TEST_F(EnginesTest, InstallPointsTheListedRowAtTheNewProgram) {
    download(Bytes::zip({{.name = "boom.exe", .bytes = "MZ"}}));

    listed().push_back({.name = "My Boom", .file = "/old/boom.exe", .dosbox = true, .portId = std::string(FIXED)});

    make();
    primed();

    ASSERT_TRUE(installs());

    ASSERT_EQ(updated.size(), 1U);
    EXPECT_EQ(updated.front().at, 0);
    EXPECT_EQ(updated.front().name, "My Boom");
    EXPECT_EQ(updated.front().file, from_path(Catalog::directory(portOf(FIXED)) / "boom.exe"));
    EXPECT_TRUE(addedFor(FIXED).empty());
}

TEST_F(EnginesTest, AnArchiveWithoutTheProgramFails) {
    const fs::path archive = download(Bytes::zip({{.name = "readme.txt", .bytes = "Boom"}}));

    make();
    primed();

    ASSERT_TRUE(installs());

    const std::string said = "Nothing in " + archive.filename().string() + " is named "
                             + std::string(portOf(FIXED).program) + ". It is kept in the downloads.";

    EXPECT_EQ(rowOf(FIXED).status, State::EngineState::Failed);
    EXPECT_EQ(rowOf(FIXED).error, said);
    EXPECT_TRUE(fs::exists(archive));
    EXPECT_EQ(notifier.messages().back().severity, ttk::Severity::Error);
}

TEST_F(EnginesTest, AnArchiveThatWillNotOpenFails) {
    const fs::path archive = download("this is not a zip");

    make();
    primed();

    ASSERT_TRUE(installs());

    EXPECT_EQ(rowOf(FIXED).status, State::EngineState::Failed);
    EXPECT_FALSE(rowOf(FIXED).error.empty());
    EXPECT_EQ(notifier.messages().back().title, "Could not unpack " + archive.filename().string());
}

TEST_F(EnginesTest, RemoveAfterInstallLeavesTheDownload) {
    const fs::path archive = download(Bytes::zip({{.name = "boom.exe", .bytes = "MZ"}}));

    make();
    primed();

    ASSERT_TRUE(installs());

    engines->remove(indexOf(FIXED));

    EXPECT_EQ(rowOf(FIXED).status, State::EngineState::Ready);
    EXPECT_FALSE(fs::exists(Catalog::directory(portOf(FIXED))));
    EXPECT_TRUE(fs::exists(archive));
    EXPECT_TRUE(std::ranges::none_of(listed(), [](const NameEntry &each) { return each.portId == FIXED; }));
}

}
