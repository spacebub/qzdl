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
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ttk/system/Text.h"

#include "core/config/Schema.h"
#include "core/config/Versions.h"

namespace {

constexpr const char *MULTIPLAYER_DMFLAGS = "dmflags";
constexpr const char *MULTIPLAYER_DMFLAGS2 = "dmflags2";
constexpr const char *REPLAY_COMPATIBILITY = "compatibility";

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

struct Step {
    int to;

    // The first release that wrote this shape. Empty for a shape no release wrote under the number 1.
    std::string_view app;

    // Null when a file of the shape before reads right as it is.
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
        {9, {}, gameplayMoves},
});

static_assert(STEPS.front().to == 2 && STEPS.back().to == Versions::CURRENT, "a row for every version after 1");
static_assert(std::ranges::adjacent_find(STEPS, [](const Step &a, const Step &b) { return b.to != a.to + 1; })
                      == STEPS.end(),
              "the rows count up by one");

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

}

namespace Versions {

int of(const yyjson_val *root) {
    const int stated = ttk::Json::obj_get_int(root, ConfigKey::VERSION, 1);

    if (stated > 1) {
        return stated;
    }

    const std::optional<Release> written = releaseOf(ttk::Json::obj_get_string(root, ConfigKey::APP_VERSION));

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

    ttk::Json::Doc out(yyjson_mut_doc_imut_copy(copy, nullptr));

    yyjson_mut_doc_free(copy);

    return out;
}

}
