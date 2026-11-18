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
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/notices/Notifier.h"
#include "ttk/system/Paths.h"
#include "ttk/util/Format.h"

#include "Spawn.h"
#include "core/config/ConfigPaths.h"
#include "core/config/Session.h"
#include "core/launch/KnownPorts.h"
#include "core/ports/Catalog.h"
#include "core/ports/Detect.h"
#include "core/ports/Install.h"
#include "core/ports/Releases.h"
#include "core/util/Sha256.h"
#include "core/wad/Bytes.h"
#include "gui/services/Downloads.h"
#include "gui/state/State.h"
#include "support/FlatpakApp.h"
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

// What the GitHub port's release is called, once a check has answered.
constexpr std::string_view ASSET = "dsda-doom-1.2.3.zip";
constexpr std::string_view VERSION = "1.2.3";

int indexOf(const std::string_view id) {
    const auto downloads = Catalog::downloads();

    for (size_t at = 0; at < downloads.size(); ++at) {
        if (Catalog::id(downloads[at]) == id) {
            return static_cast<int>(at);
        }
    }

    return -1;
}

const Catalog::Download &downloadOf(const std::string_view id) {
    return Catalog::downloads()[static_cast<size_t>(indexOf(id))];
}

const State::DownloadRow &rowOf(const std::string_view id) {
    return State::get().downloads.rows.at(static_cast<size_t>(indexOf(id)));
}

std::vector<Engine> &listed() {
    return Session::get().config().engines;
}

fs::path onPath() {
    return tests::Sandbox::root() / "bin" / std::string(downloadOf(FOUND).executable);
}

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
    Port::Port port;
    bool managed;
    bool detected;
};

struct Updated {
    int at;
    std::string name;
    std::string file;
    bool dos;
};

class DownloadsTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_GE(indexOf(RELEASED), 0);
        ASSERT_GE(indexOf(FIXED), 0);
        ASSERT_GE(indexOf(FOUND), 0);

        State::get() = State::All{};
        Session::get().config() = Config{};

        std::error_code code;

        fs::remove_all(Catalog::directory(), code);
        fs::remove_all(Catalog::downloadDirectory(), code);
        fs::remove(ttk::Paths::data_directory() / ".releases", code);

        putOnPath();
    }

    void TearDown() override { downloads.reset(); }

    // Wired as the window wires it: what is added lands in the config's list.
    Downloads &make() {
        downloads = std::make_unique<Downloads>(&shell, &notifier);
        downloads->setOffline(true);

        downloads->addEngine = [this](const Engine &engine) {
            added.push_back({
                    .file = engine.file,
                    .name = engine.name,
                    .dos = engine.dosbox,
                    .port = engine.port,
                    .managed = engine.managed,
                    .detected = engine.detected,
            });
            listed().push_back(engine);

            return engine.name;
        };

        downloads->updateEngine =
                [this](const int at, const std::string &name, const std::string &file, const bool dos) {
                    updated.push_back({.at = at, .name = name, .file = file, .dos = dos});
                    listed().at(static_cast<size_t>(at)).file = file;
                };

        downloads->removeEngine = [this](const int at) {
            removed.push_back(at);
            listed().erase(listed().begin() + at);
        };

        downloads->scheduleSave = [this] { ++saves; };

        downloads->storeVersions = [this](const std::span<const Detect::Installed> read) {
            ++stores;

            if (Detect::storeVersions(listed(), read)) {
                ++saves;
            }
        };

        return *downloads;
    }

    void primed() { ASSERT_TRUE(shell.await(PATIENCE)); }

    // Asks for a find and waits for it to end.
    void finds() {
        downloads->find();

        ASSERT_TRUE(State::get().downloads.finding);
        ASSERT_TRUE(Spawn::until(shell, TICK, [] { return !State::get().downloads.finding; }, PATIENCE));
    }

    // Every port detection finds, listed by hand, so that nothing found is new.
    static void listEverythingFound() {
        for (const Detect::Found &found : Detect::ports()) {
            listed().push_back({.name = found.name, .file = from_path(found.file)});
        }
    }

    // As the service leaves a port it fetched: the file, and the version stamped beside it.
    static fs::path unpack(const std::string_view id, const std::string &version = "1.2.3") {
        const Catalog::Download &known = downloadOf(id);
        const fs::path where = Catalog::directory(known);
        const fs::path file = where / (std::string(known.executable) + (known.dos ? ".exe" : ""));

        Bytes::write(file, "#!/bin/sh\n");

        if (!version.empty()) {
            Bytes::write(where / ".zdl-version", version);
        }

        return file;
    }

    // The GitHub port's release as a check leaves it in the cache, promising what
    // a shelf copy is then held to.
    static void released(const std::string &sha256 = {}, const long long size = 0) {
        const Releases::Answer answer{
                .port = downloadOf(RELEASED).port,
                .version = std::string(VERSION),
                .url = "https://example.invalid/" + std::string(ASSET),
                .asset = std::string(ASSET),
                .verdict = static_cast<std::uint8_t>(State::DownloadStatus::Waiting),
                .note = "",
                .checked = Releases::now(),
                .size = size,
                .sha256 = sha256,
        };

        Releases::write({&answer, 1});
    }

    // On the shelf under the name the service gives a download.
    static fs::path shelved(const std::string_view id, const std::string_view asset, const std::string &bytes) {
        const fs::path at = Catalog::downloadDirectory() / Install::downloadName(downloadOf(id), asset);

        Bytes::write(at, bytes);

        return at;
    }

    // A shelf copy of the GitHub port's release that is what the release promises.
    static fs::path download(const std::string &bytes) {
        released(sha256Text(bytes), static_cast<long long>(bytes.size()));

        return shelved(RELEASED, ASSET, bytes);
    }

    static std::string fileOf(const std::string_view id = RELEASED) {
        return std::string(downloadOf(id).executable) + (downloadOf(id).dos ? ".exe" : "");
    }

    [[nodiscard]] std::vector<Added> addedFor(const std::string_view id) const {
        std::vector<Added> out;

        std::ranges::copy_if(added, std::back_inserter(out), [id](const Added &each) {
            return each.managed && each.port == downloadOf(id).port;
        });

        return out;
    }

    [[nodiscard]] bool addedFile(const fs::path &file) const {
        return std::ranges::any_of(added, [&file](const Added &each) { return each.file == from_path(file); });
    }

    [[nodiscard]] bool noticed(const std::string &body) const {
        return std::ranges::any_of(notifier.messages(), [&body](const ttk::Notice &each) { return each.body == body; });
    }

    bool installs(const std::string_view id = RELEASED) {
        downloads->install(indexOf(id));

        return Spawn::until(shell, TICK, [id] { return rowOf(id).status != State::DownloadStatus::Unpacking; });
    }

    [[nodiscard]] bool fetched() const {
        return std::ranges::any_of(notifier.messages(),
                                   [](const ttk::Notice &each) { return each.title == "Fetched"; });
    }

    tests::Shell shell;
    ttk::Notifier notifier;
    std::unique_ptr<Downloads> downloads;

    std::vector<Added> added;
    std::vector<Updated> updated;
    std::vector<int> removed;
    int saves = 0;

    // One for each read the service applied.
    int stores = 0;
};

