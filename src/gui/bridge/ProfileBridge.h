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
#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "gui/bridge/Bridge.h"

// The profile page, and the profile list it picks from.
class ProfileBridge : public Bridge {
public:
    using Bridge::Bridge;

    void bind();

    // The active profile's fields.
    void push() const;

    void pushCards() const;
    void pushMaps();
    void pushCommand();

    // Anything that changes what would be launched.
    void touch();

    [[nodiscard]] static ui::ProfileCard cardOf(int index);

private:
    void showCommand();

    // Stats each profile, so built only when the sheet asks.
    void pushConfigDonors() const;

    static constexpr std::chrono::milliseconds PREVIEW{120};

    // Updated in place; see Models::reconcile.
    std::shared_ptr<slint::VectorModel<ui::ProfileCard>> _profileCards
        = std::make_shared<slint::VectorModel<ui::ProfileCard>>();
    std::shared_ptr<slint::VectorModel<ui::ConfigDonor>> _configDonors
        = std::make_shared<slint::VectorModel<ui::ConfigDonor>>();

    slint::Timer _preview;

    std::vector<std::string> _maps;
    bool _mapsKnown{false};

    // What _maps was worked out from.
    std::string _mapsMark;
};
