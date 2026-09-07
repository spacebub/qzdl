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
#include <fstream>
#include <utility>

#include "core/Archive.h"
#include "core/Json.h"
#include "core/Text.h"
#include "gui/Engines.h"
#include "gui/Convert.h"

namespace {

// Written beside what was fetched, so a card can say what is already here.
constexpr auto STAMP = "/.zdl-version";

// The version out of a release tag: "woof_15.3.0", "g4.14.2", "v0.29.4".
std::string tidy(const std::string &tag) {
    for (size_t at = 0; at < tag.size(); at++) {
        if (std::isdigit(static_cast<unsigned char>(tag[at])) != 0) {
            return tag.substr(at);
        }
    }

    return tag;
}

// Named after the port, whatever the other end calls the download.
std::string safeName(const std::string &name, const std::string &fallback) {
    std::string out;

    for (const char each : name) {
        if (std::isalnum(static_cast<unsigned char>(each)) != 0
            || each == '.' || each == '-' || each == '_') {
            out.push_back(each);
        }
    }

    return out.empty() || out.starts_with(".") ? fallback : out;
}

std::string fileNameOf(const std::string &url) {
    const size_t cut = url.find_last_of('/');
    const std::string last = cut == std::string::npos ? url : url.substr(cut + 1);
    const size_t query = last.find_first_of("?#");

    return query == std::string::npos ? last : last.substr(0, query);
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

    return bytes > 0
        ? std::to_string(std::max<long long>(1, (bytes + 512) / 1024)) + " KB"
        : "Nothing";
}

std::string text(const std::string_view value) {
    return {value.begin(), value.end()};
}

// The whole of what happens off the interface's thread: nothing here touches
// anything the window holds.
Engines::Placed place(const std::filesystem::path &archive, const std::filesystem::path &where,
                     const std::string &name, const std::string &program, const bool dos) {
    Engines::Placed out;
    std::error_code code;

    std::filesystem::create_directories(where, code);

    if (code) {
        out.trouble = "Could not make a directory for it: " + code.message();

        return out;
    }

    // Anything but a zip -- an AppImage, a bare program -- already is the thing
    // that runs, and is copied across as it is.
    if (Text::iendsWith(name, ".zip")) {
        if (!Archive::extract(archive, where, &out.trouble)) {
            out.headline = "Could not unpack " + name;

            return out;
        }

        out.program = Catalog::program(where, program, dos);
    } else {
        const std::filesystem::path target = where / name;

        std::filesystem::remove(target, code);
        std::filesystem::copy_file(archive, target,
                                   std::filesystem::copy_options::overwrite_existing, code);

        if (code) {
            out.trouble = "Could not put it in place";

            return out;
        }

        out.program = target;
    }

    // A zip packed on Windows carries no permission bits.
    if (!out.program.empty()) {
        std::filesystem::permissions(out.program,
                                     std::filesystem::perms::owner_exec
                                     | std::filesystem::perms::group_exec
                                     | std::filesystem::perms::others_exec,
                                     std::filesystem::perm_options::add, code);
    }

    return out;
}

}

Engines::Engines(const ui::Zdl *window, Notifier *notifier, ConfigBridge *config)
    : _window(window), _notifier(notifier), _config(config),
      _entries(Catalog::ports().size()) {
    const auto &state = _window->global<ui::Ports>();

    state.set_rows(_rows);
    state.set_directory(Convert::fromPath(Catalog::directory()));
    state.set_downloads(Convert::fromPath(Catalog::downloads()));

    state.on_refresh([this](const bool everything) { refresh(everything); });
    state.on_install([this](const int row) { install(row); });
    state.on_cancel([this](const int row) { cancel(row); });
    state.on_remove([this](const int row) { remove(row); });
    state.on_forget([this](const int listed) { forget(listed); });
    state.on_measure([this] { measure(); });
    state.on_clear_downloads([this] { clearDownloads(); });

    for (size_t row = 0; row < _entries.size(); row++) {
        settle(static_cast<int>(row));
    }

    relist();
    measure();
    push();
}

const Catalog::Port &Engines::port(const int row) {
    return Catalog::ports()[static_cast<size_t>(row)];
}

void Engines::give(const int row, const std::string &state, const std::string &error) {
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

    // A page to get it from and no build here to point at.
    if (known.repository.empty() && known.file.empty()) {
        entry.state = "elsewhere";

        return;
    }

    if (Catalog::pattern(known).empty()) {
        entry.state = "unavailable";
        entry.error = "There is no build of this one for this system";

        return;
    }

    // A build that never moves is known without asking.
    if (!known.file.empty()) {
        entry.url = text(known.file);
        entry.asset = fileNameOf(entry.url);
        entry.version = text(known.version);
    }

    const std::filesystem::path where = Catalog::directory(known);
    const std::filesystem::path found = where.empty()
        ? std::filesystem::path()
        : Catalog::program(where, known.program, known.dos);

    if (!found.empty()) {
        if (std::ifstream stamp(where.string() + STAMP); stamp) {
            std::getline(stamp, entry.have);
            entry.have = Text::trim(entry.have);
        }

        entry.file = Convert::plain(Convert::fromPath(found));
        entry.state = "installed";

        return;
    }

    entry.state = entry.url.empty() ? "waiting" : "ready";
}