TEST_F(DownloadsTest, EveryCataloguedPortHasARow) {
    make();

    const std::vector<State::DownloadRow> &rows = State::get().downloads.rows;

    ASSERT_EQ(rows.size(), Catalog::downloads().size());

    for (size_t at = 0; at < rows.size(); ++at) {
        EXPECT_EQ(rows[at].index, static_cast<int>(at));
        EXPECT_EQ(rows[at].name, Catalog::name(Catalog::downloads()[at]));
        EXPECT_EQ(rows[at].dos, Catalog::downloads()[at].dos);
    }
}

TEST_F(DownloadsTest, EachRowStartsFromWhatTheCatalogueSays) {
    make();

    for (const Catalog::Download &known : Catalog::downloads()) {
        const std::string_view id = Catalog::id(known);
        const State::DownloadRow &row = rowOf(id);

        if (known.repository.empty() && known.file.empty()) {
            EXPECT_EQ(row.status, State::DownloadStatus::Elsewhere) << id;
        } else if (Catalog::pattern(known).empty()) {
            EXPECT_EQ(row.status, State::DownloadStatus::Unavailable) << id;
            EXPECT_EQ(row.error, "There is no build of this one for this system") << id;
        } else if (!known.file.empty()) {
            EXPECT_EQ(row.status, State::DownloadStatus::Ready) << id;
            EXPECT_EQ(row.version, known.version) << id;
        } else {
            EXPECT_EQ(row.status, State::DownloadStatus::Waiting) << id;
            EXPECT_EQ(row.error, "") << id;
        }
    }
}

TEST_F(DownloadsTest, TheStateNamesWhereDownloadsGo) {
    make();

    EXPECT_EQ(State::get().downloads.directory, from_path(Catalog::directory()));
    EXPECT_EQ(State::get().downloads.downloadDirectory, from_path(Catalog::downloadDirectory()));
    EXPECT_FALSE(State::get().downloads.checking);
}

TEST_F(DownloadsTest, AnUnpackedDownloadIsInstalledWithItsStampedVersion) {
    const fs::path file = unpack(RELEASED, "0.29.3");

    make();

    EXPECT_EQ(rowOf(RELEASED).status, State::DownloadStatus::Installed);
    EXPECT_EQ(rowOf(RELEASED).have, "0.29.3");
    EXPECT_EQ(rowOf(RELEASED).file, from_path(file));
}

TEST_F(DownloadsTest, AnUnpackedDownloadWithoutAStampHasNoVersion) {
    unpack(RELEASED, "");

    make();

    EXPECT_EQ(rowOf(RELEASED).status, State::DownloadStatus::Installed);
    EXPECT_EQ(rowOf(RELEASED).have, "");
}

TEST_F(DownloadsTest, AnUnpackedDownloadMissingFromTheConfigIsReported) {
    unpack(RELEASED);

    make();

    EXPECT_TRUE(noticed("A source port ZDL4 had fetched was missing from this config."));
}

TEST_F(DownloadsTest, SeveralMissingDownloadsAreReportedTogether) {
    unpack(RELEASED);
    unpack(FIXED);

    make();

    EXPECT_TRUE(noticed("2 source ports ZDL4 had fetched were missing from this config."));
}

