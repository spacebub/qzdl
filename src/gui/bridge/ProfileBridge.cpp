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

#include <utility>

#include "core/config/Import.h"
#include "core/launch/Arguments.h"
#include "core/launch/Command.h"
#include "core/launch/Dialect.h"
#include "core/launch/Launcher.h"
#include "core/launch/Storage.h"
#include "core/util/Text.h"
#include "gui/bridge/ConfigBridge.h"
#include "gui/Convert.h"
#include "gui/Models.h"
#include "gui/bridge/ProfileBridge.h"

namespace {

std::string artKeyOf(const Profile &each, const NameEntry *game) {
    std::string key = game == nullptr ? std::string() : game->file;

    for (const FileEntry &file : each.files) {
        if (file.enabled) {
            key += '\n';
            key += file.file;
        }
    }

    return key;
}

bool dosPortOf(const Config &config, const Profile &profile) {
    const NameEntry *port = config.findPort(profile.port);

    return port != nullptr && port->dosbox;
}

std::string zdlFileName(const std::string &name) {
    static constexpr std::string_view FORBIDDEN = R"(/\:*?"<>|)";
    std::string stem;

    for (const char each : name) {
        stem.push_back(static_cast<unsigned char>(each) < 0x20 || FORBIDDEN.contains(each)
                       ? '-'
                       : each);
    }

    stem = Text::trim(stem);

    // Windows drops trailing dots and spaces.
    while (!stem.empty() && (stem.back() == '.' || stem.back() == ' ')) {
        stem.pop_back();
    }

    return (stem.empty() ? "profile" : stem) + ".zdl";
}

std::shared_ptr<slint::Model<ui::BadgeSpec>> badgesOf(const Config &config, const int index) {
    std::vector<ui::BadgeSpec> badges;

    if (index < 0 || std::cmp_greater_equal(index, config.profiles.size())) {
        return std::make_shared<slint::VectorModel<ui::BadgeSpec>>(std::move(badges));
    }

    const Profile &each = config.profiles[static_cast<size_t>(index)];
    const bool ready = !each.port.empty() || each.customCommand;
    int loaded = 0;

    for (const FileEntry &file : each.files) {
        if (file.enabled) {
            ++loaded;
        }
    }

    if (dosPortOf(config, each)) {
        badges.push_back(ui::BadgeSpec{.text = "DOS", .kind = "muted", .dot = true});
    }

    if (!ready) {
        badges.push_back(ui::BadgeSpec{.text = "No port", .kind = "warning", .dot = true});
    } else if (!each.files.empty()) {
        const size_t count = each.files.size();
        const std::string said = std::cmp_equal(loaded, count)
            ? std::to_string(count) + (count == 1 ? " file" : " files")
            : std::to_string(loaded) + " of " + std::to_string(count) + " loaded";

        badges.push_back(ui::BadgeSpec{
            .text = Convert::text(said),
            .kind = "muted",
            .dot = true,
        });
    }

    const Dialect::NetSupport net = Dialect::net(Dialect::of(config, each));

    if (const int role = ProfilePanels::netRoleOf(each.multiplayer);
        role != 0 && (role == 1 ? net.hosts : net.joins)) {
        badges.push_back(ui::BadgeSpec{
            .text = role == 1 ? "Hosting" : "Multiplayer",
            .kind = "muted",
            .dot = true,
        });
    }

    if (each.replay.mode != 0) {
        badges.push_back(ui::BadgeSpec{
            .text = each.replay.mode == 1 ? "Recording" : "Replay",
            .kind = "muted",
            .dot = true,
        });
    }

    return std::make_shared<slint::VectorModel<ui::BadgeSpec>>(std::move(badges));
}

}

ui::ProfileCard ProfileBridge::cardOf(const int index) {
    const Profile &each = config().profiles[static_cast<size_t>(index)];
    int loaded = 0;

    for (const FileEntry &file : each.files) {
        if (file.enabled) {
            ++loaded;
        }
    }

    const NameEntry *game = config().findIwad(each.iwad);

    return ui::ProfileCard{
        .index = index,
        .id = Convert::text(each.id),
        .key = Convert::text(ConfigBridge::profileKey(each.id)),
        .name = Convert::text(each.name.empty() ? "(unnamed)" : each.name),
        .iwad = Convert::text(each.iwad),
        .art_key = Convert::text(artKeyOf(each, game)),
        .port = Convert::text(each.port),
        .dos_port = dosPortOf(config(), each),
        .warp = Convert::text(each.warp),
        .files = static_cast<int>(each.files.size()),
        .loaded = loaded,
        .net_role = ProfilePanels::netRoleOf(each.multiplayer),
        .ready = !each.port.empty() || each.customCommand,
    };
}

void ProfileBridge::pushCards() {
    std::vector<ui::ProfileCard> cards;

    cards.reserve(config().profiles.size());

    for (size_t index = 0; index < config().profiles.size(); ++index) {
        cards.push_back(cardOf(static_cast<int>(index)));
    }

    Models::reconcile(*_profileCards, cards);

    _hub->bumpRev();
    _hub->library().pushShelf();
    _hub->scheduleSave();
}

