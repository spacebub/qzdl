/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <QString>

#include "ZDLConfigModel.h"
#include "zdlcommon.h"

/**
 * Bridge to the pre-JSON INI world.  This is the only remaining consumer of
 * ZDLConf/ZDLSection/ZDLLine, which are still needed for two reasons:
 *
 *  - migrating an existing zdl.ini to zdl.json on first run, and
 *  - reading and writing .zdl launch configs, which stay in the old INI format
 *    because other Doom tools exchange them.
 */
namespace ZDLIniImport {

/** Converts a parsed legacy config into the model. Existing content is replaced. */
void fromLegacyConf(ZDLConf &conf, ZDLConfigModel &model);

/** Reads a legacy zdl.ini from disk into the model. */
bool loadLegacyFile(const QString &path, ZDLConfigModel &model);

/** Builds a profile from a [zdl.save] section. The id and name are left empty. */
ZDLProfile profileFromSection(ZDLSection *section);

/** Writes a profile into a [zdl.save] section, omitting unset values. */
void profileToSection(const ZDLProfile &profile, ZDLSection *section);

/**
 * Reads a .zdl file. The returned profile gets a fresh id and is named after
 * the file, so it can be added to the config as a new profile.
 */
bool loadZdlFile(const QString &path, ZDLProfile &profile);

/** Writes a profile out as a .zdl file in the classic INI format. */
bool saveZdlFile(const QString &path, const ZDLProfile &profile);

}