TEST_F(DownloadsTest, RelistPutsAnUnpackedDownloadBackInTheList) {
    const fs::path file = unpack(RELEASED);

    make().relist();

    const std::vector<Added> back = addedFor(RELEASED);

    ASSERT_EQ(back.size(), 1U);
    EXPECT_EQ(back.front().file, from_path(file));
    EXPECT_EQ(back.front().name, Catalog::name(downloadOf(RELEASED)));
    EXPECT_FALSE(back.front().dos);
}

TEST_F(DownloadsTest, RelistAddsAnEngineOnce) {
    unpack(RELEASED);

    make().relist();
    downloads->relist();

    EXPECT_EQ(addedFor(RELEASED).size(), 1U);
}

TEST_F(DownloadsTest, ADownloadWithAnEngineIsNotRelisted) {
    unpack(RELEASED);

    const fs::path elsewhere = tests::Sandbox::scratch("engines") / "elsewhere";

    Bytes::write(elsewhere, "#!/bin/sh\n");
    listed().push_back({.name = "Mine",
                        .file = from_path(elsewhere),
                        .dosbox = false,
                        .port = downloadOf(RELEASED).port,
                        .managed = true});

    make().relist();

    EXPECT_TRUE(addedFor(RELEASED).empty());
    EXPECT_FALSE(noticed("A source port ZDL4 had fetched was missing from this config."));
}

TEST_F(DownloadsTest, ARowWhoseFileIsGoneIsPointedAtTheShelfCopy) {
    const fs::path file = unpack(RELEASED);

    listed().push_back({.name = "Mine",
                        .file = "/moved/away/dsda-doom",
                        .dosbox = false,
                        .port = downloadOf(RELEASED).port,
                        .managed = true});

    make().relist();

    ASSERT_EQ(listed().size(), 1U);
    EXPECT_EQ(listed().front().file, from_path(file));
    EXPECT_EQ(listed().front().name, "Mine");
    EXPECT_TRUE(listed().front().managed);
    EXPECT_TRUE(addedFor(RELEASED).empty());
}

TEST_F(DownloadsTest, ARowWhoseFileStillExistsKeepsItsPathAtRelist) {
    unpack(RELEASED);

    const fs::path elsewhere = tests::Sandbox::scratch("engines") / "kept";

    Bytes::write(elsewhere, "#!/bin/sh\n");
    listed().push_back({.name = "Mine",
                        .file = from_path(elsewhere),
                        .dosbox = false,
                        .port = downloadOf(RELEASED).port,
                        .managed = true});

    make().relist();

    EXPECT_EQ(listed().front().file, from_path(elsewhere));
    EXPECT_TRUE(updated.empty());
}

TEST_F(DownloadsTest, AnEngineInsideADownloadDirectoryWithoutTheFlagIsNotAdopted) {
    const fs::path file = unpack(RELEASED);

    make();

    listed().push_back({.name = "Old", .file = from_path(file), .dosbox = false});

    downloads->relist();

    EXPECT_FALSE(listed().front().managed);
    EXPECT_EQ(listed().front().port, Port::Port::unknown);
    EXPECT_EQ(addedFor(RELEASED).size(), 1U);
}

TEST_F(DownloadsTest, AManagedEngineOfAnotherPortDoesNotCount) {
    const fs::path file = unpack(RELEASED);

    listed().push_back({
            .name = "Woof",
            .file = from_path(file),
            .dosbox = false,
            .port = Port::Port::woof,
            .managed = true,
    });

    make().relist();

    EXPECT_EQ(addedFor(RELEASED).size(), 1U);
}

TEST_F(DownloadsTest, AStartWithAnEmptyListAddsAPortFoundOnThePath) {
    make();
    primed();

    EXPECT_TRUE(addedFile(onPath()));
    EXPECT_FALSE(State::get().downloads.finding);
    EXPECT_TRUE(std::ranges::any_of(notifier.messages(),
                                    [](const ttk::Notice &each) { return each.body.ends_with(" in the list now."); }));
}

TEST_F(DownloadsTest, ADetectedPortIsAddedUnmanagedAsThePortItIs) {
    make();
    primed();

    const auto found = std::ranges::find_if(added, [](const Added &each) { return each.file == from_path(onPath()); });

    ASSERT_NE(found, added.end());
    EXPECT_FALSE(found->managed);
    EXPECT_TRUE(found->detected);
    EXPECT_EQ(found->port, downloadOf(FOUND).port);
    EXPECT_EQ(found->name, Catalog::name(downloadOf(FOUND)));
}

TEST_F(DownloadsTest, AStartWithEnginesListedAddsNothing) {
    listed().push_back({.name = "Mine", .file = "/nowhere/mine"});

    make();

    EXPECT_FALSE(State::get().downloads.finding);

    primed();

    EXPECT_TRUE(added.empty());
    EXPECT_EQ(listed().size(), 1U);
}

TEST_F(DownloadsTest, AnotherConfigWithAnEmptyListGetsTheFoundPorts) {
    listed().push_back({.name = "Mine", .file = "/nowhere/mine"});

    make();
    primed();

    listed().clear();
    downloads->opened(true);

    ASSERT_TRUE(State::get().downloads.finding);
    ASSERT_TRUE(Spawn::until(shell, TICK, [] { return !State::get().downloads.finding; }, PATIENCE));
    EXPECT_TRUE(addedFile(onPath()));
}

