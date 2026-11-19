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

// ReSharper disable CppUseDesignatedInitializers
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "core/config/Profile.h"
#include "core/launch/Flags.h"

namespace Flags {

namespace {

using namespace Lines;

constexpr std::uint32_t bit(const unsigned at) {
    return 1U << at;
}

// What the port starts from, and the dialog of a word.
// clang-format off
constexpr std::array<WordInfo, WORDS> INFO = std::to_array<WordInfo>({
        {Word::dmflags,       0,    Family::gameplay},
        {Word::dmflags2,      0,    Family::gameplay},
        {Word::dmflags3,      0,    Family::gameplay},
        {Word::zadmflags,     0,    Family::gameplay},
        {Word::lmsWeapons,    1023, Family::gameplay},
        {Word::lmsSpectators, 2,    Family::gameplay},
        {Word::compatflags,   0,    Family::compatibility},
        {Word::compatflags2,  0,    Family::compatibility},
        {Word::zacompatflags, 0,    Family::compatibility},
});
// clang-format on

static_assert(std::ranges::all_of(ALL, [](const Word word) { return INFO[index(word)].word == word; }),
              "INFO in Word order");

constexpr std::array TITLES = std::to_array<std::string_view>({
        "Monsters",
        "Items",
        "Weapons and ammo",
        "Players",
        "Death and respawn",
        "Maps",
        "Cooperative",
        "Deathmatch and teams",
        "Last man standing",
        "Last man standing weapons",
        "Actors",
        "DeHackEd",
        "Maps and actions",
        "Physics",
        "Rendering",
        "Sound",
        "Scripts",
        "Online play",
});

static_assert(TITLES.size() == GROUPS, "a title for every group");

constexpr std::array FALLING = {
        Choice{0, "Map"},
        Choice{bit(3), "ZDoom"},
        Choice{bit(4), "Hexen"},
        Choice{bit(3) | bit(4), "Strife"},
};

constexpr std::array JUMP = {Choice{0, "Map"}, Choice{bit(16), "Off"}, Choice{bit(17), "On"}};
constexpr std::array LOOK = {Choice{0, "Map"}, Choice{bit(18), "Off"}, Choice{bit(19), "On"}};
constexpr std::array CROUCH = {Choice{0, "Map"}, Choice{bit(22), "Off"}, Choice{bit(23), "On"}};

// dmflags bits 18 and 19 are one choice of mouse look from ZDoom 2.8.0. Before it, and in Zandronum, they are two bits
// apart, and big powerups respawn with bit 19 instead of dmflags2 bit 27.
constexpr PortLines THREE_STATE_LOOK = ZDOOM_LINE_FROM_2_8;
constexpr PortLines TWO_STATE_LOOK = ZDOOM_TO_2_7 | ZANDRONUM;

// Skulltag forces jumping and crouching on with bits of its own.
constexpr std::array SKULLTAG_JUMP = {Choice{0, "Map"}, Choice{bit(16), "Off"}, Choice{bit(29), "On"}};
constexpr std::array SKULLTAG_CROUCH = {Choice{0, "Map"}, Choice{bit(21), "Off"}, Choice{bit(30), "On"}};

using enum Word;
using enum Group;

// Labels follow the bit: a ticked box sets it.
// clang-format off
// NOLINTBEGIN(readability-trailing-comma): each row is a line of the table, with its description below it
constexpr std::array WRITTEN = std::to_array<Option>({
        // --- dmflags ---
        {dmflags, bit(12),           EVERY_ZDOOM,  monsters,    "No monsters", "sv_nomonsters",
         "Monsters are left out of the map, as with -nomonsters."},
        {dmflags, bit(15),           EVERY_ZDOOM,  monsters,    "Fast monsters", "sv_fastmonsters",
         "Monsters and their projectiles move as fast as on Nightmare, as with -fast."},
        {dmflags, bit(13),           EVERY_ZDOOM,  monsters,    "Monsters respawn", "sv_monsterrespawn",
         "Dead monsters come back after a while, as on Nightmare and with -respawn."},
        {dmflags, bit(31),           GZDOOM_LINE,  monsters,    "Instant reactions", "sv_instantreaction",
         "Monsters attack at once, without the pause they take on every skill but Nightmare."},
        {dmflags, bit(0),            EVERY_ZDOOM,  items,       "No health pickups", "sv_nohealth",
         "Health pickups, berserk packs and megaspheres are left out of the map. Applies in deathmatch only."},
        {dmflags, bit(9),            EVERY_ZDOOM,  items,       "No armor pickups", "sv_noarmor",
         "Armor and megaspheres are left out of the map. Applies in deathmatch only."},
        {dmflags, bit(1),            EVERY_ZDOOM,  items,       "No powerups", "sv_noitems",
         "Meant to leave powerups out of the map, but no port acts on this bit."},
        {dmflags, bit(14),           EVERY_ZDOOM,  items,       "Items respawn", "sv_itemrespawn",
         "Picked up items come back after a while, as -deathmatch and -altdeath set."},
        {dmflags, bit(19),           TWO_STATE_LOOK, items,     "Big powerups respawn", "sv_respawnsuper",
         "Invulnerability, invisibility and the other big powerups come back when items respawn."},
        {dmflags, bit(11),           EVERY_ZDOOM,  weapons,     "Infinite ammo", "sv_infiniteammo",
         "Firing uses up no ammo."},
        {dmflags, bit(3) | bit(4),   EVERY_ZDOOM,  players,     "Falling damage", {},
         "Whether a fall hurts and how much. Map leaves it to the map, the rest force the rule of that game.", FALLING},
        {dmflags, bit(16) | bit(17), ZDOOM_FAMILY, players,     "Jumping", {},
         "Map leaves jumping to the map, Off and On force it.", JUMP},
        {dmflags, bit(22) | bit(23), ZDOOM_FAMILY, players,     "Crouching", {},
         "Map leaves crouching to the map, Off and On force it.", CROUCH},
        {dmflags, bit(18) | bit(19), THREE_STATE_LOOK, players, "Mouse look", {},
         "Map leaves mouse look to the map, Off and On force it.", LOOK},
        {dmflags, bit(18),           TWO_STATE_LOOK, players,   "No mouse look", "sv_nofreelook",
         "Mouse look is forced off for every player."},
        {dmflags, bit(20),           ZDOOM_FAMILY, players,     "Lock the field of view", "sv_nofov",
         "Players cannot change their field of view."},
        {dmflags, bit(8),            EVERY_ZDOOM,  respawn,     "Respawn automatically", "sv_forcerespawn",
         "Dead players come back after the respawn delay without pressing use. Applies in deathmatch only."},
        {dmflags, bit(6),            EVERY_ZDOOM,  maps,        "Stay on the same map", "sv_samelevel",
         "Exiting restarts the same map instead of moving on. Applies in deathmatch only."},
        {dmflags, bit(10),           EVERY_ZDOOM,  maps,        "Exits kill the player", "sv_noexit",
         "Using an exit kills the player instead of ending the map. Applies in deathmatch only."},
        {dmflags, bit(21),           ZDOOM_FAMILY, cooperative, "No multiplayer weapons", "sv_noweaponspawn",
         "Weapons placed for multiplayer only are left out of a cooperative game."},
        {dmflags, bit(24),           ZDOOM_LINE_FROM_2_6, cooperative, "Lose the whole inventory",
         "sv_cooploseinventory",
         "Players come back with nothing after dying in a cooperative game."},
        {dmflags, bit(24),           ZDOOM_TO_2_5, cooperative, "Lose the whole inventory", {},
         "Players come back with nothing after dying in a cooperative game."},
        {dmflags, bit(24),           ZANDRONUM,    cooperative, "Lose the whole inventory", "sv_coop_loseinventory",
         "Players come back with nothing after dying in a cooperative game."},
        {dmflags, bit(25),           ZDOOM_LINE_FROM_2_6, cooperative, "Lose keys", "sv_cooplosekeys",
         "Players lose their keys when they die in a cooperative game."},
        {dmflags, bit(25),           ZDOOM_TO_2_5, cooperative, "Lose keys", {},
         "Players lose their keys when they die in a cooperative game."},
        {dmflags, bit(25),           ZANDRONUM,    cooperative, "Lose keys", "sv_coop_losekeys",
         "Players lose their keys when they die in a cooperative game."},
        {dmflags, bit(26),           ZDOOM_LINE_FROM_2_6, cooperative, "Lose weapons", "sv_cooploseweapons",
         "Players lose the weapons they picked up when they die in a cooperative game."},
        {dmflags, bit(26),           ZDOOM_TO_2_5, cooperative, "Lose weapons", {},
         "Players lose the weapons they picked up when they die in a cooperative game."},
        {dmflags, bit(26),           ZANDRONUM,    cooperative, "Lose weapons", "sv_coop_loseweapons",
         "Players lose the weapons they picked up when they die in a cooperative game."},
        {dmflags, bit(27),           ZDOOM_LINE_FROM_2_6, cooperative, "Lose armor", "sv_cooplosearmor",
         "Players lose their armor when they die in a cooperative game."},
        {dmflags, bit(27),           ZDOOM_TO_2_5, cooperative, "Lose armor", {},
         "Players lose their armor when they die in a cooperative game."},
        {dmflags, bit(27),           ZANDRONUM,    cooperative, "Lose armor", "sv_coop_losearmor",
         "Players lose their armor when they die in a cooperative game."},
        {dmflags, bit(28),           ZDOOM_LINE_FROM_2_6, cooperative, "Lose powerups", "sv_cooplosepowerups",
         "Players lose their powerups when they die in a cooperative game."},
        {dmflags, bit(28),           ZDOOM_TO_2_5, cooperative, "Lose powerups", {},
         "Players lose their powerups when they die in a cooperative game."},
        {dmflags, bit(28),           ZANDRONUM,    cooperative, "Lose powerups", "sv_coop_losepowerups",
         "Players lose their powerups when they die in a cooperative game."},
        {dmflags, bit(29),           ZDOOM_LINE_FROM_2_6, cooperative, "Lose ammo", "sv_cooploseammo",
         "Players lose their ammo when they die in a cooperative game."},
        {dmflags, bit(29),           ZDOOM_TO_2_5, cooperative, "Lose ammo", {},
         "Players lose their ammo when they die in a cooperative game."},
        {dmflags, bit(29),           ZANDRONUM,    cooperative, "Lose ammo", "sv_coop_loseammo",
         "Players lose their ammo when they die in a cooperative game."},
        {dmflags, bit(30),           ZDOOM_LINE_FROM_2_6, cooperative, "Keep half the ammo", "sv_coophalveammo",
         "Players keep half their ammo when they die in a cooperative game, and never less than they start with."},
        {dmflags, bit(30),           ZDOOM_TO_2_5, cooperative, "Keep half the ammo", {},
         "Players keep half their ammo when they die in a cooperative game, and never less than they start with."},
        {dmflags, bit(30),           ZANDRONUM,    cooperative, "Keep half the ammo", "sv_coop_halveammo",
         "Players keep half their ammo when they die in a cooperative game, and never less than they start with."},
        {dmflags, bit(2),            EVERY_ZDOOM,  deathmatch,  "Weapons stay", "sv_weaponstay",
         "Weapon pickups stay where they are after a player takes one. Applies in deathmatch only."},
        {dmflags, bit(7),            EVERY_ZDOOM,  deathmatch,  "Spawn farthest from others", "sv_spawnfarthest",
         "Players respawn at the start farthest from everyone else instead of a random one."},

        // Eternity reads its own seven bits under the same name.
        {dmflags, bit(0), ETERNITY, items,      "Items respawn", {},
         "Picked up items come back after a while."},
        {dmflags, bit(2), ETERNITY, items,      "Barrels respawn", {},
         "Exploded barrels come back after a while."},
        {dmflags, bit(4), ETERNITY, items,      "Super items respawn", {},
         "Invulnerability, invisibility and the other super items come back along with other items."},
        {dmflags, bit(3), ETERNITY, respawn,    "Players drop items", {},
         "A player who dies drops what they carry."},
        {dmflags, bit(6), ETERNITY, respawn,    "Keep items on respawn", {},
         "Players keep their inventory when they come back."},
        {dmflags, bit(1), ETERNITY, deathmatch, "Weapons stay", {},
         "Weapon pickups stay where they are, and each player can take one once."},
        {dmflags, bit(5), ETERNITY, deathmatch, "Instagib", {},
         "Any damage at all gibs a player at once."},


        // --- dmflags2 ---
        {dmflags2, bit(25), ZDOOM_FAMILY, monsters,    "Icon of Sin kills its spawns", "sv_killbossmonst",
         "When the Icon of Sin dies, every monster it spawned dies with it."},
        {dmflags2, bit(26), ZDOOM_FAMILY_FROM_2_6, monsters, "Uncounted monsters in exit sectors",
         "sv_nocountendmonst",
         "Some sectors end the map once the player in them is nearly dead. Their monsters leave the kill count."},
        {dmflags2, bit(9),  EVERY_ZDOOM,  items,       "Barrels respawn", "sv_barrelrespawn",
         "Exploded barrels come back after a while."},
        {dmflags2, bit(27), THREE_STATE_LOOK, items,   "Big powerups respawn", "sv_respawnsuper",
         "Invulnerability, invisibility and the other big powerups come back when items respawn."},
        {dmflags2, bit(16), ZDOOM_FAMILY, items,       "Infinite inventory", "sv_infiniteinventory",
         "Using an inventory item does not use it up."},
        {dmflags2, bit(6),  EVERY_ZDOOM,  weapons,     "Double ammo", "sv_doubleammo",
         "Ammo pickups give twice as much, as on the easiest and the hardest skill."},
        {dmflags2, bit(8),  ZDOOM_LINE,   weapons,     "No BFG aiming", "sv_nobfgaim",
         "The BFG always fires straight ahead and ignores mouse look."},
        {dmflags2, bit(8),  SKULLTAG_LINE, weapons,     "BFG aiming", "sv_bfgfreeaim",
         "The BFG can be aimed up and down with mouse look."},
        {dmflags2, bit(11), SKULLTAG_LINE, weapons,     "Start with a shotgun", "sv_shotgunstart",
         "Players also carry a shotgun when they start and when they respawn."},
        {dmflags2, bit(23), ZDOOM_FAMILY, weapons,     "No autoaim", "sv_noautoaim",
         "Autoaim is off for every player, whatever their own setting."},
        {dmflags2, bit(30), GZDOOM_LINE,  weapons,     "No vertical bullet spread", "sv_novertspread",
         "Turns off the vertical bullet spread players can choose for themselves."},
        {dmflags2, bit(24), ZDOOM_FAMILY, weapons,     "Switch to empty weapons", "sv_dontcheckammo",
         "Weapon slot keys select a weapon even when it has no ammo."},
        {dmflags2, bit(7),  EVERY_ZDOOM,  players,     "Degeneration", "sv_degeneration",
         "Health above 100 drains slowly back down."},
        {dmflags2, bit(20), ZDOOM_FAMILY, players,     "No spying", "sv_disallowspying",
         "Players cannot look through the eyes of other players."},
        {dmflags2, bit(22), ZDOOM_FAMILY, players,     "No suicide", "sv_disallowsuicide",
         "The kill console command is refused."},
        {dmflags2, bit(1),  EVERY_ZDOOM,  respawn,     "Drop the weapon on death", "sv_weapondrop",
         "A player who dies drops the weapon in hand."},
        {dmflags2, bit(14), ZDOOM_FAMILY, respawn,     "No respawning", "sv_norespawn",
         "Dead players stay dead. GZDoom and UZDoom call it Monsters do not respawn, but it stops players."},
        {dmflags2, bit(10), ZDOOM_LINE,   respawn,     "Respawn protection", "sv_respawnprotect",
         "A player who respawns is invulnerable for a moment. Applies in deathmatch only."},
        {dmflags2, bit(10), SKULLTAG_LINE, respawn,     "No respawn protection", "sv_norespawninvul",
         "Players are no longer invulnerable for a moment after they respawn."},
        {dmflags2, bit(17), ZDOOM_FAMILY, maps,        "Kill all monsters to exit", "sv_killallmonsters",
         "The map cannot be exited until every monster is dead."},
        {dmflags2, bit(18), ZDOOM_FAMILY, maps,        "No automap", "sv_noautomap",
         "The automap cannot be opened."},
        {dmflags2, bit(19), ZDOOM_FAMILY, maps,        "No allies on the automap", "sv_noautomapallies",
         "Other players are not drawn on the automap."},
        {dmflags2, bit(29), GZDOOM_LINE,  maps,        "Multiplayer things in single player", "sv_alwaysspawnmulti",
         "Things placed for multiplayer only are spawned in single player too."},
        {dmflags2, bit(12), EVERY_ZDOOM,  cooperative, "Respawn where you died", "sv_samespawnspot",
         "Players come back where they died in a cooperative game instead of at their start."},
        {dmflags2, bit(28), GZDOOM_LINE,  cooperative, "No multiplayer things", "sv_nothingspawn",
         "Things placed for multiplayer only are left out of a cooperative game."},
        {dmflags2, bit(4),  EVERY_ZDOOM,  deathmatch,  "No team switching", "sv_noteamswitch",
         "Players cannot change team once they are on one."},
        {dmflags2, bit(5),  SKULLTAG_LINE, deathmatch,  "Server picks teams", "sv_noteamselect",
         "The server places players on teams and they cannot pick one."},
        {dmflags2, bit(13), ZDOOM_FAMILY, deathmatch,  "Keep frags between maps", "sv_keepfrags",
         "Frag counts carry over to the next map instead of starting again. Applies in deathmatch only."},
        {dmflags2, bit(15), ZDOOM_FAMILY, deathmatch,  "Lose a frag when killed", "sv_losefrag",
         "A player who is fragged loses one frag."},
        {dmflags2, bit(21), ZDOOM_FAMILY, deathmatch,  "Allow the chasecam", "sv_chasecam",
         "The chasecam cheat works in deathmatch without sv_cheats."},
        {dmflags2, bit(31), GZDOOM_LINE,  deathmatch,  "No extra ammo from weapons", "sv_noextraammo",
         "Weapon pickups in deathmatch give only the ammo they give in single player."},
        {dmflags2, bit(2),  SKULLTAG_LINE, deathmatch,  "No runes", "sv_norunes",
         "Skulltag runes are left out of deathmatch maps."},
        {dmflags2, bit(3),  SKULLTAG_LINE, deathmatch,  "Instant flag return", "sv_instantreturn",
         "A dropped flag or skull goes back to its base at once."},

        // The family row explains a GZDoom label that Skulltag does not have.
        {dmflags2, bit(15), SKULLTAG, respawn,     "No respawning", "sv_norespawn",
         "Dead players stay dead."},

        // --- dmflags3 ---
        {dmflags3, bit(0), GZDOOM_LINE, players,     "Players pass through each other", "sv_noplayerclip",
         "Players walk and shoot through each other."},
        {dmflags3, bit(7), GZDOOM_LINE, players,     "Pistol start every map", "sv_pistolstart",
         "Players lose their inventory at every map change and start again with a pistol."},
        {dmflags3, bit(1), GZDOOM_LINE, cooperative, "Share keys", "sv_coopsharekeys",
         "A key one player picks up goes to every player."},
        {dmflags3, bit(2), GZDOOM_LINE, cooperative, "Own copy of items", "sv_localitems",
         "Each player picks up a copy of an item, and the item stays for the others."},
        {dmflags3, bit(3), GZDOOM_LINE, cooperative, "Dropped items are shared", "sv_nolocaldrops",
         "Monsters drop items for whoever takes them first, even with own copies on."},
        {dmflags3, bit(4), GZDOOM_LINE, cooperative, "No coop only items", "sv_nocoopitems",
         "Items placed for a cooperative game only are left out."},
        {dmflags3, bit(5), GZDOOM_LINE, cooperative, "No coop only things", "sv_nocoopthings",
         "Everything placed for a cooperative game only is left out."},
        {dmflags3, bit(6), GZDOOM_LINE, cooperative, "Keep the weapon in hand", "sv_rememberlastweapon",
         "A player who respawns holds the weapon they died with instead of the best one."},


        // --- zadmflags ---
        {zadmflags, bit(9),  ZANDRONUM,          weapons,         "No rocket jumping", "sv_norocketjumping",
         "Players get no upward push from the splash of their own rockets."},
        {zadmflags, bit(4),  ZANDRONUM,          players,         "Players pass through each other",
         "sv_unblockplayers",
         "Players walk through each other."},
        {zadmflags, bit(20), ZANDRONUM_FROM_3_1, players,         "Software pitch limits",
         "sv_forcesoftwarepitchlimits",
         "Looking up and down is limited to the range of the software renderer."},
        {zadmflags, bit(15), ZANDRONUM,          items,           "No item dropping", "sv_nodrop",
         "The drop command is refused."},
        {zadmflags, bit(19), ZANDRONUM_FROM_3_1, maps,            "No closing doors", "sv_nodoorclose",
         "Players cannot close a door by hand."},
        {zadmflags, bit(26), ZANDRONUM_FROM_3_2, respawn,         "No spawn fog", "sv_nospawntelefog",
         "No teleport fog appears where a player spawns."},
        {zadmflags, bit(6),  ZANDRONUM,          cooperative,     "Share keys", "sv_sharekeys",
         "A key one player picks up goes to every player."},
        {zadmflags, bit(12), ZANDRONUM,          cooperative,     "Single player things", "sv_coop_spactorspawn",
         "An online cooperative game places things as single player does."},
        {zadmflags, bit(16), ZANDRONUM,          cooperative,     "No map reset in survival",
         "sv_survival_nomapresetondeath",
         "When every player is dead in survival, they come back with new lives and the map stays as it is."},
        {zadmflags, bit(17), ZANDRONUM,          cooperative,     "Out of lives keeps inventory",
         "sv_deadplayerscankeepinventory",
         "When a player runs out of lives, the cooperative lose rules decide what they keep."},
        {zadmflags, bit(7),  ZANDRONUM,          deathmatch,      "Keep teams between maps", "sv_keepteams",
         "Players stay on their team when the map changes."},
        {zadmflags, bit(10), ZANDRONUM,          deathmatch,      "Score damage, not kills",
         "sv_awarddamageinsteadkills",
         "Points are given for the damage dealt instead of for kills."},
        {zadmflags, bit(14), ZANDRONUM,          deathmatch,      "Teammates pass through each other",
         "sv_unblockallies",
         "Players on the same team walk through each other."},
        {zadmflags, bit(21), ZANDRONUM_FROM_3_1, deathmatch,      "Shoot through teammates", "sv_shootthroughallies",
         "Shots and projectiles pass through players on the same team."},
        {zadmflags, bit(22), ZANDRONUM_FROM_3_1, deathmatch,      "Teammates do not push", "sv_dontpushallies",
         "Attacks from a teammate do not push a player."},
        {zadmflags, bit(24), ZANDRONUM_FROM_3_2, deathmatch,      "Show enemy health and armor", "sv_donthidestats",
         "Players can see the health and armor of their enemies."},
        {zadmflags, bit(1),  ZANDRONUM,          lastManStanding, "Spectator rules everywhere",
         "sv_applylmsspectatorsettings",
         "The last man standing spectator rules apply in every game mode."},
        {zadmflags, bit(0),  ZANDRONUM,          online,          "No target names", "sv_noidentifytarget",
         "Clients cannot show the name of the player under the crosshair."},
        {zadmflags, bit(2),  ZANDRONUM,          online,          "No coop info", "sv_nocoopinfo",
         "Clients cannot draw the cooperative info overlay."},
        {zadmflags, bit(3),  ZANDRONUM,          online,          "No unlagged", "sv_nounlagged",
         "Shots are not corrected for each player's ping."},
        {zadmflags, bit(18), ZANDRONUM_FROM_3_1, online,          "No unlagged BFG tracers", "sv_nounlaggedbfgtracers",
         "BFG tracers are not corrected for each player's ping."},
        {zadmflags, bit(5),  ZANDRONUM,          online,          "No medals", "sv_nomedals",
         "Medals are neither given nor shown."},
        {zadmflags, bit(8),  ZANDRONUM_FROM_3_1, online,          "Force renderer defaults", "sv_forcevideodefaults",
         "Clients must use the default values of some renderer options."},
        {zadmflags, bit(8),  ZANDRONUM_3_0,      online,          "Force OpenGL defaults", "sv_forcegldefaults",
         "Clients must use the default values of some OpenGL options."},
        {zadmflags, bit(11), ZANDRONUM,          online,          "Force translucency", "sv_forcealpha",
         "Clients must draw translucency."},
        {zadmflags, bit(13), ZANDRONUM,          online,          "Full damage flash", "sv_maxbloodscalar",
         "Clients show the damage flash at full strength, as vanilla does."},
        {zadmflags, bit(23), ZANDRONUM_FROM_3_2, online,          "Clear the join queue between maps",
         "sv_dontkeepjoinqueue",
         "Players waiting to join start over when the map changes."},
        {zadmflags, bit(25), ZANDRONUM_FROM_3_2, online,          "Keep player colors", "sv_dontoverrideplayercolors",
         "Team and client options never change the color of a player."},
        {zadmflags, bit(27), ZANDRONUM_FROM_3_2, online,          "No ally icons", "sv_noallyicons",
         "No icons are drawn over the heads of allies."},
        {zadmflags, bit(28), ZANDRONUM_FROM_3_2, online,          "No enemy icons", "sv_noenemyicons",
         "No icons are drawn over the heads of enemies."},

        // --- last man standing ---
        {lmsSpectators, bit(0), SKULLTAG_LINE, lastManStanding, "Spectators can chat", "lms_spectatorchat",
         "Spectators may talk to players during a round."},
        {lmsSpectators, bit(1), SKULLTAG_LINE, lastManStanding, "Spectators can watch", "lms_spectatorview",
         "Spectators may watch the players still in a round."},
        {lmsSpectators, bit(2), ZANDRONUM_FROM_3_2, lastManStanding, "Spectators can voice chat",
         "lms_spectatorvoicechat",
         "Spectators may use voice chat with players during a round."},
        {lmsWeapons,    bit(9), SKULLTAG_LINE, lastManWeapons,  "Chainsaw", "lms_allowchainsaw",
         "Players get the chainsaw in last man standing."},
        {lmsWeapons,    bit(0), SKULLTAG_LINE, lastManWeapons,  "Pistol", "lms_allowpistol",
         "Players get the pistol in last man standing."},
        {lmsWeapons,    bit(1), SKULLTAG_LINE, lastManWeapons,  "Shotgun", "lms_allowshotgun",
         "Players get the shotgun in last man standing."},
        {lmsWeapons,    bit(2), SKULLTAG_LINE, lastManWeapons,  "Super shotgun", "lms_allowssg",
         "Players get the super shotgun in last man standing."},
        {lmsWeapons,    bit(3), SKULLTAG_LINE, lastManWeapons,  "Chaingun", "lms_allowchaingun",
         "Players get the chaingun in last man standing."},
        {lmsWeapons,    bit(4), SKULLTAG_LINE, lastManWeapons,  "Minigun", "lms_allowminigun",
         "Players get the minigun in last man standing."},
        {lmsWeapons,    bit(5), SKULLTAG_LINE, lastManWeapons,  "Rocket launcher", "lms_allowrocketlauncher",
         "Players get the rocket launcher in last man standing."},
        {lmsWeapons,    bit(6), SKULLTAG_LINE, lastManWeapons,  "Grenade launcher", "lms_allowgrenadelauncher",
         "Players get the grenade launcher in last man standing."},
        {lmsWeapons,    bit(7), SKULLTAG_LINE, lastManWeapons,  "Plasma rifle", "lms_allowplasma",
         "Players get the plasma rifle in last man standing."},
        {lmsWeapons,    bit(8), SKULLTAG_LINE, lastManWeapons,  "Railgun", "lms_allowrailgun",
         "Players get the railgun in last man standing."},

        // --- compatflags ---
        {compatflags, bit(25), GZDOOM_LINE,       actors,     "Arch-vile ghosts", "compat_vileghosts",
         "An Arch-vile can raise crushed monsters as ghosts that cannot be hurt.", {},
         {.feature = Dialect::Feature::corpseGibs, .lacking = true}},
        {compatflags, bit(25), GZDOOM | ZDOOM | ZANDRONUM, actors, "Crushed monsters turn to gibs", "compat_corpsegibs",
         "A crushed monster that bleeds turns into the generic gibs.", {},
         {.feature = Dialect::Feature::corpseGibs, .lacking = false}},
        {compatflags, bit(26), ZDOOM_FAMILY,      actors,     "Friends ignore monster blocking lines",
         "compat_noblockfriends",
         "Friendly monsters walk through lines that block monsters."},
        {compatflags, bit(2),  EVERY_ZDOOM,       actors,     "Limit lost souls", "compat_limitpain",
         "A pain elemental spits no lost soul while 20 or more are alive."},
        {compatflags, bit(24), ZDOOM_FAMILY,      actors,     "Monsters move with friction", "compat_mbfmonstermove",
         "Ice, mud and pushers move monsters as they do in MBF."},
        {compatflags, bit(20), ZDOOM_FAMILY,      actors,     "Monsters cannot be pushed off ledges",
         "compat_crossdropoff",
         "Nothing pushes a monster off a ledge it stands on."},
        {compatflags, bit(14), EVERY_ZDOOM,       actors,     "Monsters stick over dropoffs", "compat_dropoff",
         "A monster hanging over a dropoff cannot move off it."},
        {compatflags, bit(16), EVERY_ZDOOM,       actors,     "Monsters see invisible players", "compat_invisibility",
         "Partial invisibility does not throw off a monster's aim."},
        {compatflags, bit(22), ZDOOM_FAMILY,      actors,     "No minotaur flames in water", "compat_minotaur",
         "The minotaur floor fire goes off at once when the minotaur stands in water."},
        {compatflags, bit(7),  EVERY_ZDOOM,       actors,     "Drops land on the floor", "compat_notossdrops",
         "Items a monster drops appear on the floor instead of being tossed."},
        {compatflags, bit(12), EVERY_ZDOOM,       dehacked,   "DeHackEd health like Doom 2", "compat_dehhealth",
         "Health bonuses stop at the DeHackEd max health value, as doom2.exe reads it."},
        {compatflags, bit(23), ZDOOM_FAMILY,      dehacked,   "Original A_Mushroom speed", "compat_mushroom",
         "A_Mushroom in DeHackEd states throws its projectiles at the original speed."},
        {compatflags, bit(8),  EVERY_ZDOOM,       mapActions, "Any special line blocks use", "compat_useblocking",
         "A special line stops the use action from reaching a line behind it, as in Doom."},
        {compatflags, bit(21), ZDOOM_FAMILY,      mapActions, "Any boss death ends the map special",
         "compat_anybossdeath",
         "Any monster that calls A_BossDeath triggers the boss special, not only the bosses."},
        {compatflags, bit(9),  EVERY_ZDOOM,       mapActions, "No Boom door light", "compat_nodoorlight",
         "Tagged doors do not change the light of the sector."},
        {compatflags, bit(29), ZDOOM_FAMILY,      mapActions, "Find light levels like Doom", "compat_light",
         "Light specials look for neighbouring light levels the way Doom does."},
        {compatflags, bit(0),  EVERY_ZDOOM,       mapActions, "Shortest texture like Doom", "compat_shorttex",
         "A missing lower texture counts as the shortest when a special looks for one."},
        {compatflags, bit(1),  EVERY_ZDOOM,       mapActions, "Buggy stair building", "compat_stairs",
         "Stairs build with the loop bug from Doom."},
        {compatflags, bit(4),  EVERY_ZDOOM,       physics,    "Actors are infinitely tall", "compat_nopassover",
         "Nothing can move over or under another actor."},
        {compatflags, bit(15), EVERY_ZDOOM,       physics,    "Boom scrollers add up", "compat_boomscroll",
         "Several Boom scrollers under an actor add their pushes together."},
        {compatflags, bit(6),  EVERY_ZDOOM,       physics,    "Wall running", "compat_wallrun",
         "Wall collision works as in Doom, so players can wall run."},
        {compatflags, bit(10), EVERY_ZDOOM,       physics,    "Raven scrollers at original speed", "compat_ravenscroll",
         "Heretic and Hexen scrolling floors push at their original speed."},
        {compatflags, bit(13), EVERY_ZDOOM,       physics,    "Self referencing sectors do not block shots",
         "compat_trace",
         "Shots and sight pass through lines with the same sector on both sides."},
        {compatflags, bit(28), ZDOOM_FAMILY,      physics,    "Doom hitscan checks", "compat_hitscan",
         "Hitscan attacks find what they hit with the original Doom code."},
        {compatflags, bit(19), ZDOOM_FAMILY,      physics,    "Doom heights for projectiles", "compat_missileclip",
         "Projectiles hit monsters by their original heights."},
        {compatflags, bit(30), ZDOOM_FAMILY_FROM_2_6, rendering, "Polyobjects like Hexen",
         "compat_polyobj",
         "Polyobjects are drawn the old way in the software renderer."},
        {compatflags, bit(31), ZDOOM_FAMILY_FROM_2_6, rendering, "Masked middle textures like Doom",
         "compat_maskedmidtex",
         "Masked middle textures ignore their vertical offsets, as in Doom."},
        {compatflags, bit(27), ZDOOM_FAMILY,      rendering,  "Sprite sorting like Doom", "compat_spritesort",
         "Sprites at the same distance are drawn in the reverse order, as in Doom."},
        {compatflags, bit(5),  EVERY_ZDOOM,       sound,      "One sound per actor", "compat_soundslots",
         "An actor plays one sound at a time, which allows the silent BFG trick."},
        {compatflags, bit(3),  EVERY_ZDOOM,       sound,      "Silent pickups for others", "compat_silentpickup",
         "Only the player who picks something up hears it."},
        {compatflags, bit(17), ZDOOM_FAMILY,      sound,      "Instant floors make sound", "compat_silentinstantfloors",
         "Floors that move at once play their sound, as in Doom."},
        {compatflags, bit(18), ZDOOM_FAMILY,      sound,      "Sector sounds from the centre", "compat_sectorsounds",
         "A sector plays its sounds from its centre instead of the nearest point."},
        {compatflags, bit(11), EVERY_ZDOOM,       sound,      "Sound targets like Doom", "compat_soundtarget",
         "Monsters hear the player through the sector sound target, as in Doom."},


        // --- compatflags2 ---
        {compatflags2, bit(12), GZDOOM_LINE,             actors,     "Monsters avoid crushers", "compat_avoidhazard",
         "Monsters keep away from crushing ceilings, as in MBF."},
        {compatflags2, bit(13), GZDOOM_LINE,             actors,     "Monsters stay on lifts", "compat_stayonlift",
         "Monsters tend to stay on a lift they stand on, as in MBF."},
        {compatflags2, bit(15), GZDOOM_LINE,             actors,     "Voodoo zombies", "compat_voodoozombies",
         "Voodoo dolls keep their own health, and the death of one does not kill the player."},
        {compatflags2, bit(1),  ZDOOM_FAMILY,            mapActions, "Floors move like Doom", "compat_floormove",
         "Floors and ceilings move as in Doom, even through each other."},
        {compatflags2, bit(3),  ZDOOM_LINE_FROM_2_8, mapActions, "Doom point on line test",
         "compat_pointonline",
         "Uses the slightly wrong test Doom has for the side of a line."},
        {compatflags2, bit(4),  GZDOOM_LINE,             mapActions, "Exits trigger more than once", "compat_multiexit",
         "A map exit can be triggered again while the map is ending."},
        {compatflags2, bit(5),  GZDOOM_LINE,             mapActions, "Indirect teleports skip sector actions",
         "compat_teleport",
         "Scripted and other indirect teleports do not trigger sector actions."},
        {compatflags2, bit(16), GZDOOM_LINE,             mapActions, "Final Doom teleporters", "compat_fdteleport",
         "Teleporters keep the height glitch from Final Doom."},
        {compatflags2, bit(6),  GZDOOM_LINE | ZANDRONUM, mapActions, "Push specials skip the window check",
         "compat_pushwindow",
         "Push specials work through lines that do not block, as in Doom."},
        {compatflags2, bit(7),  GZDOOM_LINE,             mapActions, "Buggy switch range", "compat_checkswitchrange",
         "Switches keep the range check bug of older versions."},
        {compatflags2, bit(10), GZDOOM_LINE,             mapActions, "Buggy Strife railings", "compat_railing",
         "Strife railings behave as they did in the original game."},
        {compatflags2, bit(14), GZDOOM,                  mapActions, "No MBF21 line features", "compat_nombf21",
         "Doom format maps get none of the MBF21 line features."},
        {compatflags2, bit(18), UZDOOM,                  mapActions, "No lock message for voodoo dolls",
         "compat_novdolllockmsg",
         "A voodoo doll that touches a locked line shows no message."},
        {compatflags2, bit(20), UZDOOM,                  mapActions, "Ignore the reserved line flag",
         "compat_reservedlineflag",
         "Line flag bits that some maps set by mistake are ignored."},
        {compatflags2, bit(21), UZDOOM,                  mapActions, "Floor transfers carry the secret",
         "compat_transfersecret",
         "Boom floor transfer specials also pass on the secret flag."},
        {compatflags2, bit(0),  ZDOOM_FAMILY,            physics,    "No exact compass angles", "compat_badangles",
         "Players cannot face exactly north, south, east or west, as in Doom."},
        {compatflags2, bit(8),  GZDOOM_LINE,             physics,    "Explosions without vertical thrust",
         "compat_explode1",
         "Explosions push only sideways."},
        {compatflags2, bit(9),  GZDOOM_LINE,             physics,    "Explosions like Doom", "compat_explode2",
         "Explosions use the original Doom code throughout, infinite height included."},
        {compatflags2, bit(19), UZDOOM,                  physics,    "Mikoportals", "compat_emulatemikoportals",
         "Keeps the height underflow that Mikoportals rely on."},
        {compatflags2, bit(2),  ZDOOM_LINE_FROM_2_8, sound, "Sounds stop with their actor",
         "compat_soundcutoff",
         "A sound stops when the actor that made it is removed."},
        {compatflags2, bit(11), GZDOOM_LINE,             scripts,    "Script wait skips idle scripts", {},
         "ACS script_wait does not wait for a script that has not started yet."},
        {compatflags2, bit(17), GZDOOM_LINE,             scripts,    "No ACS argument check", {},
         "ACS functions skip the check on how many arguments they get."},


        // --- zacompatflags ---
        {zacompatflags, bit(23), ZANDRONUM, actors,    "No stealth monsters", "compat_disablestealthmonsters",
         "Stealth monsters are always visible, as in doom2.exe."},
        {zacompatflags, bit(4),  ZANDRONUM, actors,    "Floating spheres", "compat_nogravity_spheres",
         "Invulnerability, soul, mega and blur spheres float, as in Skulltag."},
        {zacompatflags, bit(3),  ZANDRONUM, physics,   "Doom random numbers", "compat_oldrandom",
         "Uses the random number table of Doom instead of the ZDoom generator."},
        {zacompatflags, bit(6),  ZANDRONUM, physics,   "Old explosion thrust", "compat_explosionthrust",
         "Explosions push sideways as in old ZDoom versions."},
        {zacompatflags, bit(24), ZANDRONUM, physics,   "Infinitely tall splash damage", "compat_oldradiusdmg",
         "Splash damage reaches any height, as in Doom."},
        {zacompatflags, bit(7),  ZANDRONUM, physics,   "Drops fall through bridges", "compat_bridgedrops",
         "Things that do not block, such as flags, fall through invisible bridges."},
        {zacompatflags, bit(8),  ZANDRONUM, physics,   "Old vertical movement", "compat_oldzdoomzmovement",
         "Jumping and gravity work as in old ZDoom versions."},
        {zacompatflags, bit(12), ZANDRONUM, physics,   "Skulltag jumping", "compat_skulltagjumping",
         "Jumping works as it did in Skulltag."},
        {zacompatflags, bit(17), ZANDRONUM, physics,   "Limit air movement", "compat_limited_airmovement",
         "Players have less control while in the air."},
        {zacompatflags, bit(18), ZANDRONUM, physics,   "Grab items like vanilla", "compat_plasmabump",
         "Players can grab items through walls with the plasma bump, as in vanilla."},
        {zacompatflags, bit(9),  ZANDRONUM, physics,   "Weapons lower fully", "compat_fullweaponlower",
         "A weapon lowers all the way before the next one comes up, as in vanilla."},
        {zacompatflags, bit(10), ZANDRONUM, physics,   "Autoaim with gaps", "compat_autoaim",
         "Autoaim uses the three vanilla traces, which leave gaps up and down."},
        {zacompatflags, bit(26), ZANDRONUM, physics,   "Switch weapons on pickup", "compat_oldweaponswitch",
         "Players always switch to a weapon they pick up, as in vanilla."},
        {zacompatflags, bit(19), ZANDRONUM, physics,   "Respawn instantly", "compat_instantrespawn",
         "Dead players can respawn at once, without the one second wait after death."},
        {zacompatflags, bit(22), ZANDRONUM, rendering, "Old intermission screens", "compat_oldintermission",
         "Uses the intermission screens and music of doom2.exe."},
        {zacompatflags, bit(11), ZANDRONUM, sound,     "West facing spawns are silent", "compat_silentwestspawns",
         "A player who spawns facing west makes no teleport sound, as in vanilla."},
        {zacompatflags, bit(21), ZANDRONUM, sound,     "Original sound curve", "compat_originalsoundcurve",
         "Meant to use the sound curve of doom2.exe, but the port never reads this bit."},
        {zacompatflags, bit(0),  ZANDRONUM, scripts,   "NET scripts run on clients", "compat_netscriptsareclientside",
         "ACS scripts marked NET run on the clients and not on the server."},
        {zacompatflags, bit(5),  ZANDRONUM, scripts,   "Player scripts outlive a disconnect",
         "compat_dont_stop_player_scripts_on_disconnect",
         "Scripts of a player who leaves keep running."},
        {zacompatflags, bit(13), ZANDRONUM_FROM_3_1, scripts, "Reset global ACS variables",
         "compat_resetglobalvarsonmapreset",
         "World and global ACS variables start over when the map resets."},
        {zacompatflags, bit(1),  ZANDRONUM, online,    "Clients send every button", "compat_clientssendfullbuttoninfo",
         "Clients send all their buttons to the server, which GetPlayerInput needs for the higher ones."},
        {zacompatflags, bit(2),  ZANDRONUM, online,    "No land command", "compat_noland",
         "The land console command is refused."},
        {zacompatflags, bit(14), ZANDRONUM_FROM_3_1, online, "No obituaries", "compat_noobituaries",
         "Messages about who killed whom are not printed."},
        {zacompatflags, bit(20), ZANDRONUM, online,    "No taunts", "compat_disabletaunts",
         "The taunt command is refused."},
        {zacompatflags, bit(25), ZANDRONUM, online,    "No crosshairs", "compat_nocrosshair",
         "Clients cannot draw a crosshair."},

});
// NOLINTEND(readability-trailing-comma)
// clang-format on

// A bit Skulltag reads like a written row of another port, at a bit and cvar of its own.
struct Moved {
    Word word;
    std::uint32_t mask;