void ProfileBridge::pushConfigDonors() {
    const Profile &profile = active();
    std::vector<ui::ConfigDonor> donors;

    if (!profile.port.empty()) {
        for (const Profile &other : config().profiles) {
            if (other.id == profile.id || !Text::iequals(other.port, profile.port)) {
                continue;
            }

            const std::filesystem::path file = Storage::configFile(other);
            std::error_code asked;

            if (file.empty() || !std::filesystem::is_regular_file(file, asked)) {
                continue;
            }

            donors.push_back(ui::ConfigDonor{
                .id = Convert::text(other.id),
                .name = Convert::text(other.name),
                .file = Convert::fromPath(file),
                .shared = other.sharedConfig,
            });
        }
    }

    Models::reconcile(*_configDonors, donors);
}

void ProfileBridge::push() {
    const ui::Cfg &state = cfg();
    const Profile &profile = active();

    state.set_profile_index(config().activeProfileIndex());
    state.set_profile_name(Convert::text(profile.name));
    state.set_profile_key(Convert::text(ConfigBridge::profileKey(config().activeProfileId)));
    state.set_iwad(Convert::text(profile.iwad));
    state.set_port(Convert::text(profile.port));
    state.set_skill(profile.skill);
    state.set_monsters(profile.monsters);
    state.set_warp(Convert::text(profile.warp));
    state.set_extra(Convert::text(profile.extra));
    state.set_multiplayer_open(profile.dialogOpen);
    state.set_shared_config(profile.sharedConfig);
    state.set_command_override(profile.customCommand);
    state.set_command(Convert::text(profile.command));
    state.set_dos_fullscreen(profile.dosFullscreen);
    state.set_capture_output(profile.captureOutput);
    state.set_config_file(Convert::fromPath(Storage::configFile(profile)));
    state.set_dos_port(Launcher::isDosPort(config()));
    _hub->bumpRev();

    _hub->panels().pushReplay();
    _hub->panels().pushSave();
    _hub->library().pushGameRev();
    _hub->scheduleSave();
}

// Opens every ticked file, so only redone when they changed.
void ProfileBridge::pushMaps() {
    const Profile &profile = active();
    const NameEntry *game = config().findIwad(profile.iwad);
    std::string mark = game == nullptr ? std::string() : game->file;

    for (const FileEntry &entry : profile.files) {
        if (entry.enabled) {
            mark += '\n';
            mark += entry.file;
        }
    }

    if (_mapsKnown && mark == _mapsMark) {
        return;
    }

    _maps = Arguments::maps(config());
    _mapsMark = std::move(mark);
    _mapsKnown = true;

    cfg().set_maps(Convert::strings(_maps));
}

// Building the line opens the game, so it is debounced.
void ProfileBridge::pushCommand() {
    _hub->bumpRev();
    _preview.start(slint::TimerMode::SingleShot, PREVIEW, [this] { showCommand(); });

    _hub->scheduleSave();
}

void ProfileBridge::showCommand() {
    _preview.stop();

    cfg().set_command_line(Convert::text(Command::line(config())));
    cfg().set_command_trouble(Convert::text(Command::trouble(config())));
}

void ProfileBridge::touch() {
    pushMaps();
    pushCommand();
    pushCards();
}