TEST_F(DownloadsTest, AnotherConfigWithEnginesListedFindsNothing) {
    listed().push_back({.name = "Mine", .file = "/nowhere/mine"});

    make();
    primed();

    downloads->opened(true);

    EXPECT_FALSE(State::get().downloads.finding);

    primed();

    EXPECT_TRUE(added.empty());
}

TEST_F(DownloadsTest, AConfigEmptiedWhileTheStartFindRunsGetsNothing) {
    make();

    ASSERT_TRUE(State::get().downloads.finding);

    downloads->opened(false);

    ASSERT_TRUE(Spawn::until(shell, TICK, [] { return !State::get().downloads.finding; }, PATIENCE));
    EXPECT_TRUE(added.empty());
    EXPECT_TRUE(notifier.messages().empty());
}

TEST_F(DownloadsTest, AFindRunningWhenAnotherConfigOpensIsDoneAgainForIt) {
    listed().push_back({.name = "Found", .file = from_path(onPath())});

    make();
    primed();

    downloads->find();

    // The running find saw the port on the path listed. The config opened now lacks it.
    listed().clear();
    listed().push_back({.name = "Mine", .file = "/nowhere/mine"});
    downloads->opened(true);

    ASSERT_TRUE(Spawn::until(shell, TICK, [] { return !State::get().downloads.finding; }, PATIENCE));
    EXPECT_TRUE(addedFile(onPath()));
}

TEST_F(DownloadsTest, AFindAddsOnlyFilesTheListLacks) {
    listEverythingFound();
    std::erase_if(listed(), [](const Engine &each) { return each.file == from_path(onPath()); });

    const size_t before = listed().size();

    make();
    primed();
    finds();

    ASSERT_EQ(added.size(), 1U);
    EXPECT_EQ(added.front().file, from_path(onPath()));
    EXPECT_EQ(listed().size(), before + 1);
}

TEST_F(DownloadsTest, AFindThatFindsNothingNewSaysSo) {
    listEverythingFound();

    make();
    primed();
    finds();

    EXPECT_TRUE(added.empty());
    EXPECT_TRUE(noticed("No other engines were found on this machine."));
}

TEST_F(DownloadsTest, AListFilledWhileTheStartFindRunsGetsNothing) {
    make();

    ASSERT_TRUE(State::get().downloads.finding);

    listed().push_back({.name = "Mine", .file = "/nowhere/mine"});
    primed();

    EXPECT_TRUE(added.empty());
    EXPECT_FALSE(State::get().downloads.finding);
    EXPECT_TRUE(notifier.messages().empty());
}

TEST_F(DownloadsTest, ARemovedEngineComesBackOnlyWhenAsked) {
    listed().push_back({.name = "Mine", .file = "/nowhere/mine"});
    listed().push_back({.name = "Found", .file = from_path(onPath())});

    make();
    primed();

    listed().pop_back();
    downloads->opened(true);

    EXPECT_FALSE(State::get().downloads.finding);
    EXPECT_FALSE(addedFile(onPath()));

    finds();

    EXPECT_TRUE(addedFile(onPath()));
}

#ifdef __linux__
// A UZDoom Flatpak whose install names `version`.
tests::FlatpakApp flatpakNaming(const std::string &version) {
    const tests::FlatpakApp deployed =
            tests::deployFlatpak(tests::Sandbox::scratch("downloads-flatpak") / "flatpak", "org.zdoom.UZDoom");

    tests::nameRelease(deployed, version);

    return deployed;
}

TEST_F(DownloadsTest, AStartStoresTheReleaseAListedFlatpakNames) {
    listed().push_back({.name = "UZDoom", .file = from_path(flatpakNaming("5.0.3").exported), .version = "4.14.3"});

    make();
    primed();

    EXPECT_EQ(listed().front().version, "5.0.3");
    EXPECT_EQ(saves, 1);
}

TEST_F(DownloadsTest, AStartWithTheStoredReleaseSavesNothing) {
    listed().push_back({.name = "UZDoom", .file = from_path(flatpakNaming("5.0.3").exported), .version = "5.0.3"});

    make();
    primed();

    EXPECT_EQ(listed().front().version, "5.0.3");
    EXPECT_EQ(saves, 0);
}

TEST_F(DownloadsTest, AFindStoresTheReleaseOfAnUpdatedFlatpakAndAddsNewPorts) {
    const fs::path root = tests::Sandbox::scratch("downloads-flatpak-update") / "flatpak";
    const tests::FlatpakApp deployed = tests::deployFlatpak(root, "org.zdoom.UZDoom");

    tests::nameRelease(deployed, "4.14.3");
    listed().push_back({.name = "UZDoom", .file = from_path(deployed.exported), .version = "4.14.3"});

    make();
    primed();

    ASSERT_EQ(saves, 0);

    tests::nameRelease(tests::deployFlatpak(root, "org.zdoom.UZDoom", "stable", "4567cdef"), "5.0.3");
    finds();

    EXPECT_EQ(listed().front().version, "5.0.3");
    EXPECT_TRUE(addedFile(onPath()));
}