    // The label of the row the group and the description come from.
    std::string_view label;

    std::string_view cvar;

    // Empty keeps the choices of that row.
    std::span<const Choice> choices{};
};

// clang-format off
// NOLINTBEGIN(readability-trailing-comma): each row is a line of the table
constexpr std::array SKULLTAG_MOVED = std::to_array<Moved>({
        {dmflags, bit(18), "Big powerups respawn", "sv_respawnsuper"},
        {dmflags, bit(16) | bit(29), "Jumping", {}, SKULLTAG_JUMP},
        {dmflags, bit(21) | bit(30), "Crouching", {}, SKULLTAG_CROUCH},
        {dmflags, bit(17), "No mouse look", "sv_nofreelook"},
        {dmflags, bit(19), "Lock the field of view", "sv_nofov"},
        {dmflags, bit(20), "No multiplayer weapons", "sv_noweaponspawn"},
        {dmflags, bit(22), "Lose the whole inventory", "sv_coop_loseinventory"},
        {dmflags, bit(23), "Lose keys", "sv_coop_losekeys"},
        {dmflags, bit(24), "Lose weapons", "sv_coop_loseweapons"},
        {dmflags, bit(25), "Lose armor", "sv_coop_losearmor"},
        {dmflags, bit(26), "Lose powerups", "sv_coop_losepowerups"},
        {dmflags, bit(27), "Lose ammo", "sv_coop_loseammo"},
        {dmflags, bit(28), "Keep half the ammo", "sv_coop_halveammo"},
        {dmflags2, bit(17), "Infinite inventory", "sv_infiniteinventory"},
        {dmflags2, bit(19), "No rocket jumping", "sv_norocketjumping"},
        {dmflags2, bit(28), "No autoaim", "sv_noautoaim"},
        {dmflags2, bit(25), "No spying", "sv_disallowspying"},
        {dmflags2, bit(27), "No suicide", "sv_disallowsuicide"},
        {dmflags2, bit(22), "Kill all monsters to exit", "sv_killallmonsters"},
        {dmflags2, bit(23), "No automap", "sv_noautomap"},
        {dmflags2, bit(24), "No allies on the automap", "sv_noautomapallies"},
        {dmflags2, bit(29), "Single player things", "sv_coop_spactorspawn"},
        {dmflags2, bit(13), "Keep teams between maps", "sv_keepteams"},
        {dmflags2, bit(14), "Keep frags between maps", "sv_keepfrags"},
        {dmflags2, bit(16), "Lose a frag when killed", "sv_losefrag"},
        {dmflags2, bit(20), "Score damage, not kills", "sv_awarddamageinsteadkills"},
        {dmflags2, bit(26), "Allow the chasecam", "sv_chasecam"},
        {dmflags2, bit(18), "Force OpenGL defaults", "sv_forcegldefaults"},
        {dmflags2, bit(21), "Force translucency", "sv_forcealpha"},
        {dmflags3, bit(1), "Spectator rules everywhere", "sv_applylmsspectatorsettings"},
        {dmflags3, bit(0), "No target names", "sv_noidentifytarget"},
        {dmflags3, bit(2), "No coop info", "sv_nocoopinfo"},
        {dmflags3, bit(3), "No unlagged", "sv_nounlagged"},
        {compatflags, bit(23), "No stealth monsters", "compat_disablestealthmonsters"},
        {compatflags, bit(30), "Monsters cannot be pushed off ledges", "compat_crossdropoff"},
        {compatflags, bit(17), "Limit air movement", "compat_limited_airmovement"},
        {compatflags, bit(18), "Grab items like vanilla", "compat_plasmabump"},
        {compatflags, bit(19), "Respawn instantly", "compat_instantrespawn"},
        {compatflags, bit(24), "Infinitely tall splash damage", "compat_oldradiusdmg"},
        {compatflags, bit(26), "Switch weapons on pickup", "compat_oldweaponswitch"},
        {compatflags, bit(29), "Doom heights for projectiles", "compat_missileclip"},
        {compatflags, bit(22), "Old intermission screens", "compat_oldintermission"},
        {compatflags, bit(21), "Original sound curve", "compat_originalsoundcurve"},
        {compatflags, bit(27), "Instant floors make sound", "compat_silentinstantfloors"},
        {compatflags, bit(28), "Sector sounds from the centre", "compat_sectorsounds"},
        {compatflags, bit(20), "No taunts", "compat_disabletaunts"},
        {compatflags, bit(25), "No crosshairs", "compat_nocrosshair"},
        {compatflags2, bit(0), "NET scripts run on clients", "compat_netscriptsareclientside"},
        {compatflags2, bit(1), "Clients send every button", "compat_clientssendfullbuttoninfo"},
        {compatflags2, bit(2), "No land command", "compat_noland"},
});
// NOLINTEND(readability-trailing-comma)
// clang-format on

// Not constexpr, so a call to it fails the build inside a constant expression.
void unwritten() {
}

// The written row a moved bit takes its text from. Every written row of the label must say the same.
constexpr Option writtenFor(const Moved &moved) {
    const Option *found = nullptr;

    for (const Option &each : WRITTEN) {
        if (each.label != moved.label || INFO[index(each.word)].family != INFO[index(moved.word)].family) {
            continue;
        }

        if (found != nullptr && (found->description != each.description || found->group != each.group)) {
            unwritten();
        }

        found = &each;
    }

    if (found == nullptr) {
        unwritten();

        return {};
    }

    return *found;
}

constexpr std::array OPTIONS = [] {
    std::array<Option, WRITTEN.size() + SKULLTAG_MOVED.size()> out{};
    std::size_t at = 0;

    for (const Option &each : WRITTEN) {
        out[at++] = each;
    }

    for (const Moved &each : SKULLTAG_MOVED) {
        Option placed = writtenFor(each);

        placed.word = each.word;
        placed.mask = each.mask;
        placed.lines = SKULLTAG;
        placed.cvar = each.cvar;
        placed.choices = each.choices.empty() ? placed.choices : each.choices;
        placed.gate = {};
        out[at++] = placed;
    }

    return out;
}();

// Grouped by word above, so the dialog order comes from a stable sort on the group.
constexpr std::array<Option, OPTIONS.size()> sorted() {
    std::array<Option, OPTIONS.size()> out = OPTIONS;

    for (std::size_t at = 1; at < out.size(); ++at) {
        const Option held = out[at];
        std::size_t to = at;

        while (to > 0 && out[to - 1].group > held.group) {
            out[to] = out[to - 1];
            --to;
        }

        out[to] = held;
    }

    return out;
}

constexpr std::array<Option, OPTIONS.size()> GROUPED = sorted();

constexpr std::array UZDOOM_MODES = {
        Mode{"Default", 0U, 0},
        Mode{"Doom", 2820160259U, 1048834},
        Mode{"Doom (strict)", 3122215895U, 1606155},
        Mode{"Boom", 2148050944U, 3145984},
        Mode{"Boom (strict)", 2416552144U, 3670536},
        Mode{"MBF", 2240325632U, 1061120},
        Mode{"MBF (strict)", 2508826832U, 3158792},
        Mode{"MBF21", 2241374208U, 3158272},
        Mode{"MBF21 (strict)", 2509875408U, 3683080},
        Mode{"ZDoom 2.0.63", 536872960U, 3145968},
};

constexpr std::array GZDOOM_MODES = {
        Mode{"Default", 0U, 0},
        Mode{"Doom", 2820160259U, 16642},
        Mode{"Doom (strict)", 3122215895U, 49675},
        Mode{"Boom", 2148050944U, 16640},
        Mode{"Boom (strict)", 2416552144U, 16904},
        Mode{"MBF", 2240325632U, 28928},
        Mode{"MBF (strict)", 2508826832U, 29448},
        Mode{"MBF21", 2241374208U, 12544},
        Mode{"MBF21 (strict)", 2509875408U, 13064},
        Mode{"ZDoom 2.0.63", 536872960U, 16624},
};

constexpr std::array ZDOOM_MODES = {
        Mode{"Default", 0, 0},
        Mode{"Doom", 672676611U, 2},
        Mode{"Doom (strict)", 3122215895U, 11},
        Mode{"Boom", 567296U, 0},
        Mode{"Boom (strict)", 302622928U, 8},
        Mode{"MBF", 92841984U, 0},
        Mode{"ZDoom 2.0.63", 536872960U, 0},
};

// ZDoom 2.6.0 to 2.7.1 have no compatflags2 bit 3 for the strict modes to set.
constexpr std::array ZDOOM_26_MODES = {
        Mode{"Default", 0, 0},
        Mode{"Doom", 672676611U, 2},
        Mode{"Doom (strict)", 3122215895U, 3},
        Mode{"Boom", 567296U, 0},
        Mode{"Boom (strict)", 302622928U, 0},
        Mode{"MBF", 92841984U, 0},
        Mode{"ZDoom 2.0.63", 536872960U, 0},
};

// ZDoom 2.5.0 has no compatflags2, and its Doom (strict) leaves out compatflags bit 31.
constexpr std::array ZDOOM_25_MODES = {
        Mode{"Default", 0, 0},
        Mode{"Doom", 672676611U, 0},
        Mode{"Doom (strict)", 974732247U, 0},
        Mode{"Boom", 567296U, 0},
        Mode{"Boom (strict)", 302622928U, 0},
        Mode{"MBF", 92841984U, 0},
        Mode{"ZDoom 2.0.63", 536872960U, 0},
};

constexpr std::array ZANDRONUM_MODES = {
        Mode{"Default", 0, 0},
        Mode{"Doom", 672676611U, 2},
        Mode{"Doom (strict)", 3122215895U, 3},
        Mode{"Boom", 567296U, 0},
        Mode{"Boom (strict)", 302622928U, 0},
        Mode{"MBF", 92841984U, 0},
        Mode{"ZDoom 2.0.63", 536872960U, 64},
};

// Skulltag modes set compatflags alone, and compatflags2 holds rules for online play.
constexpr std::array SKULLTAG_MODES = {
        Mode{"Default", 0, 0},
        Mode{"Doom", 1610627843U, 0},
        Mode{"Doom (strict)", 1610693399U, 0},
        Mode{"Boom", 43008U, 0},
        Mode{"ZDoom 2.0.63", 2048U, 0},
};

constexpr std::array BOTH_WORDS = {Word::compatflags, Word::compatflags2};
constexpr std::string_view FILLS_BOTH =
        "The compatibility mode of the port. A mode fills compatflags and compatflags2, "
        "and Custom means they match no mode";

constexpr std::array FIRST_WORD = {Word::compatflags};
constexpr std::string_view FILLS_FIRST =
        "The compatibility mode of the port. A mode fills compatflags, and Custom means it matches no mode";

constexpr std::size_t MODE_WORDS = BOTH_WORDS.size();

struct ModeTable {
    std::span<const Mode> modes;
    std::span<const Word> words;
};

constexpr ModeTable lineTable(const PortLine line) {
    switch (line) {
        case PortLine::uzdoom:
            return {.modes = UZDOOM_MODES, .words = BOTH_WORDS};
        case PortLine::gzdoom:
            return {.modes = GZDOOM_MODES, .words = BOTH_WORDS};
        case PortLine::zdoom28:
            return {.modes = ZDOOM_MODES, .words = BOTH_WORDS};
        case PortLine::zdoom26:
            return {.modes = ZDOOM_26_MODES, .words = BOTH_WORDS};
        case PortLine::zdoom25:
            return {.modes = ZDOOM_25_MODES, .words = FIRST_WORD};
        case PortLine::zandronum32:
        case PortLine::zandronum31:
        case PortLine::zandronum30:
            return {.modes = ZANDRONUM_MODES, .words = BOTH_WORDS};
        case PortLine::skulltag:
            return {.modes = SKULLTAG_MODES, .words = FIRST_WORD};
        default:
            break;
    }

    return {.modes = {}, .words = {}};
}

// A release without compatflags2 runs a mode on compatflags alone.
constexpr ModeTable releaseTable(const PortLine line, const Dialect::Features &features) {
    ModeTable table = lineTable(line);

    if (!table.words.empty() && !features.has(Dialect::Feature::compatflags2)) {
        table.words = FIRST_WORD;
    }

    return table;
}

constexpr std::uint32_t valueOf(const Mode &mode, const Word word) {
    return word == Word::compatflags ? mode.compatflags : mode.compatflags2;
}

}

const WordInfo &info(const Word word) {
    return INFO[index(word)];
}

std::string_view title(const Group group) {
    return TITLES[static_cast<std::size_t>(group)];
}

std::span<const Option> options() {
    return GROUPED;
}

bool offered(const Option &option, const PortLine line, const Dialect::Features &features) {
    const Gate &gate = option.gate;

    return has(option.lines, line) && features.has(feature(option.word))
           && (gate.feature == Dialect::Feature::count || features.has(gate.feature) != gate.lacking);
}

std::uint32_t known(const PortLine line, const Dialect::Features &features, const Word word) {
    std::uint32_t bits = 0;

    for (const Option &each : OPTIONS) {
        if (each.word == word && offered(each, line, features)) {
            bits |= each.mask;
        }
    }

    return bits;
}

std::span<const Mode> modes(const PortLine line) {
    return lineTable(line).modes;
}

std::span<const Word> modeWords(const PortLine line, const Dialect::Features &features) {
    return releaseTable(line, features).words;
}

std::string_view modeHint(const PortLine line, const Dialect::Features &features) {
    const std::span<const Word> words = modeWords(line, features);

    if (words.empty()) {
        return {};
    }

    return words.size() == BOTH_WORDS.size() ? FILLS_BOTH : FILLS_FIRST;
}

int modeIndex(const PortLine line, const Dialect::Features &features, const Profile &profile) {
    const ModeTable table = releaseTable(line, features);

    if (std::ranges::all_of(table.words, [&profile](const Word word) { return profile.flag(word).empty(); })) {
        return 0;
    }

    // An empty word beside a typed one runs as the port has it, which a mode cannot promise.
    std::array<std::uint32_t, MODE_WORDS> typed{};

    for (std::size_t at = 0; at < table.words.size(); ++at) {
        const std::optional<std::uint32_t> value = parse(profile.flag(table.words[at]));

        if (!value) {
            return -1;
        }

        typed[at] = *value;
    }

    const auto found = std::ranges::find_if(table.modes, [&table, &typed](const Mode &mode) {
        for (std::size_t at = 0; at < table.words.size(); ++at) {
            if (valueOf(mode, table.words[at]) != typed[at]) {
                return false;
            }
        }

        return true;
    });

    return found == table.modes.end() ? -1 : static_cast<int>(found - table.modes.begin()) + 1;
}

void setMode(const PortLine line, const Dialect::Features &features, Profile &profile, const int index) {
    const ModeTable table = releaseTable(line, features);
    const bool picked = index > 0 && std::cmp_less_equal(index, table.modes.size());

    for (const Word word : table.words) {
        profile.setFlag(word,
                        picked ? std::to_string(valueOf(table.modes[static_cast<std::size_t>(index) - 1], word))
                               : std::string());
    }
}

std::optional<std::uint32_t> parse(const std::string_view typed) {
    if (typed.empty()) {
        return std::nullopt;
    }

    std::uint64_t value = 0;

    for (const char each : typed) {
        if (each < '0' || each > '9') {
            return std::nullopt;
        }

        value = (value * 10) + static_cast<std::uint64_t>(each - '0');

        if (value > std::numeric_limits<std::uint32_t>::max()) {
            return std::nullopt;
        }
    }

    return static_cast<std::uint32_t>(value);
}

std::string decimal(const std::uint32_t value, const bool signedWord) {
    // ZDoom 2.8 and Zandronum read it with strtol, which caps at 2^31 - 1 where long is 32 bits, as on Windows.
    // The same 32 bits as a negative number come through whole.
    if (signedWord && value > std::numeric_limits<std::int32_t>::max()) {
        return std::to_string(static_cast<std::int32_t>(value));
    }

    return std::to_string(value);
}

}