void ProfileBridge::bind() {
    const ui::Cfg &state = cfg();

    state.set_profile_cards(_profileCards);
    state.set_config_donors(_configDonors);

    state.on_profile_badges([](int, const int index) { return badgesOf(config(), index); });

    state.on_art_key([](int) {
        const Profile &profile = active();

        return Convert::text(artKeyOf(profile, config().findIwad(profile.iwad)));
    });

    state.on_zdl_file_name([] { return Convert::text(zdlFileName(active().name)); });

    state.on_set_profile_index([this](const int index) {
        const std::vector<Profile> &profiles = config().profiles;

        if (index < 0 || std::cmp_greater_equal(index, profiles.size())
            || profiles[static_cast<size_t>(index)].id == config().activeProfileId) {
            return;
        }

        config().setActiveProfile(profiles[static_cast<size_t>(index)].id);
        _hub->reload();
    });

    state.on_set_iwad([this](const slint::SharedString &value) {
        if (Convert::plain(value) == active().iwad) {
            return;
        }

        active().iwad = Convert::plain(value);

        push();
        touch();
    });

    state.on_set_port([this](const slint::SharedString &value) {
        if (Convert::plain(value) == active().port) {
            return;
        }

        active().port = Convert::plain(value);

        push();
        pushCommand();
    });

    state.on_set_skill([this](const int value) {
        active().skill = value;

        push();
        pushCommand();
    });

    state.on_set_monsters([this](const int value) {
        active().monsters = value;

        push();
        pushCommand();
    });

    state.on_set_warp([this](const slint::SharedString &value) {
        active().warp = Convert::plain(value);

        push();
        pushCommand();
    });

    state.on_set_extra([this](const slint::SharedString &value) {
        active().extra = Convert::plain(value);

        push();
        pushCommand();
    });

    state.on_set_shared_config([this](const bool value) {
        active().sharedConfig = value;

        push();
        pushCommand();
    });

    state.on_set_command_override([this](const bool value) {
        if (value && active().command.empty()) {
            active().command = Command::pattern(config());
        }

        active().customCommand = value;

        push();
        pushCommand();
        pushCards();
    });

    state.on_set_command([this](const slint::SharedString &value) {
        active().command = Convert::plain(value);

        push();
        pushCommand();
    });

    state.on_set_dos_fullscreen([this](const bool value) {
        active().dosFullscreen = value;

        push();
        pushCommand();
    });

    state.on_set_capture_output([this](const bool value) {
        active().captureOutput = value;

        push();
    });

    state.on_move_profile([this](const int from, const int to) {
        moveTo(config().profiles, from, to);

        pushCards();
        push();
    });

    state.on_add_profile([this](const slint::SharedString &name) {
        config().setActiveProfile(config().addProfile(Convert::plain(name)));
        _hub->reload();
    });

    state.on_duplicate_profile([this] {
        if (config().profiles.empty()) {
            return;
        }

        config().setActiveProfile(config().duplicateActiveProfile(active().name));
        _hub->reload();
    });

    state.on_refresh_config_donors([this] { pushConfigDonors(); });

    state.on_copy_engine_config([this](const slint::SharedString &id) {
        const int index = config().indexOfProfile(Convert::plain(id));

        if (index < 0) {
            return;
        }

        const Profile &source = config().profiles[static_cast<size_t>(index)];
        const Profile &profile = active();

        const std::filesystem::path taken = Storage::configFile(source);
        const std::filesystem::path here = Storage::configFile(profile);

        if (taken.empty() || here.empty() || taken == here) {
            return;
        }

        std::error_code code;

        std::filesystem::create_directories(here.parent_path(), code);

        if (!std::filesystem::copy_file(taken, here,
                                        std::filesystem::copy_options::overwrite_existing,
                                        code)) {
            _notifier->error("Could not copy " + source.name + "'s engine config: "
                             + code.message());

            return;
        }

        if (!config().general.profileConfigs || profile.sharedConfig) {
            _notifier->warning("Copied " + source.name + "'s engine config, but this profile "
                               "launches on the port's config, so nothing reads it yet.");

            return;
        }

        _notifier->success("This profile now starts on a copy of " + source.name
                           + "'s engine config.");
    });

    state.on_rename_profile([this](const slint::SharedString &name) {
        const std::string wanted = Convert::plain(name);

        if (wanted.empty()) {
            return;
        }

        // uniqueProfileName would turn an unchanged name into "name (2)".
        Profile &profile = active();

        profile.name = Text::iequals(profile.name, wanted)
            ? Text::trim(wanted)
            : config().uniqueProfileName(wanted);

        pushCards();
        push();
    });

    state.on_remove_profile([this] {
        config().removeProfile(config().activeProfileId);
        _hub->reload();
    });

    state.on_clear_profile([this] {
        active().clearSettings();
        _hub->reload();
    });

    state.on_load_zdl([this](const slint::SharedString &path) {
        Profile loaded;

        if (!Import::loadZdlFile(Convert::toPath(path), loaded)) {
            _notifier->error("Could not read " + Convert::plain(path) + " as a .zdl file.");

            return;
        }

        loaded.name = config().uniqueProfileName(loaded.name);
        config().profiles.push_back(loaded);

        config().ensureConfigFiles();
        config().setActiveProfile(loaded.id);
        _hub->reload();

        _notifier->success("Added " + loaded.name + " from " + Convert::plain(path) + ".");
    });

    state.on_save_zdl([this](const slint::SharedString &path) {
        if (!Import::saveZdlFile(Convert::toPath(path), active())) {
            _notifier->error("Could not write " + Convert::plain(path) + ".");

            return;
        }

        _notifier->success("Saved " + active().name + " to " + Convert::plain(path) + ".");
    });

    state.on_launch([this] {
        if (config().profiles.empty()) {
            return;
        }

        _hub->start(ConfigBridge::profileKey(config().activeProfileId), active().name, config());
    });

    state.on_launch_at([this](const int index) {
        const std::vector<Profile> &profiles = config().profiles;

        if (index < 0 || std::cmp_greater_equal(index, profiles.size())) {
            return;
        }

        if (profiles[static_cast<size_t>(index)].id != config().activeProfileId) {
            config().setActiveProfile(profiles[static_cast<size_t>(index)].id);
            _hub->reload();
        }

        _hub->start(ConfigBridge::profileKey(config().activeProfileId), active().name, config());
    });
}