TEST_F(DownloadsTest, AnotherConfigStoresTheReleasesItsFlatpaksName) {
    listed().push_back({.name = "Mine", .file = "/nowhere/mine"});

    make();
    primed();

    listed().clear();
    listed().push_back({.name = "UZDoom", .file = from_path(flatpakNaming("5.0.3").exported), .version = "4.14.3"});
    downloads->opened(true);

    EXPECT_FALSE(State::get().downloads.finding);

    primed();

    EXPECT_EQ(listed().front().version, "5.0.3");
    EXPECT_TRUE(added.empty());
}

TEST_F(DownloadsTest, AnotherConfigOpenedDuringAFindGetsTheReleasesItsFlatpaksName) {
    listed().push_back({.name = "Mine", .file = "/nowhere/mine"});

    make();
    primed();
    downloads->find();

    listed().clear();
    listed().push_back({.name = "UZDoom", .file = from_path(flatpakNaming("5.0.3").exported), .version = "4.14.3"});
    downloads->opened(true);

    ASSERT_TRUE(Spawn::until(shell, TICK, [] { return !State::get().downloads.finding; }, PATIENCE));
    EXPECT_EQ(listed().front().version, "5.0.3");
}

TEST_F(DownloadsTest, AFlatpakThatNamesNoReleaseKeepsItsVersionAtStart) {
    listed().push_back({.name = "UZDoom", .file = from_path(flatpakNaming("nightly").exported), .version = "4.14.3"});

    make();
    primed();

    EXPECT_EQ(listed().front().version, "4.14.3");
    EXPECT_EQ(saves, 0);
}
#endif

TEST_F(DownloadsTest, ASecondFindWhileOneRunsDoesNothing) {
    listed().push_back({.name = "Mine", .file = "/nowhere/mine"});

    make();
    primed();

    const int before = stores;

    downloads->find();
    downloads->find();

    ASSERT_TRUE(Spawn::until(shell, TICK, [] { return !State::get().downloads.finding; }, PATIENCE));

    // The service waits for its reader as it goes, so a second read has posted by now.
    downloads.reset();

    EXPECT_EQ(stores - before + static_cast<int>(shell.posted()), 1);
    EXPECT_EQ(std::ranges::count_if(added, [](const Added &each) { return each.file == from_path(onPath()); }), 1);
}

TEST_F(DownloadsTest, AClickDuringTheStartFindJoinsItAndAddsToAListFilledMeanwhile) {
    make();

    ASSERT_TRUE(State::get().downloads.finding);

    listed().push_back({.name = "Mine", .file = "/nowhere/mine"});
    downloads->find();

    ASSERT_TRUE(Spawn::until(shell, TICK, [] { return !State::get().downloads.finding; }, PATIENCE));
    EXPECT_TRUE(addedFile(onPath()));

    downloads.reset();

    EXPECT_EQ(stores + static_cast<int>(shell.posted()), 1);
}

TEST_F(DownloadsTest, ForgetRemovesTheEngineAndTheFilesOfItsDownload) {
    const fs::path file = unpack(RELEASED);

    listed().push_back({.name = "Dsda",
                        .file = from_path(file),
                        .dosbox = false,
                        .port = downloadOf(RELEASED).port,
                        .managed = true});

    make().forget(0);

    EXPECT_EQ(removed, (std::vector<int>{0}));
    EXPECT_FALSE(fs::exists(Catalog::directory(downloadOf(RELEASED))));
    EXPECT_NE(rowOf(RELEASED).status, State::DownloadStatus::Installed);
    EXPECT_EQ(rowOf(RELEASED).file, "");
    EXPECT_EQ(saves, 1);
}

TEST_F(DownloadsTest, ForgetOfAnEngineOutsideEveryDownloadOnlyRemovesTheEngine) {
    const fs::path file = unpack(RELEASED);

    listed().push_back({.name = "Dsda",
                        .file = from_path(file),
                        .dosbox = false,
                        .port = downloadOf(RELEASED).port,
                        .managed = true});
    listed().push_back({.name = "Other", .file = "/usr/games/other", .dosbox = false});

    make().forget(1);

    EXPECT_EQ(removed, (std::vector<int>{1}));
    EXPECT_TRUE(fs::exists(file));
    EXPECT_EQ(rowOf(RELEASED).status, State::DownloadStatus::Installed);
}

TEST_F(DownloadsTest, ForgetOutOfRangeDoesNothing) {
    make();

    downloads->forget(-1);
    downloads->forget(0);

    EXPECT_TRUE(removed.empty());
}

TEST_F(DownloadsTest, RemoveTakesTheDownloadFilesAndOnlyTheEngineItKeeps) {
    const fs::path file = unpack(RELEASED);
    const std::string inside = from_path(file.parent_path() / "other-build");

    listed().push_back({.name = "Dsda",
                        .file = from_path(file),
                        .dosbox = false,
                        .port = downloadOf(RELEASED).port,
                        .managed = true});
    listed().push_back({.name = "Keep", .file = "/usr/games/keep", .dosbox = false});
    listed().push_back({.name = "Older", .file = inside, .dosbox = false});

    make().remove(indexOf(RELEASED));

    EXPECT_EQ(removed, (std::vector<int>{0}));
    ASSERT_EQ(listed().size(), 2U);
    EXPECT_EQ(listed()[0].name, "Keep");
    EXPECT_EQ(listed()[1].name, "Older");
    EXPECT_FALSE(fs::exists(file));
    EXPECT_EQ(rowOf(RELEASED).status, State::DownloadStatus::Waiting);
}

