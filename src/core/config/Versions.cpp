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
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ttk/system/Text.h"

#include "core/config/Schema.h"
#include "core/config/Versions.h"

namespace {

constexpr const char *MULTIPLAYER_DMFLAGS = "dmflags";
constexpr const char *MULTIPLAYER_DMFLAGS2 = "dmflags2";
constexpr const char *REPLAY_COMPATIBILITY = "compatibility";
constexpr const char *PORT_DIALECT = "dialect";
constexpr const char *PORT_ID = "portId";
constexpr const char *GENERAL_DETECTED = "detected";

struct Renamed {
    std::string_view dialect;
    std::string_view program;
};

// 4.5.0 stored these dialects by a word that is not the id of the port each is named for.
constexpr std::array DIALECT_PROGRAMS = std::to_array<Renamed>({
        {.dialect = "dsda", .program = "dsda-doom"},
        {.dialect = "prboom", .program = "prboom-plus"},
        {.dialect = "chocolate", .program = "chocolate-doom"},
        {.dialect = "legacy", .program = "doomlegacy"},
});

template <typename Visit>
void eachProfile(yyjson_mut_val *root, Visit &&visit) {
    const yyjson_mut_val *profiles = yyjson_mut_obj_get(root, ConfigKey::PROFILES);
    std::size_t idx = 0;
    std::size_t max = 0;
    yyjson_mut_val *profile = nullptr;

    yyjson_mut_arr_foreach(profiles, idx, max, profile) {
        if (yyjson_mut_is_obj(profile)) {
            visit(profile);
        }
    }
}

bool has(const yyjson_mut_val *obj, const char *key) {
    return yyjson_mut_obj_get(obj, key) != nullptr;
}

// A file from before the key existed comes from a release that never started DOSBox fullscreen.
void dosWindowed(yyjson_mut_doc *doc, yyjson_mut_val *root) {
    eachProfile(root, [doc](yyjson_mut_val *profile) {
        if (!has(profile, ProfileKey::DOS_FULLSCREEN)) {
            yyjson_mut_obj_add_false(doc, profile, ProfileKey::DOS_FULLSCREEN);
        }
    });
}

// A file stamped 4.5.0 may already hold the new places, so a value moves only where the new place is empty.
void gameplayMoves(yyjson_mut_doc *doc, yyjson_mut_val *root) {
    eachProfile(root, [doc](yyjson_mut_val *profile) {
        yyjson_mut_val *multiplayer = yyjson_mut_obj_get(profile, ProfileKey::MULTIPLAYER);
        yyjson_mut_val *dmflags = yyjson_mut_obj_remove_key(multiplayer, MULTIPLAYER_DMFLAGS);
        yyjson_mut_val *dmflags2 = yyjson_mut_obj_remove_key(multiplayer, MULTIPLAYER_DMFLAGS2);
        yyjson_mut_val *replay = yyjson_mut_obj_get(profile, ProfileKey::REPLAY);
        yyjson_mut_val *compatibility = yyjson_mut_obj_remove_key(replay, REPLAY_COMPATIBILITY);

        if (!has(profile, ProfileKey::FLAGS)) {
            yyjson_mut_val *flags = yyjson_mut_obj_add_obj(doc, profile, ProfileKey::FLAGS);

            yyjson_mut_obj_add_val(doc, flags, FlagWord::DMFLAGS, dmflags);
            yyjson_mut_obj_add_val(doc, flags, FlagWord::DMFLAGS2, dmflags2);
        }

        if (!has(profile, ProfileKey::COMPLEVEL)) {
            yyjson_mut_obj_add_val(doc, profile, ProfileKey::COMPLEVEL, compatibility);
        }
    });
}

// 4.5.0 stored the dialect picked for a port. A file stamped 4.5.0 may already hold the program.
void pickedPrograms(yyjson_mut_doc *doc, const yyjson_mut_val *root) {
    const yyjson_mut_val *ports = yyjson_mut_obj_get(root, ConfigKey::PORTS);
    std::size_t idx = 0;
    std::size_t max = 0;
    yyjson_mut_val *port = nullptr;

    yyjson_mut_arr_foreach(ports, idx, max, port) {
        const yyjson_mut_val *dialect = yyjson_mut_obj_remove_key(port, PORT_DIALECT);

        if (!yyjson_mut_is_str(dialect) || has(port, ConfigKey::PROGRAM)) {
            continue;
        }

        const std::string_view word(yyjson_mut_get_str(dialect), yyjson_mut_get_len(dialect));
        // NOLINTNEXTLINE(readability-qualified-auto): the MSVC array iterator is a class, not a pointer.
        const auto renamed = std::ranges::find(DIALECT_PROGRAMS, word, &Renamed::dialect);
        const std::string_view program = renamed == DIALECT_PROGRAMS.end() ? word : renamed->program;

        yyjson_mut_obj_add_strncpy(doc, port, ConfigKey::PROGRAM, program.data(), program.size());
    }
}

// From 4.4.2 an engine ZDL4 downloaded carried the catalog id, which is the id of the port.
void managedPorts(yyjson_mut_doc *doc, const yyjson_mut_val *root) {
    const yyjson_mut_val *ports = yyjson_mut_obj_get(root, ConfigKey::PORTS);
    std::size_t idx = 0;
    std::size_t max = 0;
    yyjson_mut_val *port = nullptr;

    yyjson_mut_arr_foreach(ports, idx, max, port) {
        const yyjson_mut_val *id = yyjson_mut_obj_remove_key(port, PORT_ID);

        if (!yyjson_mut_is_str(id) || yyjson_mut_get_len(id) == 0) {
            continue;
        }

        // A downloaded engine ran as the catalog port in 4.5.0, whatever was picked.
        yyjson_mut_obj_remove_key(port, ConfigKey::PROGRAM);
        yyjson_mut_obj_add_strncpy(doc, port, ConfigKey::PROGRAM, yyjson_mut_get_str(id), yyjson_mut_get_len(id));

        yyjson_mut_obj_remove_key(port, ConfigKey::MANAGED);
        yyjson_mut_obj_add_true(doc, port, ConfigKey::MANAGED);
    }
}

// Ports that detection offered. The list names no engine, so nothing of it is kept.
void offeredPorts(const yyjson_mut_val *root) {
    yyjson_mut_obj_remove_key(yyjson_mut_obj_get(root, ConfigKey::GENERAL), GENERAL_DETECTED);
}

void toNine(yyjson_mut_doc *doc, yyjson_mut_val *root) {
    gameplayMoves(doc, root);
    pickedPrograms(doc, root);
    managedPorts(doc, root);
    offeredPorts(root);
}

struct Step {
    int to;

