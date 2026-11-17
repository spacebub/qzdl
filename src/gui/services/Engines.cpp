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
#include <format>
#include <utility>

#include "ttk/util/Clock.h"
#include "ttk/util/Format.h"

#include "core/config/Session.h"
#include "core/ports/Detect.h"
#include "core/ports/Install.h"
#include "core/ports/Releases.h"
#include "gui/services/Engines.h"

using namespace ttk;

namespace {

// The release cache stores the verdict as its own enum value.
// Waiting is the defalt fallback.
State::EngineState verdictFrom(const std::uint8_t stored) {
    return stored <= static_cast<std::uint8_t>(State::EngineState::Failed) ? static_cast<State::EngineState>(stored)
                                                                           : State::EngineState::Waiting;
}

std::string megabytes(const long long bytes) {
    if (bytes <= 0) {
        return {};
    }

    return std::format("{:.1f} MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
}

std::string measured(const long long bytes) {
    if (bytes >= 1024LL * 1024) {
        return megabytes(bytes);
    }

    return bytes > 0 ? std::to_string(std::max<long long>(1, (bytes + 512) / 1024)) + " KB" : "Nothing";
}

std::string text(const std::string_view value) {
    return {value.begin(), value.end()};
}

// An empty root holds nothing, or every absolute path would be inside it.
bool inside(const std::string &root, const std::string &file) {
    return !root.empty() && file.starts_with(root + "/");
}

// Whether the config row is the one ZDL4 keeps for the catalog port.
bool keptFor(const PortEntry &entry, const Catalog::Port &known) {
    return entry.managed && entry.program == known.program;
}

}

Engines::Engines(Clock *clock, Notifier *notifier)
    : _clock(clock), _notifier(notifier), _entries(Catalog::ports().size()) {
    State::PortsState &state = State::get().ports;

    state.directory = Format::from_path(Catalog::directory());
    state.downloads = Format::from_path(Catalog::downloads());

    // Before settle(): a cached answer is what makes a row "ready".
    readCache();

    for (size_t row = 0; row < _entries.size(); row++) {
        settle(static_cast<int>(row));
    }

    relist();
    push();

    // Detection walks /opt, the home directory, PATH and the flatpak exports, and
    // the download shelf is a directory of its own: on a cold disk that is the whole
    // of start-up, and the window does not exist yet to say so.
    _priming = std::thread([this, alive = _alive] {
        (void)Detect::ports();

        const long long held = Install::shelfBytes();

        _clock->post([this, alive, held] {
            if (!alive->load()) {
                return;
            }

            _cached = held;

            discover();
            push();
        });
    });
}

Engines::~Engines() {
    _alive->store(false);

    if (_priming.joinable()) {
        _priming.join();
    }
}

const Catalog::Port &Engines::port(const int row) {
    return Catalog::ports()[static_cast<size_t>(row)];
}

void Engines::give(const int row, const State::EngineState state, const std::string &error) {
    Entry &entry = _entries[static_cast<size_t>(row)];

    entry.state = state;
    entry.error = error;

    push();
}

void Engines::settle(const int row) {
    const Catalog::Port &known = port(row);
    Entry &entry = _entries[static_cast<size_t>(row)];

    entry.error.clear();
    entry.have.clear();
    entry.progress = 0;

    if (known.repository.empty() && known.file.empty()) {
        entry.state = State::EngineState::Elsewhere;

        return;
    }

    if (Catalog::pattern(known).empty()) {
        entry.state = State::EngineState::Unavailable;
        entry.error = "There is no build of this one for this system";

        return;
    }

    if (!known.file.empty()) {
        entry.url = text(known.file);
        entry.asset = Install::fileNameOf(entry.url);
        entry.version = text(known.version);
        entry.sha256 = text(known.sha256);
    }

    const std::filesystem::path where = Catalog::directory(known);
    const std::filesystem::path found =
            where.empty() ? std::filesystem::path() : Catalog::program(where, known.executable, known.dos);

    if (!found.empty()) {
        entry.have = Install::stampedVersion(known);
        entry.file = Format::from_path(found);
        entry.state = State::EngineState::Installed;

        return;
    }

    if (!entry.url.empty()) {
        entry.state = State::EngineState::Ready;

        return;
    }

    entry.state = entry.verdict;
    entry.error = entry.note;
}

void Engines::readCache() {
    for (const Releases::Answer &one : Releases::read()) {
        for (size_t row = 0; row < _entries.size(); row++) {
            if (port(static_cast<int>(row)).program != one.program) {
                continue;
            }

            Entry &entry = _entries[row];

            entry.checked = one.checked;
            entry.version = one.version;
            entry.url = one.url;
            entry.asset = one.asset;
            entry.size = one.size;
            entry.sha256 = one.sha256;
            entry.verdict = verdictFrom(one.verdict);
            entry.note = one.note;

            break;
        }
    }
}

void Engines::writeCache() const {
    std::vector<Releases::Answer> answers;

    for (size_t row = 0; row < _entries.size(); row++) {
        const Entry &entry = _entries[row];
        const Catalog::Port &known = port(static_cast<int>(row));

        if (entry.checked > 0 && !known.repository.empty()) {
            answers.push_back({
                    .program = known.program,
                    .version = entry.version,
                    .url = entry.url,
                    .asset = entry.asset,
                    .verdict = static_cast<std::uint8_t>(entry.verdict),
                    .note = entry.note,
                    .checked = entry.checked,
                    .size = entry.size,
                    .sha256 = entry.sha256,
            });
        }
    }

    Releases::write(answers);
}

void Engines::refresh(const bool everything) {
    if (_offline) {
        return;
    }

    std::vector<int> asking;

    for (size_t row = 0; row < _entries.size(); row++) {
        const Entry &entry = _entries[row];
        const Catalog::Port &known = port(static_cast<int>(row));

        if (entry.fetch || known.repository.empty() || Catalog::pattern(known).empty()) {
            continue;
        }

        // The page asks only what went stale. The button asks everything.
        if (everything || !Releases::fresh(entry.checked)) {
            asking.push_back(static_cast<int>(row));
        }
    }

    if (!asking.empty()) {
        _trouble.clear();
    }

    for (const int row : asking) {
        Entry &entry = _entries[static_cast<size_t>(row)];

        if (std::ranges::find(_queued, row) != _queued.end()) {
            continue;
        }

        _queued.push_back(row);

        if (entry.state != State::EngineState::Installed) {
            entry.state = State::EngineState::Checking;
        }
    }

    pump();
    push();
}

// GitHub answers only so many questions an hour from one address, and "Check again"
// asks about every catalogued port at once: a few at a time is kinder to it and does
// not spike fourteen threads.
void Engines::pump() {
    auto busy = static_cast<size_t>(std::ranges::count_if(_entries, [](const Entry &each) { return each.asking; }));

    while (busy < ASKING && !_queued.empty()) {
        const int row = _queued.front();

        _queued.erase(_queued.begin());
        check(row);

        ++busy;
    }
}

void Engines::check(const int row) {
    const Catalog::Port &known = port(row);
    Entry &entry = _entries[static_cast<size_t>(row)];

    if (known.repository.empty() || entry.fetch || _offline) {
        return;
    }

    entry.asking = true;
    entry.fetch =
            std::make_unique<Http::Fetch>("https://api.github.com/repos/" + text(known.repository) + "/releases/latest",
                                          "application/vnd.github+json",
                                          std::filesystem::path());

    if (entry.state != State::EngineState::Installed) {
        entry.state = State::EngineState::Checking;
    }

    if (_ticker == 0) {
        _ticker = _clock->every(TICK, [this] { sweep(); });
    }

    push();
}

void Engines::install(const int row) {
    if (row < 0 || std::cmp_greater_equal(row, _entries.size())) {
        return;
    }

    Entry &entry = _entries[static_cast<size_t>(row)];

    if (entry.fetch || entry.state == State::EngineState::Unpacking) {
        return;
    }

    if (entry.url.empty()) {
        entry.wanted = true;

        check(row);

        return;
    }

    fetch(row);
}

void Engines::fetch(const int row) {
    const Catalog::Port &known = port(row);
    Entry &entry = _entries[static_cast<size_t>(row)];
    const std::filesystem::path shelf = Catalog::downloads();

    if (!Install::secure(entry.url)) {
        give(row, State::EngineState::Failed, "The download is not offered over https");

        return;
    }

    if (shelf.empty()) {
        give(row, State::EngineState::Failed, "There is nowhere to put it");

        return;
    }

    std::error_code code;

    std::filesystem::create_directories(shelf, code);

    if (code) {
        give(row, State::EngineState::Failed, "Could not make a directory for it: " + code.message());

        return;
    }

    entry.into = shelf / Install::downloadName(known, entry.asset);
    entry.partial = entry.into;
    entry.partial += ".part";

    // Already downloaded. Held to the promise before it is used, as a fresh one is.
    if (std::error_code asked; std::filesystem::is_regular_file(entry.into, asked)) {
        unpack(row, false);

        return;
    }

    if (_offline) {
        return;
    }

    entry.progress = 0;
    entry.state = State::EngineState::Fetching;
    entry.asking = false;
    entry.fetch = std::make_unique<Http::Fetch>(entry.url, std::string(), entry.partial);

    if (_ticker == 0) {
        _ticker = _clock->every(TICK, [this] { sweep(); });
    }

    push();
}

void Engines::cancel(const int row) const {
    if (row < 0 || std::cmp_greater_equal(row, _entries.size())) {
        return;
    }

    if (const Entry &entry = _entries[static_cast<size_t>(row)]; entry.fetch) {
        entry.fetch->cancel();
    }
}

void Engines::sweep() {
    bool moved = false;
    bool stamped = false;

    for (size_t row = 0; row < _entries.size(); row++) {
        Entry &entry = _entries[row];

        if (entry.unpacking) {
            if (!entry.unpacking->done.load()) {
                continue;
            }

            unpacked(static_cast<int>(row));
            moved = true;

            continue;
        }

        if (!entry.fetch) {
            continue;
        }

        if (!entry.fetch->done()) {
            if (!entry.asking) {
                entry.progress = entry.fetch->progress();
                moved = true;
            }

            continue;
        }

        // Taken out first, since the handling below may start another fetch.
        const std::unique_ptr<Http::Fetch> answered = std::exchange(entry.fetch, nullptr);
        const std::string trouble = answered->error();
        const int status = answered->status();

        moved = true;

        if (entry.asking) {
            const bool held = entry.state == State::EngineState::Installed;
            const bool wanted = std::exchange(entry.wanted, false);

            entry.asking = false;

            if (!trouble.empty()) {
                const bool limited = status == 403 || status == 429;

                _trouble = limited ? "GitHub is not answering any more questions from here just now. "
                                     "Its limit lifts within the hour."
                                   : trouble;

                // Any HTTP answer is cached, so a live rate limit is not walked into
                // again. A network failure is not.
                if (status > 0) {
                    entry.checked = Releases::now();
                    entry.verdict = State::EngineState::Failed;
                    entry.note = _trouble;
                    stamped = true;
                }

                give(static_cast<int>(row),
                     held ? State::EngineState::Installed : State::EngineState::Failed,
                     _trouble);

                continue;
            }

            const Install::Release release = Install::parseRelease(answered->body(), port(static_cast<int>(row)));

            entry.version = release.version;
            entry.url = release.url;
            entry.asset = release.asset;
            entry.size = release.size;
            entry.sha256 = release.sha256;

            entry.checked = Releases::now();
            stamped = true;

            if (entry.url.empty()) {
                entry.verdict = State::EngineState::Unavailable;
                entry.note = "The latest release has no build for this system";

                give(static_cast<int>(row),
                     held ? State::EngineState::Installed : State::EngineState::Unavailable,
                     entry.note);

                continue;
            }

            entry.verdict = State::EngineState::Waiting;
            entry.note.clear();

            give(static_cast<int>(row), held ? State::EngineState::Installed : State::EngineState::Ready);

            if (wanted) {
                fetch(static_cast<int>(row));
            }

            continue;
        }

        std::error_code code;

        if (answered->cancelled()) {
            std::filesystem::remove(entry.partial, code);
            settle(static_cast<int>(row));
            push();

            continue;
        }

        if (!trouble.empty()) {
            std::filesystem::remove(entry.partial, code);
            give(static_cast<int>(row), State::EngineState::Failed, trouble);
            _notifier->error(trouble, "Could not fetch it");

            continue;
        }

        unpack(static_cast<int>(row), true);
    }

    if (stamped) {
        writeCache();
    }

    pump();

    const bool busy = !_queued.empty() || std::ranges::any_of(_entries, [](const Entry &each) {
        return each.fetch != nullptr || each.unpacking != nullptr;
    });

    if (!busy) {
        _clock->cancel(_ticker);

        _ticker = 0;
    }

    if (moved) {
        push();
    }
}

void Engines::unpack(const int row, const bool fresh) {
    const Catalog::Port &known = port(row);

    give(row, State::EngineState::Unpacking);

    Entry &entry = _entries[static_cast<size_t>(row)];

    entry.unpacking = std::make_unique<Unpacking>();
    entry.unpacking->name = entry.into.filename().string();
    entry.unpacking->fresh = fresh;

    Unpacking *work = entry.unpacking.get();
    const std::filesystem::path from = fresh ? entry.partial : entry.into;
    const std::filesystem::path into = entry.into;
    const Install::Promise promise{.size = entry.size, .sha256 = entry.sha256};

    work->worker = std::thread([work, from, into, promise, &known] {
        work->answer = Install::accept(from, into, promise, known);
        work->done.store(true);
    });

    if (_ticker == 0) {
        _ticker = _clock->every(TICK, [this] { sweep(); });
    }
}

void Engines::unpacked(const int row) {
    Entry &entry = _entries[static_cast<size_t>(row)];

    const std::unique_ptr<Unpacking> work = std::exchange(entry.unpacking, nullptr);
    const Catalog::Port &known = port(row);

    if (work->fresh || work->answer.rejected) {
        measure();
    }

    // The shelf copy is not what the release promises any more: most likely the
    // release was republished under the same asset name since it was fetched.
    if (work->answer.rejected && !work->fresh) {
        settle(row);
        fetch(row);

        return;
    }

    if (!work->answer.trouble.empty()) {
        // What just came down is not what was asked about. The next install asks again.
        if (work->answer.rejected && !known.repository.empty()) {
            entry.url.clear();
            entry.checked = 0;
            writeCache();
        }

        give(row, State::EngineState::Failed, work->answer.trouble);

        if (work->answer.rejected) {
            _notifier->error(work->answer.trouble, "Could not fetch it");
        } else if (!work->answer.headline.empty()) {
            _notifier->error(work->answer.trouble, work->answer.headline);
        }

        return;
    }

    if (work->answer.program.empty()) {
        const std::string said =
                "Nothing in " + work->name + " is named " + text(known.executable) + ". It is kept in the downloads.";

        give(row, State::EngineState::Failed, said);
        _notifier->error(said, "Could not set " + text(Catalog::name(known)) + " up");

        return;
    }

    adopt(row, Format::from_path(work->answer.program));
}

void Engines::adopt(const int row, const std::string &file) {
    Entry &entry = _entries[static_cast<size_t>(row)];
    const Catalog::Port &known = port(row);
    const std::string name = text(Catalog::name(known));
    const std::string root = Format::from_path(Catalog::directory(known));
    const std::string before = entry.file;

    entry.file = file;
    entry.have = entry.version;
    entry.progress = 0;

    Install::stamp(known, entry.version);

    // A build named after its version leaves the old one behind.
    if (before != file && inside(root, before)) {
        std::error_code code;
        std::filesystem::remove(before, code);
    }

    repoint(row);
    give(row, State::EngineState::Installed);

    _notifier->success(entry.version.empty() ? name + " is ready to use"
                                             : name + " " + entry.version + " is ready to use",
                       "Fetched");
}

int Engines::rowOf(const int row) {
    const Catalog::Port &known = port(row);
    const std::vector<PortEntry> &ports = Session::get().config().ports;

    for (size_t each = 0; each < ports.size(); each++) {
        if (keptFor(ports[each], known)) {
            return static_cast<int>(each);
        }
    }

    return -1;
}

bool Engines::enlist(const int row) const {
    const Entry &entry = _entries[static_cast<size_t>(row)];
    const Catalog::Port &known = port(row);

    if (entry.file.empty() || Catalog::directory(known).empty() || rowOf(row) >= 0) {
        return false;
    }

    if (addPort) {
        (void)addPort({
                .name = text(Catalog::name(known)),
                .file = entry.file,
                .dosbox = known.dos,
                .program = known.program,
                .managed = true,
        });
    }

    return true;
}

void Engines::repoint(const int row) const {
    const Entry &entry = _entries[static_cast<size_t>(row)];
    const int at = rowOf(row);

    if (at < 0) {
        (void)enlist(row);

        return;
    }

    const std::vector<PortEntry> &held = Session::get().config().ports;
    const PortEntry &listed = held[static_cast<size_t>(at)];

    if (Format::from_path(listed.file) == entry.file) {
        return;
    }

    if (updatePort) {
        updatePort(at, listed.name, entry.file, port(row).dos);
    }
}

void Engines::repointMissing(const int row) const {
    const Entry &entry = _entries[static_cast<size_t>(row)];
    const int at = rowOf(row);

    if (at < 0 || entry.file.empty() || !updatePort) {
        return;
    }

    const PortEntry &listed = Session::get().config().ports[static_cast<size_t>(at)];
    std::error_code code;

    if (listed.file == entry.file || std::filesystem::exists(std::filesystem::path(listed.file), code)) {
        return;
    }

    updatePort(at, listed.name, entry.file, listed.dosbox);
}

void Engines::relist() const {
    int added = 0;

    for (size_t row = 0; row < _entries.size(); row++) {
        if (enlist(static_cast<int>(row))) {
            added++;
        } else {
            repointMissing(static_cast<int>(row));
        }
    }

    if (added == 0) {
        return;
    }

    _notifier->info(added == 1
                            ? "A source port ZDL4 had fetched was missing from this config."
                            : std::to_string(added) + " source ports ZDL4 had fetched were missing from this config.",
                    "Put back in the list");
}

void Engines::discover() {
    std::vector<Program::Program> &offered = Session::get().config().general.detected;
    const std::vector<PortEntry> &listed = Session::get().config().ports;
    const size_t before = offered.size();
    std::string only;
    int added = 0;

    for (const Detect::Found &found : Detect::ports()) {
        if (std::ranges::find(offered, found.port) != offered.end()) {
            continue;
        }

        offered.push_back(found.port);

        if (std::ranges::any_of(listed, [&found](const PortEntry &entry) {
                return Detect::same(std::filesystem::path(entry.file), found.program);
            })) {
            continue;
        }

        if (addPort) {
            only = addPort({.name = found.name, .file = Format::from_path(found.program), .dosbox = found.dos});
        }
        added++;
    }

    if (offered.size() != before) {
        if (scheduleSave) {
            scheduleSave();
        }
    }

    if (added == 0) {
        return;
    }

    _notifier->info(added == 1 ? only + " is on this machine already and is in the list now."
                               : std::to_string(added) + " source ports on this machine are in the list now.",
                    added == 1 ? "Found a source port" : "Found source ports");

    push();
}

void Engines::erase(const int row) {
    const Catalog::Port &known = port(row);
    const std::filesystem::path where = Catalog::directory(known);

    cancel(row);

    // Only ever the directory ZDL unpacked into.
    if (!where.empty()) {
        std::error_code code;

        std::filesystem::remove_all(where, code);
    }

    // Nothing is unpacked for it any more, so no row is its copy.
    for (PortEntry &held : Session::get().config().ports) {
        if (keptFor(held, known)) {
            held.managed = false;

            if (scheduleSave) {
                scheduleSave();
            }
        }
    }

    _entries[static_cast<size_t>(row)].file.clear();

    settle(row);
    push();
}

void Engines::remove(const int row) {
    if (row < 0 || std::cmp_greater_equal(row, _entries.size())) {
        return;
    }

    // Taken before erase clears the flag that finds it.
    const int at = rowOf(row);

    erase(row);

    if (at >= 0 && removePort) {
        removePort(at);
    }
}

void Engines::forget(const int listed) {
    const std::vector<PortEntry> &ports = Session::get().config().ports;

    if (listed < 0 || std::cmp_greater_equal(listed, ports.size())) {
        return;
    }

    // A copy, since erase clears the flag on the row.
    const PortEntry entry = ports[static_cast<size_t>(listed)];

    if (entry.managed) {
        for (size_t row = 0; row < _entries.size(); row++) {
            if (keptFor(entry, port(static_cast<int>(row)))) {
                erase(static_cast<int>(row));

                break;
            }
        }
    }

    if (removePort) {
        removePort(listed);
    }
}

void Engines::measure() {
    const long long held = Install::shelfBytes();

    if (held != _cached) {
        _cached = held;

        push();
    }
}

void Engines::clearDownloads() {
    Install::clearShelf();
    measure();

    _notifier->info("Anything fetched again comes down the wire afresh", "The downloads are empty");
}

void Engines::push() {
    State::PortsState &state = State::get().ports;
    std::vector<State::EngineRow> rows;

    rows.reserve(_entries.size());

    for (size_t row = 0; row < _entries.size(); row++) {
        const Entry &entry = _entries[row];
        const Catalog::Port &known = port(static_cast<int>(row));

        rows.push_back(State::EngineRow{
                .index = static_cast<int>(row),
                .name = std::string(Catalog::name(known)),
                .blurb = std::string(known.blurb),
                .homepage = std::string(known.homepage),
                .status = entry.state,
                .version = entry.version,
                .have = entry.have,
                .sizeText = megabytes(entry.size),
                .progress = static_cast<float>(entry.progress),
                .file = entry.file,
                .error = entry.error,
                .dos = known.dos,
                .asking = entry.asking,
        });
    }

    state.rows = std::move(rows);

    state.checking = std::ranges::any_of(_entries, [](const Entry &entry) { return entry.asking; });

    state.trouble = _trouble;
    state.cached = _cached > 0;
    state.cachedText = measured(_cached);

    State::get().touch();
}