void Engines::refresh(const bool everything) {
    _trouble.clear();

    for (size_t row = 0; row < _entries.size(); row++) {
        const Entry &entry = _entries[row];

        if (entry.fetch || port(static_cast<int>(row)).repository.empty()
            || Catalog::pattern(port(static_cast<int>(row))).empty()) {
            continue;
        }

        // One already fetched is asked too, so the card can say what is newer.
        if (everything || entry.state == "waiting" || entry.state == "failed"
            || (entry.state == "installed" && entry.url.empty())) {
            check(static_cast<int>(row));
        }
    }

    push();
}

void Engines::check(const int row) {
    const Catalog::Port &known = port(row);
    Entry &entry = _entries[static_cast<size_t>(row)];

    if (known.repository.empty() || entry.fetch) {
        return;
    }

    entry.asking = true;
    entry.fetch = std::make_unique<Http::Fetch>(
        "https://api.github.com/repos/" + text(known.repository) + "/releases/latest",
        true, std::filesystem::path());

    if (entry.state != "installed") {
        entry.state = "checking";
    }

    _clock.start(slint::TimerMode::Repeated, TICK, [this] { sweep(); });

    push();
}

void Engines::install(const int row) {
    if (row < 0 || std::cmp_greater_equal(row, _entries.size())) {
        return;
    }

    Entry &entry = _entries[static_cast<size_t>(row)];

    if (entry.fetch || entry.state == "unpacking") {
        return;
    }

    // Nothing to fetch yet: ask for the release and carry on from the answer.
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

    if (shelf.empty()) {
        give(row, "failed", "There is nowhere to put it");

        return;
    }

    std::error_code code;

    std::filesystem::create_directories(shelf, code);

    if (code) {
        give(row, "failed", "Could not make a directory for it: " + code.message());

        return;
    }

    entry.into = shelf / safeName(entry.asset, text(known.id));

    // The same build fetched before and kept.
    if (std::error_code asked; std::filesystem::is_regular_file(entry.into, asked)
        && (entry.size <= 0
            || std::cmp_equal(std::filesystem::file_size(entry.into, asked), entry.size))) {
        unpack(row, entry.into);

        return;
    }

    entry.partial = entry.into;
    entry.partial += ".part";
    entry.progress = 0;
    entry.state = "fetching";
    entry.asking = false;
    entry.fetch = std::make_unique<Http::Fetch>(entry.url, false, entry.partial);

    _clock.start(slint::TimerMode::Repeated, TICK, [this] { sweep(); });

    push();
}

void Engines::cancel(const int row) {
    if (row < 0 || std::cmp_greater_equal(row, _entries.size())) {
        return;
    }

    if (const Entry &entry = _entries[static_cast<size_t>(row)]; entry.fetch) {
        entry.fetch->cancel();
    }
}