    // The first release that wrote this format. Empty when every file of this format states its version.
    std::string_view app;

    // Null when a file of the version before needs no change.
    void (*apply)(yyjson_mut_doc *doc, yyjson_mut_val *root);
};

constexpr std::array STEPS = std::to_array<Step>({
        {2, "4.1.0", dosWindowed},
        {3, "4.2.0", nullptr},
        {4, "4.3.0", nullptr},
        {5, "4.4.0", nullptr},
        {6, "4.4.2", nullptr},
        {7, "4.4.7", nullptr},
        {8, "4.5.0", nullptr},
        {9, {}, toNine},
});

static_assert(STEPS.front().to == 2 && STEPS.back().to == Versions::CURRENT,
              "The rows run from version 2 to Versions::CURRENT. A new row needs CURRENT raised to its version.");
static_assert(std::ranges::adjacent_find(STEPS, [](const Step &a, const Step &b) { return b.to != a.to + 1; })
                      == STEPS.end(),
              "Each row is one version above the row before it. Give a new row the next version.");

using Release = std::array<int, 3>;

// Up to three numbers joined by dots, a missing one read as 0. Empty for any other text.
std::optional<Release> releaseOf(const std::string_view text) {
    const std::vector<std::string> parts = ttk::Text::split(text, '.');
    Release release{};

    if (parts.empty() || parts.size() > release.size()) {
        return std::nullopt;
    }

    for (std::size_t at = 0; at < parts.size(); ++at) {
        release[at] = ttk::Text::to_int(parts[at], -1);

        if (release[at] < 0) {
            return std::nullopt;
        }
    }

    return release;
}

// A JSON integer from 1 up that fits an int. 0 for anything else.
int usableNumber(const yyjson_val *stated) {
    if (yyjson_is_uint(stated)) {
        const std::uint64_t number = yyjson_get_uint(stated);

        return number >= 1 && std::in_range<int>(number) ? static_cast<int>(number) : 0;
    }

    if (yyjson_is_sint(stated)) {
        const std::int64_t number = yyjson_get_sint(stated);

        return number >= 1 && std::in_range<int>(number) ? static_cast<int>(number) : 0;
    }

    return 0;
}

}

namespace Versions {

int of(const yyjson_val *root) {
    const int stated = usableNumber(yyjson_obj_get(root, ConfigKey::VERSION));

    if (stated > 1) {
        return stated;
    }

    const std::string app = ttk::Json::obj_get_string(root, ConfigKey::APP_VERSION);

    if (stated == 0 && app.empty()) {
        return CURRENT;
    }

    const std::optional<Release> written = releaseOf(app);

    if (!written) {
        return 1;
    }

    int version = 1;

    for (const Step &step : STEPS) {
        if (const std::optional<Release> first = releaseOf(step.app); first && *first <= *written) {
            version = step.to;
        }
    }

    return version;
}

ttk::Json::Doc upgraded(const ttk::Json::Doc &doc, const int from) {
    yyjson_mut_doc *copy = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *root = yyjson_val_mut_copy(copy, doc.root());

    yyjson_mut_doc_set_root(copy, root);

    for (const Step &step : STEPS) {
        if (step.to > from && step.apply != nullptr) {
            step.apply(copy, root);
        }
    }

    // Through text, since yyjson copies a mutable document into an immutable one by recursion. The writer and the
    // reader keep their stack on the heap, so a deeply nested file cannot overflow the call stack.
    std::size_t length = 0;
    char *text = yyjson_mut_write(copy, 0, &length);

    yyjson_mut_doc_free(copy);

    ttk::Json::Doc out(text == nullptr ? nullptr : yyjson_read(text, length, 0));

    std::free(text);

    return out;
}

}