// A relative executable in portable mode leaves no data directory, so every port directory is empty.
TEST_F(DownloadsTest, WithoutADataDirectoryNoFileOutsideIsDeleted) {
    const fs::path executable = ttk::Paths::executable();
    const fs::path kept = tests::Sandbox::scratch("engines-kept") / "dsda-doom";

    Bytes::write(kept, "#!/bin/sh\n");
    ttk::Paths::set_executable("ZDL4");
    ConfigPaths::setPortable(true);

    listed().push_back({.name = "Dsda",
                        .file = from_path(kept),
                        .dosbox = false,
                        .port = downloadOf(RELEASED).port,
                        .managed = true});
    listed().push_back({.name = "Keep", .file = "/usr/games/keep", .dosbox = false});

    make().remove(indexOf(RELEASED));
    downloads->forget(0);

    ConfigPaths::setPortable(false);
    ttk::Paths::set_executable(executable);

    EXPECT_EQ(removed, (std::vector<int>{0, 0}));
    EXPECT_TRUE(listed().empty());
    EXPECT_TRUE(fs::exists(kept));
}

TEST_F(DownloadsTest, RemoveTakesTheRowItKeepsWhereverItsFileIs) {
    unpack(RELEASED);

    listed().push_back({.name = "Moved",
                        .file = "/usr/games/moved",
                        .dosbox = false,
                        .port = downloadOf(RELEASED).port,
                        .managed = true});

    make().remove(indexOf(RELEASED));

    EXPECT_EQ(removed, (std::vector<int>{0}));
    EXPECT_TRUE(listed().empty());
}

TEST_F(DownloadsTest, ForgetKeepsTheFilesForAnEngineAddedByHandInsideThePortsFolder) {
    const fs::path file = unpack(RELEASED);

    listed().push_back({.name = "Old", .file = from_path(file), .dosbox = false});

    make().forget(0);

    EXPECT_EQ(removed, (std::vector<int>{0}));
    EXPECT_TRUE(fs::exists(file));
    EXPECT_EQ(rowOf(RELEASED).status, State::DownloadStatus::Installed);
}

TEST_F(DownloadsTest, ForgetDeletesTheFolderForAManagedRowWithAFileElsewhere) {
    const fs::path file = unpack(RELEASED);

    listed().push_back({.name = "Moved",
                        .file = "/usr/games/moved",
                        .dosbox = false,
                        .port = downloadOf(RELEASED).port,
                        .managed = true});

    make().forget(0);

    EXPECT_EQ(removed, (std::vector<int>{0}));
    EXPECT_FALSE(fs::exists(file));
    EXPECT_NE(rowOf(RELEASED).status, State::DownloadStatus::Installed);
}

TEST_F(DownloadsTest, OutOfRangeRowsAreIgnored) {
    make();

    const std::vector<State::DownloadRow> before = State::get().downloads.rows;
    const int past = static_cast<int>(Catalog::downloads().size());

    downloads->install(-1);
    downloads->install(past);
    downloads->cancel(-1);
    downloads->cancel(past);
    downloads->remove(-1);
    downloads->remove(past);

    EXPECT_EQ(State::get().downloads.rows, before);
    EXPECT_EQ(shell.pending(), 0U);
}

TEST_F(DownloadsTest, AnEmptyShelfIsNothing) {
    make();
    primed();

    EXPECT_FALSE(State::get().downloads.cached);
    EXPECT_EQ(State::get().downloads.cachedText, "Nothing");
}

TEST_F(DownloadsTest, MeasureCountsTheDownloads) {
    make();
    primed();

    Bytes::write(Catalog::downloadDirectory() / "a.zip", std::string(std::size_t{2} * 1024 * 1024, 'a'));
    Bytes::write(Catalog::downloadDirectory() / "b.zip", std::string(std::size_t{1024} * 1024 / 2, 'b'));

    downloads->measure();

    EXPECT_TRUE(State::get().downloads.cached);
    EXPECT_EQ(State::get().downloads.cachedText, "2.5 MB");
}

TEST_F(DownloadsTest, ASmallShelfIsMeasuredInKilobytes) {
    make();
    primed();

    Bytes::write(Catalog::downloadDirectory() / "a.zip", std::string(3000, 'a'));

    downloads->measure();

    EXPECT_EQ(State::get().downloads.cachedText, "3 KB");
}

TEST_F(DownloadsTest, AFewBytesAreAKilobyte) {
    make();
    primed();

    Bytes::write(Catalog::downloadDirectory() / "a.zip", "abc");

    downloads->measure();

    EXPECT_EQ(State::get().downloads.cachedText, "1 KB");
}

TEST_F(DownloadsTest, TheShelfIsMeasuredAtStartUp) {
    Bytes::write(Catalog::downloadDirectory() / "a.zip", std::string(3000, 'a'));

    make();
    primed();

    EXPECT_TRUE(State::get().downloads.cached);
    EXPECT_EQ(State::get().downloads.cachedText, "3 KB");
}

