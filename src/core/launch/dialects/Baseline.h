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

#include <array>

#include "core/launch/Dialect.h"

namespace Dialect::dialects {

// clang-format off
// NOLINTBEGIN(readability-trailing-comma): each row is a line of the table, with its description below it
// The spelling a port gets for a feature when it lists no change.
inline constexpr std::array BASELINE = [] {
    using enum Feature;

    return std::to_array<Spelling>({
            {iwad,               {"-iwad"}},
            {skill,              {"-skill"}},
            {noMonsters,         {"-nomonsters"}},
            {fast,               {"-fast"}},
            {warp,               {"-warp"}},
            {configFile,         {"-config"}},
            {extraConfig,        {"-extraconfig"}},
            {saveFolder,         {"-savedir"}},
            {pwads,              {"-file"}},
            {deh,                {"-deh"}},
            {bex,                {"-deh"}},
            {exec,               {"+exec"}},
            {map,                {"+map"}},
            {respawn,            {"-respawn"}},
            {loadgame,           {"-loadgame"}},
            {playdemo,           {"-playdemo"}},
            {record,             {"-record"}},
            {timedemo,           {"-timedemo"}},
            {fastdemo,           {"-fastdemo"}},
            {longtics,           {"-longtics"}},
            {soloNet,            {"-solo-net"}},
            {continues,          {"-recordfromto"}},
            {levelstat,          {"-levelstat"}},
            {noAutoload,         {"-noautoload"}},
            {nomusic,            {"-nomusic"}},
            {nosfx,              {"-nosfx"}},
            {fullscreen,         {"-fullscreen"}},
            {windowed,           {"-window"}},
            {shotdir,            {"-shotdir"}},
            {players,            {"-host"}},
            {server,             {"-server"}},
            {privateServer,      {"-privateserver"}},
            {joins,              {"-join"}},
            {netPort,            {"-port"}},
            {deathmatch,         {"-deathmatch"}},
            {altDeathmatch,      {"-altdeath"}},
            {deathmatch3,        {"-dm3"}},
            {fragLimit,          {"+set", {}, "fraglimit"}},
            {timeLimit,          {"+set", {}, "timelimit"}},
            {teams,              {"+teamplay", {}, "1"}},
            {extratic,           {"-extratic"}},
            {netmode,            {"-netmode"}},
            {dup,                {"-dup"}},
            {pistolStart,        {"-pistolstart",
             "Starts every map with only the pistol. Left out while a demo is recorded or played and in a netgame, "
             "since the port stops on it there.",
             {}, Range{}, RECORDING | PLAYBACK | NETGAME}},
            {cheats,             {"+sv_cheats",
             "Turns on sv_cheats, which a netgame or the hardest skill needs before any cheat works.",
             "1"}},
            {coopSpawns,         {"-coop_spawns",
             "A single player game places the things meant for a cooperative one."}},
            {coopPreset,         {"-coop",
             "Sets up a cooperative game in one go. Multiplayer weapons stay out, players pass through each other, "
             "keys are shared, each player picks up a copy of an item and the last weapon is kept. It does nothing in "
             "a deathmatch."}},
            {alwaysApplyDmflags, {"+alwaysapplydmflags",
             "The dmflags that apply only in deathmatch apply in every game.",
             "1"}},
            {chainEpisodes,      {"-chain_episodes",
             "Finishing an episode goes on to the next one without a stop."}},
            {doubleAmmo,         {"-doubleammo",
             "Ammo pickups give twice as much. Left out while a demo is recorded or played and in a netgame, since "
             "the port stops on it there.",
             {}, Range{}, RECORDING | PLAYBACK | NETGAME}},
            {noCheats,           {"-nocheats",
             "Cheat codes that a DeHackEd patch changes are not applied."}},
            {noOptions,          {"-nooptions",
             "OPTIONS lumps in loaded files are not read, so a map cannot change the settings of the port."}},
            {limitRemoving,      {"-lr",
             "Vanilla complevels ignore the limits and overflows of the original game."}},
            {vanillaMode,        {"-vanilla",
             "Turns on every compatibility option and records demos as Doom 1.9 does. It holds only when the game "
             "starts from the command line, as with a map or a skill picked."}},
            {mbfBeta,            {"-beta",
             "Plays as the beta of MBF did. Needs betagrph.wad, and goes on only with the MBF complevel picked, since "
             "the port stops at any other.",
             {}, Range{}, 0, Levels{.from = MBF, .to = MBF, .onLine = true}}},
            {stroller,           {"-stroller",
             "Plays by the stroller category rules: half speed and no strafing. Turbo has no say while it is on."}},
            {flipLevels,         {"-fliplevels",
             "Mirrors every map from left to right, weapons included. With Mirrored weapons on as well, the weapons "
             "come out the right way round."}},
            {flipWeapons,        {"-flipweapons",
             "Mirrors the weapon sprites only. With Mirrored maps on as well, the weapons come out the right way "
             "round."}},
            {turbo,              {"-turbo",
             "Player speed in percent, from 10 to 255. 100 is normal.",
             {}, Range{.least = 10, .most = 255}}},
            {gameSpeed,          {"-speed",
             "Game speed in percent, from 10 to 1000.",
             {}, Range{.least = 10, .most = 1000}}},
            {dogs,               {"-dogs",
             "How many helper dogs join a single player game, from 0 to 3.",
             {}, Range{.least = 0, .most = 3}}},
            {dmflags,            {"+dmflags"}},
            {dmflags2,           {"+dmflags2"}},
            {dmflags3,           {"+dmflags3"}},
            {zadmflags,          {"+zadmflags"}},
            {lmsWeapons,         {"+lmsallowedweapons"}},
            {lmsSpectators,      {"+lmsspectatorsettings"}},
            {compatflags,        {"+compatflags"}},
            {compatflags2,       {"+compatflags2"}},
            {zacompatflags,      {"+zacompatflags"}},
            {complevel,          {"-complevel"}},
            {stdoutSwitch,       {"-stdout"}},
    });
}();
// NOLINTEND(readability-trailing-comma)
// clang-format on

inline constexpr Arg NO_ARG{};

// An empty Arg for a feature with no spelling.
[[nodiscard]] constexpr const Arg &baseline(const Feature feature) {
    for (const Spelling &each : BASELINE) {
        if (each.feature == feature) {
            return each.arg;
        }
    }

    return NO_ARG;
}

// The spelling of a feature on this port: its change, or the baseline.
[[nodiscard]] constexpr const Arg &resolve(const Port &port, const Feature feature) {
    for (const Spelling &each : port.changes) {
        if (each.feature == feature) {
            return each.arg;
        }
    }

    return baseline(feature);
}

}