void Engines::sweep() {
    bool waiting = false;
    bool moved = false;

    for (size_t row = 0; row < _entries.size(); row++) {
        Entry &entry = _entries[row];

        if (entry.unpacking) {
            if (!entry.unpacking->done.load()) {
                waiting = true;

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
            waiting = true;

            if (!entry.asking) {
                entry.progress = entry.fetch->progress();
                moved = true;
            }

            continue;
        }

        // Held apart: what is done with it below can start another in its place.
        const std::unique_ptr<Http::Fetch> answered = std::exchange(entry.fetch, nullptr);
        const std::string trouble = answered->error();
        const int status = answered->status();

        moved = true;

        if (entry.asking) {
            const bool held = entry.state == "installed";
            const bool wanted = std::exchange(entry.wanted, false);

            if (!trouble.empty()) {
                // GitHub's hourly limit is the one failure worth naming; the rest
                // is the network being the network.
                _trouble = status == 403 || status == 429
                    ? "GitHub is not answering any more questions from here just now. "
                      "Its limit lifts within the hour."
                    : trouble;

                give(static_cast<int>(row), held ? "installed" : "failed", _trouble);

                continue;
            }

            const std::string body = answered->body();
            std::string parseTrouble;
            const Json::Doc release = Json::readData(body, &parseTrouble);
            const std::string_view pattern = Catalog::pattern(port(static_cast<int>(row)));

            entry.version = tidy(Json::objGetString(release.root(), "tag_name"));
            entry.url.clear();
            entry.asset.clear();
            entry.size = 0;

            if (yyjson_val *assets = Json::objGet(release.root(), "assets");
                assets != nullptr) {
                size_t index = 0;
                size_t count = 0;
                yyjson_val *asset = nullptr;

                yyjson_arr_foreach(assets, index, count, asset) {
                    const std::string name = Json::objGetString(asset, "name");

                    if (Catalog::matches(name, pattern)) {
                        entry.asset = name;
                        entry.url = Json::objGetString(asset, "browser_download_url");
                        entry.size = Json::objGetInt(asset, "size");

                        break;
                    }
                }
            }

            if (entry.url.empty()) {
                give(static_cast<int>(row), held ? "installed" : "unavailable",
                     "The latest release has no build for this system");

                continue;
            }

            give(static_cast<int>(row), held ? "installed" : "ready");

            if (wanted) {
                fetch(static_cast<int>(row));
                waiting = true;
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
            give(static_cast<int>(row), "failed", trouble);
            _notifier->error(trouble, "Could not fetch it");

            continue;
        }

        std::filesystem::remove(entry.into, code);
        std::filesystem::rename(entry.partial, entry.into, code);

        if (code) {
            std::filesystem::remove(entry.partial, code);
            give(static_cast<int>(row), "failed", "Could not keep the download");

            continue;
        }

        measure();
        unpack(static_cast<int>(row), entry.into);
    }

    if (!waiting) {
        _clock.stop();
    }

    if (moved) {
        push();
    }
}

void Engines::unpack(const int row, const std::filesystem::path &archive) {
    const Catalog::Port &known = port(row);
    const std::filesystem::path where = Catalog::directory(known);
    const std::string name = archive.filename().string();

    give(row, "unpacking");

    // Now rather than at the end of the sweep: the point of the thread is that
    // the window says what it is doing while it does it.
    push();

    Entry &entry = _entries[static_cast<size_t>(row)];

    entry.unpacking = std::make_unique<Unpacking>();
    entry.unpacking->name = name;

    Unpacking *work = entry.unpacking.get();
    const std::string program = text(known.program);
    const bool dos = known.dos;

    work->worker = std::thread([work, archive, where, name, program, dos] {
        work->answer = place(archive, where, name, program, dos);
        work->done.store(true);
    });

    _clock.start(slint::TimerMode::Repeated, TICK, [this] { sweep(); });
}

void Engines::unpacked(const int row) {
    Entry &entry = _entries[static_cast<size_t>(row)];

    // Joined as it goes out of scope, which it has already run to the end of.
    const std::unique_ptr<Unpacking> work = std::exchange(entry.unpacking, nullptr);
    const Catalog::Port &known = port(row);

    if (!work->answer.trouble.empty()) {
        give(row, "failed", work->answer.trouble);

        if (!work->answer.headline.empty()) {
            _notifier->error(work->answer.trouble, work->answer.headline);
        }

        return;
    }

    if (work->answer.program.empty()) {
        const std::string said = "Nothing in " + work->name + " is named " + text(known.program)
            + ". It is kept in the downloads.";

        give(row, "failed", said);
        _notifier->error(said, "Could not set " + text(known.name) + " up");

        return;
    }

    adopt(row, Convert::plain(Convert::fromPath(work->answer.program)));
}

void Engines::adopt(const int row, const std::string &file) {
    Entry &entry = _entries[static_cast<size_t>(row)];
    const Catalog::Port &known = port(row);
    const std::string name = text(known.name);
    const std::string root = Convert::plain(Convert::fromPath(Catalog::directory(known)));
    const std::string before = entry.file;

    entry.file = file;
    entry.have = entry.version;
    entry.progress = 0;

    if (std::ofstream stamp(Catalog::directory(known).string() + STAMP, std::ios::trunc);
        stamp) {
        stamp << entry.version;
    }

    std::error_code code;

    // A build named after its version does not overwrite the one it replaces.
    if (!before.empty() && before != file && before.starts_with(root + "/")) {
        std::filesystem::remove(before, code);
    }

    enlist(row, before);
    give(row, "installed");

    _notifier->success(entry.version.empty()
                           ? name + " is ready to use"
                           : name + " " + entry.version + " is ready to use",
                       "Fetched");
}

bool Engines::enlist(const int row, const std::string &before) {
    const Entry &entry = _entries[static_cast<size_t>(row)];
    const Catalog::Port &known = port(row);
    const std::string root = Convert::plain(Convert::fromPath(Catalog::directory(known)));

    if (entry.file.empty() || root.empty()) {
        return false;
    }

    // Fetched twice over is still one source port: the entry already pointing
    // inside its directory is moved to the new build, so profiles on it follow.
    const std::vector<NameEntry> &ports = ConfigBridge::ports();
    int at = -1;

    for (size_t each = 0; each < ports.size(); each++) {
        const std::string held = Convert::plain(Convert::fromPath(ports[each].file));

        if (held == entry.file) {
            return false;
        }

        if (at < 0 && (held == before || held.starts_with(root + "/"))) {
            at = static_cast<int>(each);
        }
    }

    if (at >= 0) {
        _config->updatePort(at, ports[static_cast<size_t>(at)].name, entry.file, known.dos);

        return false;
    }

    _config->addPort(entry.file, text(known.name), known.dos);

    return true;
}

void Engines::relist() {
    int added = 0;

    for (size_t row = 0; row < _entries.size(); row++) {
        if (enlist(static_cast<int>(row))) {
            added++;
        }
    }

    if (added == 0) {
        return;
    }

    _notifier->info(added == 1
                        ? "A source port ZDL had fetched was missing from this config."
                        : std::to_string(added)
                          + " source ports ZDL had fetched were missing from this config.",
                    "Put back in the list");
}

void Engines::erase(const int row) {
    const std::filesystem::path where = Catalog::directory(port(row));

    cancel(row);

    // Only ever the directory ZDL unpacked into.
    if (!where.empty()) {
        std::error_code code;

        std::filesystem::remove_all(where, code);
    }

    _entries[static_cast<size_t>(row)].file.clear();

    settle(row);
    push();
}

void Engines::remove(const int row) {
    if (row < 0 || std::cmp_greater_equal(row, _entries.size())) {
        return;
    }

    const std::string root = Convert::plain(Convert::fromPath(Catalog::directory(port(row))));

    erase(row);

    for (int each = static_cast<int>(ConfigBridge::ports().size()) - 1; each >= 0; each--) {
        if (Convert::plain(Convert::fromPath(ConfigBridge::ports()[static_cast<size_t>(each)].file))
                .starts_with(root + "/")) {
            _config->removePort(each);
        }
    }
}

void Engines::forget(const int listed) {
    const std::vector<NameEntry> &ports = ConfigBridge::ports();

    if (listed < 0 || std::cmp_greater_equal(listed, ports.size())) {
        return;
    }

    const std::string file =
        Convert::plain(Convert::fromPath(ports[static_cast<size_t>(listed)].file));

    for (size_t row = 0; row < _entries.size(); row++) {
        const std::string root = Convert::plain(
            Convert::fromPath(Catalog::directory(port(static_cast<int>(row)))));

        if (!root.empty() && file.starts_with(root + "/")) {
            erase(static_cast<int>(row));

            break;
        }
    }

    _config->removePort(listed);
}

void Engines::measure() {
    const std::filesystem::path shelf = Catalog::downloads();
    std::error_code code;
    long long held = 0;

    for (std::filesystem::directory_iterator walk(shelf, code), end;
         walk != end && !code; walk.increment(code)) {
        std::error_code asked;

        if (walk->is_regular_file(asked)) {
            held += static_cast<long long>(walk->file_size(asked));
        }
    }

    if (held != _cached) {
        _cached = held;

        push();
    }
}

void Engines::clearDownloads() {
    const std::filesystem::path shelf = Catalog::downloads();
    std::error_code code;

    for (std::filesystem::directory_iterator walk(shelf, code), end;
         walk != end && !code; walk.increment(code)) {
        std::error_code asked;

        std::filesystem::remove_all(walk->path(), asked);
    }

    measure();

    _notifier->info("Anything fetched again comes down the wire afresh",
                    "The downloads are empty");
}

void Engines::push() {
    const auto &state = _window->global<ui::Ports>();
    std::vector<ui::EngineBrowseRow> rows;

    rows.reserve(_entries.size());

    for (size_t row = 0; row < _entries.size(); row++) {
        const Entry &entry = _entries[row];
        const Catalog::Port &known = port(static_cast<int>(row));

        rows.push_back(ui::EngineBrowseRow{
            .index = static_cast<int>(row),
            .name = Convert::text(known.name),
            .blurb = Convert::text(known.blurb),
            .homepage = Convert::text(known.homepage),
            .status = Convert::text(entry.state),
            .version = Convert::text(entry.version),
            .have = Convert::text(entry.have),
            .size_text = Convert::text(megabytes(entry.size)),
            .progress = static_cast<float>(entry.progress),
            .file = Convert::text(entry.file),
            .error = Convert::text(entry.error),
            .dos = known.dos,
        });
    }

    Models::reconcile(*_rows, rows);

    state.set_checking(std::ranges::any_of(_entries, [](const Entry &entry) {
        return entry.state == "checking";
    }));

    state.set_trouble(Convert::text(_trouble));
    state.set_cached(_cached > 0);
    state.set_cached_text(Convert::text(measured(_cached)));
}