TEST_F(DownloadsTest, ClearDownloadsEmptiesTheShelf) {
    Bytes::write(Catalog::downloadDirectory() / "a.zip", std::string(3000, 'a'));
    Bytes::write(Catalog::downloadDirectory() / "nested" / "b.zip", "b");

    make();
    primed();

    downloads->clearDownloads();

    EXPECT_TRUE(fs::is_empty(Catalog::downloadDirectory()));
    EXPECT_FALSE(State::get().downloads.cached);
    EXPECT_EQ(State::get().downloads.cachedText, "Nothing");
    EXPECT_EQ(notifier.messages().back().title, "The downloads are empty");
}

TEST_F(DownloadsTest, ACachedReleaseFillsTheRow) {
    const Releases::Answer answer{
            .port = downloadOf(RELEASED).port,
            .version = "0.29.3",
            .url = "https://example.invalid/dsda-doom-0.29.3-linux-x86_64.appimage",
            .asset = "dsda-doom-0.29.3-linux-x86_64.appimage",
            .verdict = static_cast<std::uint8_t>(State::DownloadStatus::Waiting),
            .note = "",
            .checked = Releases::now(),
            .size = 3LL * 1024 * 1024,
            .sha256 = "",
    };

    Releases::write({&answer, 1});

    make();

    EXPECT_EQ(rowOf(RELEASED).status, State::DownloadStatus::Ready);
    EXPECT_EQ(rowOf(RELEASED).version, "0.29.3");
    EXPECT_EQ(rowOf(RELEASED).sizeText, "3.0 MB");
}

TEST_F(DownloadsTest, ACachedVerdictWithoutABuildIsTheRowsState) {
    const Releases::Answer answer{
            .port = downloadOf(RELEASED).port,
            .version = "0.29.3",
            .url = "",
            .asset = "",
            .verdict = static_cast<std::uint8_t>(State::DownloadStatus::Unavailable),
            .note = "The latest release has no build for this system",
            .checked = Releases::now(),
            .size = 0,
            .sha256 = "",
    };

    Releases::write({&answer, 1});

    make();

    EXPECT_EQ(rowOf(RELEASED).status, State::DownloadStatus::Unavailable);
    EXPECT_EQ(rowOf(RELEASED).error, "The latest release has no build for this system");
    EXPECT_EQ(rowOf(RELEASED).sizeText, "");
}

TEST_F(DownloadsTest, ACachedVerdictOutOfRangeIsWaiting) {
    const Releases::Answer answer{
            .port = downloadOf(RELEASED).port,
            .version = "",
            .url = "",
            .asset = "",
            .verdict = 200,
            .note = "",
            .checked = Releases::now(),
            .size = 0,
            .sha256 = "",
    };

    Releases::write({&answer, 1});

    make();

    EXPECT_EQ(rowOf(RELEASED).status, State::DownloadStatus::Waiting);
}

TEST_F(DownloadsTest, ACorruptReleaseCacheIsIgnoredAndRemoved) {
    const fs::path cache = ttk::Paths::data_directory() / ".releases";

    Bytes::write(cache, "not a release cache");

    make();

    EXPECT_EQ(rowOf(RELEASED).status, State::DownloadStatus::Waiting);
    EXPECT_FALSE(fs::exists(cache));
}

TEST_F(DownloadsTest, RefreshAsksNothingOffline) {
    make();

    downloads->refresh(true);

    EXPECT_FALSE(State::get().downloads.checking);
    EXPECT_EQ(rowOf(RELEASED).status, State::DownloadStatus::Waiting);
    EXPECT_EQ(shell.pending(), 0U);
}

TEST_F(DownloadsTest, InstallAsksNothingOfTheNetworkWhileOffline) {
    make();

    downloads->install(indexOf(RELEASED));

    EXPECT_FALSE(rowOf(RELEASED).asking);
    EXPECT_EQ(shell.pending(), 0U);
}

TEST_F(DownloadsTest, InstallUnpacksADownloadAlreadyOnTheShelf) {
    download(Bytes::zip({{.name = fileOf(), .bytes = "#!/bin/sh\n"}, {.name = "readme.txt", .bytes = "Dsda"}}));

    make();
    primed();

    ASSERT_TRUE(installs());

    const fs::path at = Catalog::directory(downloadOf(RELEASED)) / fileOf();

    EXPECT_EQ(rowOf(RELEASED).status, State::DownloadStatus::Installed);
    EXPECT_EQ(rowOf(RELEASED).file, from_path(at));
    EXPECT_EQ(rowOf(RELEASED).have, VERSION);
    EXPECT_EQ(Install::stampedVersion(downloadOf(RELEASED)), VERSION);
    EXPECT_EQ(shell.pending(), 0U);
}

TEST_F(DownloadsTest, AnInstalledDownloadIsPutInTheList) {
    download(Bytes::zip({{.name = fileOf(), .bytes = "#!/bin/sh\n"}}));

    make();
    primed();

    ASSERT_TRUE(installs());

    const std::vector<Added> put = addedFor(RELEASED);

    ASSERT_EQ(put.size(), 1U);
    EXPECT_FALSE(put.front().dos);
    EXPECT_EQ(notifier.messages().back().title, "Fetched");
    EXPECT_EQ(notifier.messages().back().body,
              std::string(Catalog::name(downloadOf(RELEASED))) + " " + std::string(VERSION) + " is ready to use");
}

TEST_F(DownloadsTest, InstallPointsTheListedRowAtTheNewProgram) {
    download(Bytes::zip({{.name = fileOf(), .bytes = "#!/bin/sh\n"}}));

    listed().push_back({.name = "My Dsda",
                        .file = "/old/dsda-doom",
                        .dosbox = false,
                        .port = downloadOf(RELEASED).port,
                        .managed = true});

    make();
    primed();

    ASSERT_TRUE(installs());

    ASSERT_EQ(updated.size(), 1U);
    EXPECT_EQ(updated.front().at, 0);
    EXPECT_EQ(updated.front().name, "My Dsda");
    EXPECT_EQ(updated.front().file, from_path(Catalog::directory(downloadOf(RELEASED)) / fileOf()));
    EXPECT_TRUE(addedFor(RELEASED).empty());
}

TEST_F(DownloadsTest, AnArchiveWithoutTheProgramFails) {
    const fs::path archive = download(Bytes::zip({{.name = "readme.txt", .bytes = "Dsda"}}));

    make();
    primed();

    ASSERT_TRUE(installs());

    const std::string said = "Nothing in " + archive.filename().string() + " is named "
                             + std::string(downloadOf(RELEASED).executable) + ". It is kept in the downloads.";

    EXPECT_EQ(rowOf(RELEASED).status, State::DownloadStatus::Failed);
    EXPECT_EQ(rowOf(RELEASED).error, said);
    EXPECT_TRUE(fs::exists(archive));
    EXPECT_EQ(notifier.messages().back().severity, ttk::Severity::Error);
}

TEST_F(DownloadsTest, AnArchiveThatWillNotOpenFails) {
    const fs::path archive = download("this is not a zip");

    make();
    primed();

    ASSERT_TRUE(installs());

    EXPECT_EQ(rowOf(RELEASED).status, State::DownloadStatus::Failed);
    EXPECT_FALSE(rowOf(RELEASED).error.empty());
    EXPECT_TRUE(fs::exists(archive)) << "it is what the release promised, so it is kept";
    EXPECT_EQ(notifier.messages().back().title, "Could not unpack " + archive.filename().string());
}

TEST_F(DownloadsTest, RemoveAfterInstallLeavesTheDownload) {
    const fs::path archive = download(Bytes::zip({{.name = fileOf(), .bytes = "#!/bin/sh\n"}}));

    make();
    primed();

    ASSERT_TRUE(installs());

    downloads->remove(indexOf(RELEASED));

    EXPECT_EQ(rowOf(RELEASED).status, State::DownloadStatus::Ready);
    EXPECT_FALSE(fs::exists(Catalog::directory(downloadOf(RELEASED))));
    EXPECT_TRUE(fs::exists(archive));
    EXPECT_TRUE(std::ranges::none_of(listed(), [](const Engine &each) { return each.managed; }));
}

TEST_F(DownloadsTest, AShelfCopyWithNoPromiseIsTakenAsItIs) {
    released();
    shelved(RELEASED, ASSET, Bytes::zip({{.name = fileOf(), .bytes = "#!/bin/sh\n"}}));

    make();
    primed();

    ASSERT_TRUE(installs());

    EXPECT_EQ(rowOf(RELEASED).status, State::DownloadStatus::Installed);
    EXPECT_TRUE(fetched());
}

TEST_F(DownloadsTest, AShelfCopyOfAnotherChecksumIsDroppedAndFetchedAfresh) {
    released(sha256Text("the release as it is now"));

    const fs::path archive = shelved(RELEASED, ASSET, Bytes::zip({{.name = fileOf(), .bytes = "#!/bin/sh\n"}}));

    make();
    primed();

    ASSERT_TRUE(installs());

    EXPECT_FALSE(fs::exists(archive));
    EXPECT_FALSE(fs::exists(Catalog::directory(downloadOf(RELEASED))));
    EXPECT_EQ(rowOf(RELEASED).status, State::DownloadStatus::Ready) << "offline, the fetch that follows does not start";
    EXPECT_EQ(rowOf(RELEASED).error, "");
    EXPECT_FALSE(fetched());
}

TEST_F(DownloadsTest, AShelfCopyOfAnotherSizeIsDropped) {
    released({}, 12345);

    const fs::path archive = shelved(RELEASED, ASSET, Bytes::zip({{.name = fileOf(), .bytes = "#!/bin/sh\n"}}));

    make();
    primed();

    ASSERT_TRUE(installs());

    EXPECT_FALSE(fs::exists(archive));
    EXPECT_EQ(rowOf(RELEASED).status, State::DownloadStatus::Ready);
    EXPECT_FALSE(fetched());
}

TEST_F(DownloadsTest, AFixedDownloadShelfCopyIsHeldToTheCatalogsChecksum) {
    const Catalog::Download &known = downloadOf(FIXED);
    const fs::path archive =
            shelved(FIXED, Install::fileNameOf(known.file), Bytes::zip({{.name = fileOf(FIXED), .bytes = "MZ"}}));

    make();
    primed();

    ASSERT_TRUE(installs(FIXED));

    EXPECT_FALSE(fs::exists(archive));
    EXPECT_FALSE(fs::exists(Catalog::directory(known)));
    EXPECT_EQ(rowOf(FIXED).status, State::DownloadStatus::Ready);
    EXPECT_FALSE(fetched());
}

}
