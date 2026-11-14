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

#include <array>
#include <limits>

#include "core/config/Schema.h"
#include "core/launch/Flags.h"

namespace Flags {

namespace {

constexpr Sets UZ = bit(Set::uzdoom);
constexpr Sets GZ = bit(Set::gzdoom);
constexpr Sets ZD = bit(Set::zdoom);
constexpr Sets ZA = bit(Set::zandronum);
constexpr Sets ET = bit(Set::eternity);

constexpr Sets MODERN = UZ | GZ;
constexpr Sets ZDOOMS = UZ | GZ | ZD;
constexpr Sets FAMILY = UZ | GZ | ZD | ZA;

constexpr std::array<WordInfo, WORDS> WORD_INFO = std::to_array<WordInfo>({
        {FlagWord::DMFLAGS, Family::gameplay, FAMILY | ET, 0, 0},
        {FlagWord::DMFLAGS2, Family::gameplay, FAMILY, 0, 0},
        {FlagWord::DMFLAGS3, Family::gameplay, MODERN, 0, 0},
        {FlagWord::ZADMFLAGS, Family::gameplay, ZA, 0, 0},
        {FlagWord::LMS_WEAPONS, Family::gameplay, ZA, 1023, 0},
        {FlagWord::LMS_SPECTATORS, Family::gameplay, ZA, 2, 0},
        {FlagWord::COMPATFLAGS, Family::compatibility, FAMILY, 0, ZDOOMS},
        {FlagWord::COMPATFLAGS2, Family::compatibility, FAMILY, 0, ZDOOMS},
        {FlagWord::ZACOMPATFLAGS, Family::compatibility, ZA, 0, 0},
});

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
        Choice{1U << 3U, "ZDoom"},
        Choice{2U << 3U, "Hexen"},
        Choice{3U << 3U, "Strife"},
};

constexpr std::array JUMP = {Choice{0, "Map"}, Choice{1U << 16U, "Off"}, Choice{1U << 17U, "On"}};
constexpr std::array LOOK = {Choice{0, "Map"}, Choice{1U << 18U, "Off"}, Choice{1U << 19U, "On"}};
constexpr std::array CROUCH = {Choice{0, "Map"}, Choice{1U << 22U, "Off"}, Choice{1U << 23U, "On"}};

using enum Word;
using enum Group;

// Labels follow the bit: a ticked box sets it.
// clang-format off
// NOLINTBEGIN(readability-trailing-comma): each row is a line of the table, with its description below it
constexpr std::array OPTIONS = std::to_array<Option>({
        // --- dmflags ---
        {dmflags, 1U << 12U, FAMILY, monsters, "No monsters", "sv_nomonsters",
         "Monsters are left out of the map, as with -nomonsters."},
        {dmflags, 1U << 15U, FAMILY, monsters, "Fast monsters", "sv_fastmonsters",
         "Monsters and their projectiles move as fast as on Nightmare, as with -fast."},
        {dmflags, 1U << 13U, FAMILY, monsters, "Monsters respawn", "sv_monsterrespawn",
         "Dead monsters come back after a while, as on Nightmare and with -respawn."},
        {dmflags, 1U << 31U, MODERN, monsters, "Instant reactions", "sv_instantreaction",
         "Monsters attack at once, without the pause they take on every skill but Nightmare."},
        {dmflags, 1U << 0U, FAMILY, items, "No health pickups", "sv_nohealth",
         "Health pickups, berserk packs and megaspheres are left out of the map. Applies in deathmatch only."},
        {dmflags, 1U << 9U, FAMILY, items, "No armor pickups", "sv_noarmor",
         "Armor and megaspheres are left out of the map. Applies in deathmatch only."},
        {dmflags, 1U << 1U, FAMILY, items, "No powerups", "sv_noitems",
         "Meant to leave powerups out of the map, but no port acts on this bit."},
        {dmflags, 1U << 14U, FAMILY, items, "Items respawn", "sv_itemrespawn",
         "Picked up items come back after a while, as -deathmatch and -altdeath set."},
        {dmflags, 1U << 19U, ZA, items, "Big powerups respawn", "sv_respawnsuper",
         "Invulnerability, invisibility and the other big powerups come back when items respawn."},
        {dmflags, 1U << 11U, FAMILY, weapons, "Infinite ammo", "sv_infiniteammo",
         "Firing uses up no ammo."},
        {dmflags, 3U << 3U, FAMILY, players, "Falling damage", {},
         "Whether a fall hurts and how much. Map leaves it to the map, the rest force the rule of that game.",
         FALLING},
        {dmflags, 3U << 16U, FAMILY, players, "Jumping", {},
         "Map leaves jumping to the map, Off and On force it.", JUMP},
        {dmflags, 3U << 22U, FAMILY, players, "Crouching", {},
         "Map leaves crouching to the map, Off and On force it.", CROUCH},
        {dmflags, 3U << 18U, ZDOOMS, players, "Mouse look", {},
         "Map leaves mouse look to the map, Off and On force it.", LOOK},
        {dmflags, 1U << 18U, ZA, players, "No mouse look", "sv_nofreelook",
         "Mouse look is forced off for every player."},
        {dmflags, 1U << 20U, FAMILY, players, "Lock the field of view", "sv_nofov",
         "Players cannot change their field of view."},
        {dmflags, 1U << 8U, FAMILY, respawn, "Respawn automatically", "sv_forcerespawn",
         "Dead players come back after the respawn delay without pressing use. Applies in deathmatch only."},
        {dmflags, 1U << 6U, FAMILY, maps, "Stay on the same map", "sv_samelevel",
         "Exiting restarts the same map instead of moving on. Applies in deathmatch only."},
        {dmflags, 1U << 10U, FAMILY, maps, "Exits kill the player", "sv_noexit",
         "Using an exit kills the player instead of ending the map. Applies in deathmatch only."},
        {dmflags, 1U << 21U, FAMILY, cooperative, "No multiplayer weapons", "sv_noweaponspawn",
         "Weapons placed for multiplayer only are left out of a cooperative game."},
        {dmflags, 1U << 24U, ZDOOMS, cooperative, "Lose the whole inventory", "sv_cooploseinventory",
         "Players come back with nothing after dying in a cooperative game."},
        {dmflags, 1U << 24U, ZA, cooperative, "Lose the whole inventory", "sv_coop_loseinventory",
         "Players come back with nothing after dying in a cooperative game."},
        {dmflags, 1U << 25U, ZDOOMS, cooperative, "Lose keys", "sv_cooplosekeys",
         "Players lose their keys when they die in a cooperative game."},
        {dmflags, 1U << 25U, ZA, cooperative, "Lose keys", "sv_coop_losekeys",
         "Players lose their keys when they die in a cooperative game."},
        {dmflags, 1U << 26U, ZDOOMS, cooperative, "Lose weapons", "sv_cooploseweapons",
         "Players lose the weapons they picked up when they die in a cooperative game."},
        {dmflags, 1U << 26U, ZA, cooperative, "Lose weapons", "sv_coop_loseweapons",
         "Players lose the weapons they picked up when they die in a cooperative game."},
        {dmflags, 1U << 27U, ZDOOMS, cooperative, "Lose armor", "sv_cooplosearmor",
         "Players lose their armor when they die in a cooperative game."},
        {dmflags, 1U << 27U, ZA, cooperative, "Lose armor", "sv_coop_losearmor",
         "Players lose their armor when they die in a cooperative game."},
        {dmflags, 1U << 28U, ZDOOMS, cooperative, "Lose powerups", "sv_cooplosepowerups",
         "Players lose their powerups when they die in a cooperative game."},
        {dmflags, 1U << 28U, ZA, cooperative, "Lose powerups", "sv_coop_losepowerups",
         "Players lose their powerups when they die in a cooperative game."},
        {dmflags, 1U << 29U, ZDOOMS, cooperative, "Lose ammo", "sv_cooploseammo",
         "Players lose their ammo when they die in a cooperative game."},
        {dmflags, 1U << 29U, ZA, cooperative, "Lose ammo", "sv_coop_loseammo",
         "Players lose their ammo when they die in a cooperative game."},
        {dmflags, 1U << 30U, ZDOOMS, cooperative, "Keep half the ammo", "sv_coophalveammo",
         "Players keep half their ammo when they die in a cooperative game, and never less than they start with."},
        {dmflags, 1U << 30U, ZA, cooperative, "Keep half the ammo", "sv_coop_halveammo",
         "Players keep half their ammo when they die in a cooperative game, and never less than they start with."},
        {dmflags, 1U << 2U, FAMILY, deathmatch, "Weapons stay", "sv_weaponstay",
         "Weapon pickups stay where they are after a player takes one. Applies in deathmatch only."},
        {dmflags, 1U << 7U, FAMILY, deathmatch, "Spawn farthest from others", "sv_spawnfarthest",
         "Players respawn at the start farthest from everyone else instead of a random one."},

        // Eternity reads its own seven bits under the same name.
        {dmflags, 1U << 0U, ET, items, "Items respawn", {},
         "Picked up items come back after a while."},
        {dmflags, 1U << 2U, ET, items, "Barrels respawn", {},
         "Exploded barrels come back after a while."},
        {dmflags, 1U << 4U, ET, items, "Super items respawn", {},
         "Invulnerability, invisibility and the other super items come back along with other items."},
        {dmflags, 1U << 3U, ET, respawn, "Players drop items", {},
         "A player who dies drops what they carry."},
        {dmflags, 1U << 6U, ET, respawn, "Keep items on respawn", {},
         "Players keep their inventory when they come back."},
        {dmflags, 1U << 1U, ET, deathmatch, "Weapons stay", {},
         "Weapon pickups stay where they are, and each player can take one once."},
        {dmflags, 1U << 5U, ET, deathmatch, "Instagib", {},
         "Any damage at all gibs a player at once."},

        // --- dmflags2 ---
        {dmflags2, 1U << 25U, FAMILY, monsters, "Icon of Sin kills its spawns", "sv_killbossmonst",
         "When the Icon of Sin dies, every monster it spawned dies with it."},
        {dmflags2, 1U << 26U, FAMILY, monsters, "Uncounted monsters in exit sectors", "sv_nocountendmonst",
         "Some sectors end the map once the player in them is nearly dead. Their monsters leave the kill count."},
        {dmflags2, 1U << 9U, FAMILY, items, "Barrels respawn", "sv_barrelrespawn",
         "Exploded barrels come back after a while."},
        {dmflags2, 1U << 27U, ZDOOMS, items, "Big powerups respawn", "sv_respawnsuper",
         "Invulnerability, invisibility and the other big powerups come back when items respawn."},
        {dmflags2, 1U << 16U, FAMILY, items, "Infinite inventory", "sv_infiniteinventory",
         "Using an inventory item does not use it up."},
        {dmflags2, 1U << 6U, FAMILY, weapons, "Double ammo", "sv_doubleammo",
         "Ammo pickups give twice as much, as on the easiest and the hardest skill."},
        {dmflags2, 1U << 8U, ZDOOMS, weapons, "No BFG aiming", "sv_nobfgaim",
         "The BFG always fires straight ahead and ignores mouse look."},
        {dmflags2, 1U << 8U, ZA, weapons, "BFG aiming", "sv_bfgfreeaim",
         "The BFG can be aimed up and down with mouse look."},
        {dmflags2, 1U << 11U, ZA, weapons, "Start with a shotgun", "sv_shotgunstart",
         "Players also carry a shotgun when they start and when they respawn."},
        {dmflags2, 1U << 23U, FAMILY, weapons, "No autoaim", "sv_noautoaim",
         "Autoaim is off for every player, whatever their own setting."},
        {dmflags2, 1U << 30U, MODERN, weapons, "No vertical bullet spread", "sv_novertspread",
         "Turns off the vertical bullet spread players can choose for themselves."},
        {dmflags2, 1U << 24U, FAMILY, weapons, "Switch to empty weapons", "sv_dontcheckammo",
         "Weapon slot keys select a weapon even when it has no ammo."},
        {dmflags2, 1U << 7U, FAMILY, players, "Degeneration", "sv_degeneration",
         "Health above 100 drains slowly back down."},
        {dmflags2, 1U << 20U, FAMILY, players, "No spying", "sv_disallowspying",
         "Players cannot look through the eyes of other players."},
        {dmflags2, 1U << 22U, FAMILY, players, "No suicide", "sv_disallowsuicide",
         "The kill console command is refused."},
        {dmflags2, 1U << 1U, FAMILY, respawn, "Drop the weapon on death", "sv_weapondrop",
         "A player who dies drops the weapon in hand."},
        {dmflags2, 1U << 14U, FAMILY, respawn, "No respawning", "sv_norespawn",
         "Dead players stay dead. GZDoom and UZDoom call it Monsters do not respawn, but it stops players."},
        {dmflags2, 1U << 10U, ZDOOMS, respawn, "Respawn protection", "sv_respawnprotect",
         "A player who respawns is invulnerable for a moment. Applies in deathmatch only."},
        {dmflags2, 1U << 10U, ZA, respawn, "No respawn protection", "sv_norespawninvul",
         "Players are no longer invulnerable for a moment after they respawn."},
        {dmflags2, 1U << 17U, FAMILY, maps, "Kill all monsters to exit", "sv_killallmonsters",
         "The map cannot be exited until every monster is dead."},
        {dmflags2, 1U << 18U, FAMILY, maps, "No automap", "sv_noautomap",
         "The automap cannot be opened."},
        {dmflags2, 1U << 19U, FAMILY, maps, "No allies on the automap", "sv_noautomapallies",
         "Other players are not drawn on the automap."},
        {dmflags2, 1U << 29U, MODERN, maps, "Multiplayer things in single player", "sv_alwaysspawnmulti",
         "Things placed for multiplayer only are spawned in single player too."},
        {dmflags2, 1U << 12U, FAMILY, cooperative, "Respawn where you died", "sv_samespawnspot",
         "Players come back where they died in a cooperative game instead of at their start."},
        {dmflags2, 1U << 28U, MODERN, cooperative, "No multiplayer things", "sv_nothingspawn",
         "Things placed for multiplayer only are left out of a cooperative game."},
        {dmflags2, 1U << 4U, FAMILY, deathmatch, "No team switching", "sv_noteamswitch",
         "Players cannot change team once they are on one."},
        {dmflags2, 1U << 5U, ZA, deathmatch, "Server picks teams", "sv_noteamselect",
         "The server places players on teams and they cannot pick one."},
        {dmflags2, 1U << 13U, FAMILY, deathmatch, "Keep frags between maps", "sv_keepfrags",
         "Frag counts carry over to the next map instead of starting again. Applies in deathmatch only."},
        {dmflags2, 1U << 15U, FAMILY, deathmatch, "Lose a frag when killed", "sv_losefrag",
         "A player who is fragged loses one frag."},
        {dmflags2, 1U << 21U, FAMILY, deathmatch, "Allow the chasecam", "sv_chasecam",
         "The chasecam cheat works in deathmatch without sv_cheats."},
        {dmflags2, 1U << 31U, MODERN, deathmatch, "No extra ammo from weapons", "sv_noextraammo",
         "Weapon pickups in deathmatch give only the ammo they give in single player."},
        {dmflags2, 1U << 2U, ZA, deathmatch, "No runes", "sv_norunes",
         "Skulltag runes are left out of deathmatch maps."},
        {dmflags2, 1U << 3U, ZA, deathmatch, "Instant flag return", "sv_instantreturn",
         "A dropped flag or skull goes back to its base at once."},

        // --- dmflags3 ---
        {dmflags3, 1U << 0U, MODERN, players, "Players pass through each other", "sv_noplayerclip",
         "Players walk and shoot through each other."},
        {dmflags3, 1U << 7U, MODERN, players, "Pistol start every map", "sv_pistolstart",
         "Players lose their inventory at every map change and start again with a pistol."},
        {dmflags3, 1U << 1U, MODERN, cooperative, "Share keys", "sv_coopsharekeys",
         "A key one player picks up goes to every player."},
        {dmflags3, 1U << 2U, MODERN, cooperative, "Own copy of items", "sv_localitems",
         "Each player picks up a copy of an item, and the item stays for the others."},
        {dmflags3, 1U << 3U, MODERN, cooperative, "Dropped items are shared", "sv_nolocaldrops",
         "Monsters drop items for whoever takes them first, even with own copies on."},
        {dmflags3, 1U << 4U, MODERN, cooperative, "No coop only items", "sv_nocoopitems",
         "Items placed for a cooperative game only are left out."},
        {dmflags3, 1U << 5U, MODERN, cooperative, "No coop only things", "sv_nocoopthings",
         "Everything placed for a cooperative game only is left out."},
        {dmflags3, 1U << 6U, MODERN, cooperative, "Keep the weapon in hand", "sv_rememberlastweapon",
         "A player who respawns holds the weapon they died with instead of the best one."},

        // --- zadmflags ---
        {zadmflags, 1U << 9U, ZA, weapons, "No rocket jumping", "sv_norocketjumping",
         "Players get no upward push from the splash of their own rockets."},
        {zadmflags, 1U << 4U, ZA, players, "Players pass through each other", "sv_unblockplayers",
         "Players walk through each other."},
        {zadmflags, 1U << 20U, ZA, players, "Software pitch limits", "sv_forcesoftwarepitchlimits",
         "Looking up and down is limited to the range of the software renderer."},
        {zadmflags, 1U << 15U, ZA, items, "No item dropping", "sv_nodrop",
         "The drop command is refused."},
        {zadmflags, 1U << 19U, ZA, maps, "No closing doors", "sv_nodoorclose",
         "Players cannot close a door by hand."},
        {zadmflags, 1U << 26U, ZA, respawn, "No spawn fog", "sv_nospawntelefog",
         "No teleport fog appears where a player spawns."},
        {zadmflags, 1U << 6U, ZA, cooperative, "Share keys", "sv_sharekeys",
         "A key one player picks up goes to every player."},
        {zadmflags, 1U << 12U, ZA, cooperative, "Single player things", "sv_coop_spactorspawn",
         "An online cooperative game places things as single player does."},
        {zadmflags, 1U << 16U, ZA, cooperative, "No map reset in survival", "sv_survival_nomapresetondeath",
         "When every player is dead in survival, they come back with new lives and the map stays as it is."},
        {zadmflags, 1U << 17U, ZA, cooperative, "Out of lives keeps inventory", "sv_deadplayerscankeepinventory",
         "When a player runs out of lives, the cooperative lose rules decide what they keep."},
        {zadmflags, 1U << 7U, ZA, deathmatch, "Keep teams between maps", "sv_keepteams",
         "Players stay on their team when the map changes."},
        {zadmflags, 1U << 10U, ZA, deathmatch, "Score damage, not kills", "sv_awarddamageinsteadkills",
         "Points are given for the damage dealt instead of for kills."},
        {zadmflags, 1U << 14U, ZA, deathmatch, "Teammates pass through each other", "sv_unblockallies",
         "Players on the same team walk through each other."},
        {zadmflags, 1U << 21U, ZA, deathmatch, "Shoot through teammates", "sv_shootthroughallies",
         "Shots and projectiles pass through players on the same team."},
        {zadmflags, 1U << 22U, ZA, deathmatch, "Teammates do not push", "sv_dontpushallies",
         "Attacks from a teammate do not push a player."},
        {zadmflags, 1U << 24U, ZA, deathmatch, "Show enemy health and armor", "sv_donthidestats",
         "Players can see the health and armor of their enemies."},
        {zadmflags, 1U << 1U, ZA, lastManStanding, "Spectator rules everywhere", "sv_applylmsspectatorsettings",
         "The last man standing spectator rules apply in every game mode."},
        {zadmflags, 1U << 0U, ZA, online, "No target names", "sv_noidentifytarget",
         "Clients cannot show the name of the player under the crosshair."},
        {zadmflags, 1U << 2U, ZA, online, "No coop info", "sv_nocoopinfo",
         "Clients cannot draw the cooperative info overlay."},
        {zadmflags, 1U << 3U, ZA, online, "No unlagged", "sv_nounlagged",
         "Shots are not corrected for each player's ping."},
        {zadmflags, 1U << 18U, ZA, online, "No unlagged BFG tracers", "sv_nounlaggedbfgtracers",
         "BFG tracers are not corrected for each player's ping."},
        {zadmflags, 1U << 5U, ZA, online, "No medals", "sv_nomedals",
         "Medals are neither given nor shown."},
        {zadmflags, 1U << 8U, ZA, online, "Force renderer defaults", "sv_forcevideodefaults",
         "Clients must use the default values of some renderer options."},
        {zadmflags, 1U << 11U, ZA, online, "Force translucency", "sv_forcealpha",
         "Clients must draw translucency."},
        {zadmflags, 1U << 13U, ZA, online, "Full damage flash", "sv_maxbloodscalar",
         "Clients show the damage flash at full strength, as vanilla does."},
        {zadmflags, 1U << 23U, ZA, online, "Clear the join queue between maps", "sv_dontkeepjoinqueue",
         "Players waiting to join start over when the map changes."},
        {zadmflags, 1U << 25U, ZA, online, "Keep player colors", "sv_dontoverrideplayercolors",
         "Team and client options never change the color of a player."},
        {zadmflags, 1U << 27U, ZA, online, "No ally icons", "sv_noallyicons",
         "No icons are drawn over the heads of allies."},
        {zadmflags, 1U << 28U, ZA, online, "No enemy icons", "sv_noenemyicons",
         "No icons are drawn over the heads of enemies."},

        // --- last man standing ---
        {lmsSpectators, 1U << 0U, ZA, lastManStanding, "Spectators can chat", "lms_spectatorchat",
         "Spectators may talk to players during a round."},
        {lmsSpectators, 1U << 1U, ZA, lastManStanding, "Spectators can watch", "lms_spectatorview",
         "Spectators may watch the players still in a round."},
        {lmsSpectators, 1U << 2U, ZA, lastManStanding, "Spectators can voice chat", "lms_spectatorvoicechat",
         "Spectators may use voice chat with players during a round."},
        {lmsWeapons, 1U << 9U, ZA, lastManWeapons, "Chainsaw", "lms_allowchainsaw",
         "Players get the chainsaw in last man standing."},
        {lmsWeapons, 1U << 0U, ZA, lastManWeapons, "Pistol", "lms_allowpistol",
         "Players get the pistol in last man standing."},
        {lmsWeapons, 1U << 1U, ZA, lastManWeapons, "Shotgun", "lms_allowshotgun",
         "Players get the shotgun in last man standing."},
        {lmsWeapons, 1U << 2U, ZA, lastManWeapons, "Super shotgun", "lms_allowssg",
         "Players get the super shotgun in last man standing."},
        {lmsWeapons, 1U << 3U, ZA, lastManWeapons, "Chaingun", "lms_allowchaingun",
         "Players get the chaingun in last man standing."},
        {lmsWeapons, 1U << 4U, ZA, lastManWeapons, "Minigun", "lms_allowminigun",
         "Players get the minigun in last man standing."},
        {lmsWeapons, 1U << 5U, ZA, lastManWeapons, "Rocket launcher", "lms_allowrocketlauncher",
         "Players get the rocket launcher in last man standing."},
        {lmsWeapons, 1U << 6U, ZA, lastManWeapons, "Grenade launcher", "lms_allowgrenadelauncher",
         "Players get the grenade launcher in last man standing."},
        {lmsWeapons, 1U << 7U, ZA, lastManWeapons, "Plasma rifle", "lms_allowplasma",
         "Players get the plasma rifle in last man standing."},
        {lmsWeapons, 1U << 8U, ZA, lastManWeapons, "Railgun", "lms_allowrailgun",
         "Players get the railgun in last man standing."},

        // --- compatflags ---
        {compatflags, 1U << 25U, MODERN, actors, "Arch-vile ghosts", "compat_vileghosts",
         "An Arch-vile can raise crushed monsters as ghosts that cannot be hurt."},
        {compatflags, 1U << 25U, ZD | ZA, actors, "Crushed monsters turn to gibs", "compat_corpsegibs",
         "A crushed monster that bleeds turns into the generic gibs."},
        {compatflags, 1U << 26U, FAMILY, actors, "Friends ignore monster blocking lines", "compat_noblockfriends",
         "Friendly monsters walk through lines that block monsters."},
        {compatflags, 1U << 2U, FAMILY, actors, "Limit lost souls", "compat_limitpain",
         "A pain elemental spits no lost soul while 20 or more are alive."},
        {compatflags, 1U << 24U, FAMILY, actors, "Monsters move with friction", "compat_mbfmonstermove",
         "Ice, mud and pushers move monsters as they do in MBF."},
        {compatflags, 1U << 20U, FAMILY, actors, "Monsters cannot be pushed off ledges", "compat_crossdropoff",
         "Nothing pushes a monster off a ledge it stands on."},
        {compatflags, 1U << 14U, FAMILY, actors, "Monsters stick over dropoffs", "compat_dropoff",
         "A monster hanging over a dropoff cannot move off it."},
        {compatflags, 1U << 16U, FAMILY, actors, "Monsters see invisible players", "compat_invisibility",
         "Partial invisibility does not throw off a monster's aim."},
        {compatflags, 1U << 22U, FAMILY, actors, "No minotaur flames in water", "compat_minotaur",
         "The minotaur floor fire goes off at once when the minotaur stands in water."},
        {compatflags, 1U << 7U, FAMILY, actors, "Drops land on the floor", "compat_notossdrops",
         "Items a monster drops appear on the floor instead of being tossed."},
        {compatflags, 1U << 12U, FAMILY, dehacked, "DeHackEd health like Doom 2", "compat_dehhealth",
         "Health bonuses stop at the DeHackEd max health value, as doom2.exe reads it."},
        {compatflags, 1U << 23U, FAMILY, dehacked, "Original A_Mushroom speed", "compat_mushroom",
         "A_Mushroom in DeHackEd states throws its projectiles at the original speed."},
        {compatflags, 1U << 8U, FAMILY, mapActions, "Any special line blocks use", "compat_useblocking",
         "A special line stops the use action from reaching a line behind it, as in Doom."},
        {compatflags, 1U << 21U, FAMILY, mapActions, "Any boss death ends the map special", "compat_anybossdeath",
         "Any monster that calls A_BossDeath triggers the boss special, not only the bosses."},
        {compatflags, 1U << 9U, FAMILY, mapActions, "No Boom door light", "compat_nodoorlight",
         "Tagged doors do not change the light of the sector."},
        {compatflags, 1U << 29U, FAMILY, mapActions, "Find light levels like Doom", "compat_light",
         "Light specials look for neighbouring light levels the way Doom does."},
        {compatflags, 1U << 0U, FAMILY, mapActions, "Shortest texture like Doom", "compat_shorttex",
         "A missing lower texture counts as the shortest when a special looks for one."},
        {compatflags, 1U << 1U, FAMILY, mapActions, "Buggy stair building", "compat_stairs",
         "Stairs build with the loop bug from Doom."},
        {compatflags, 1U << 4U, FAMILY, physics, "Actors are infinitely tall", "compat_nopassover",
         "Nothing can move over or under another actor."},
        {compatflags, 1U << 15U, FAMILY, physics, "Boom scrollers add up", "compat_boomscroll",
         "Several Boom scrollers under an actor add their pushes together."},
        {compatflags, 1U << 6U, FAMILY, physics, "Wall running", "compat_wallrun",
         "Wall collision works as in Doom, so players can wall run."},
        {compatflags, 1U << 10U, FAMILY, physics, "Raven scrollers at original speed", "compat_ravenscroll",
         "Heretic and Hexen scrolling floors push at their original speed."},
        {compatflags, 1U << 13U, FAMILY, physics, "Self referencing sectors do not block shots", "compat_trace",
         "Shots and sight pass through lines with the same sector on both sides."},
        {compatflags, 1U << 28U, FAMILY, physics, "Doom hitscan checks", "compat_hitscan",
         "Hitscan attacks find what they hit with the original Doom code."},
        {compatflags, 1U << 19U, FAMILY, physics, "Doom heights for projectiles", "compat_missileclip",
         "Projectiles hit monsters by their original heights."},
        {compatflags, 1U << 30U, FAMILY, rendering, "Polyobjects like Hexen", "compat_polyobj",
         "Polyobjects are drawn the old way in the software renderer."},
        {compatflags, 1U << 31U, FAMILY, rendering, "Masked middle textures like Doom", "compat_maskedmidtex",
         "Masked middle textures ignore their vertical offsets, as in Doom."},
        {compatflags, 1U << 27U, FAMILY, rendering, "Sprite sorting like Doom", "compat_spritesort",
         "Sprites at the same distance are drawn in the reverse order, as in Doom."},
        {compatflags, 1U << 5U, FAMILY, sound, "One sound per actor", "compat_soundslots",
         "An actor plays one sound at a time, which allows the silent BFG trick."},
        {compatflags, 1U << 3U, FAMILY, sound, "Silent pickups for others", "compat_silentpickup",
         "Only the player who picks something up hears it."},
        {compatflags, 1U << 17U, FAMILY, sound, "Instant floors make sound", "compat_silentinstantfloors",
         "Floors that move at once play their sound, as in Doom."},
        {compatflags, 1U << 18U, FAMILY, sound, "Sector sounds from the centre", "compat_sectorsounds",
         "A sector plays its sounds from its centre instead of the nearest point."},
        {compatflags, 1U << 11U, FAMILY, sound, "Sound targets like Doom", "compat_soundtarget",
         "Monsters hear the player through the sector sound target, as in Doom."},

        // --- compatflags2 ---
        {compatflags2, 1U << 12U, MODERN, actors, "Monsters avoid crushers", "compat_avoidhazard",
         "Monsters keep away from crushing ceilings, as in MBF."},
        {compatflags2, 1U << 13U, MODERN, actors, "Monsters stay on lifts", "compat_stayonlift",
         "Monsters tend to stay on a lift they stand on, as in MBF."},
        {compatflags2, 1U << 15U, MODERN, actors, "Voodoo zombies", "compat_voodoozombies",
         "Voodoo dolls keep their own health, and the death of one does not kill the player."},
        {compatflags2, 1U << 1U, FAMILY, mapActions, "Floors move like Doom", "compat_floormove",
         "Floors and ceilings move as in Doom, even through each other."},
        {compatflags2, 1U << 3U, ZDOOMS, mapActions, "Doom point on line test", "compat_pointonline",
         "Uses the slightly wrong test Doom has for the side of a line."},
        {compatflags2, 1U << 4U, MODERN, mapActions, "Exits trigger more than once", "compat_multiexit",
         "A map exit can be triggered again while the map is ending."},
        {compatflags2, 1U << 5U, MODERN, mapActions, "Indirect teleports skip sector actions", "compat_teleport",
         "Scripted and other indirect teleports do not trigger sector actions."},
        {compatflags2, 1U << 16U, MODERN, mapActions, "Final Doom teleporters", "compat_fdteleport",
         "Teleporters keep the height glitch from Final Doom."},
        {compatflags2, 1U << 6U, MODERN | ZA, mapActions, "Push specials skip the window check", "compat_pushwindow",
         "Push specials work through lines that do not block, as in Doom."},
        {compatflags2, 1U << 7U, MODERN, mapActions, "Buggy switch range", "compat_checkswitchrange",
         "Switches keep the range check bug of older versions."},
        {compatflags2, 1U << 10U, MODERN, mapActions, "Buggy Strife railings", "compat_railing",
         "Strife railings behave as they did in the original game."},
        {compatflags2, 1U << 14U, GZ, mapActions, "No MBF21 line features", "compat_nombf21",
         "Doom format maps get none of the MBF21 line features."},
        {compatflags2, 1U << 18U, UZ, mapActions, "No lock message for voodoo dolls", "compat_novdolllockmsg",
         "A voodoo doll that touches a locked line shows no message."},
        {compatflags2, 1U << 20U, UZ, mapActions, "Ignore the reserved line flag", "compat_reservedlineflag",
         "Line flag bits that some maps set by mistake are ignored."},
        {compatflags2, 1U << 21U, UZ, mapActions, "Floor transfers carry the secret", "compat_transfersecret",
         "Boom floor transfer specials also pass on the secret flag."},
        {compatflags2, 1U << 0U, FAMILY, physics, "No exact compass angles", "compat_badangles",
         "Players cannot face exactly north, south, east or west, as in Doom."},
        {compatflags2, 1U << 8U, MODERN, physics, "Explosions without vertical thrust", "compat_explode1",
         "Explosions push only sideways."},
        {compatflags2, 1U << 9U, MODERN, physics, "Explosions like Doom", "compat_explode2",
         "Explosions use the original Doom code throughout, infinite height included."},
        {compatflags2, 1U << 19U, UZ, physics, "Mikoportals", "compat_emulatemikoportals",
         "Keeps the height underflow that Mikoportals rely on."},
        {compatflags2, 1U << 2U, ZDOOMS, sound, "Sounds stop with their actor", "compat_soundcutoff",
         "A sound stops when the actor that made it is removed."},
        {compatflags2, 1U << 11U, MODERN, scripts, "Script wait skips idle scripts", {},
         "ACS script_wait does not wait for a script that has not started yet."},
        {compatflags2, 1U << 17U, MODERN, scripts, "No ACS argument check", {},
         "ACS functions skip the check on how many arguments they get."},

        // --- zacompatflags ---
        {zacompatflags, 1U << 23U, ZA, actors, "No stealth monsters", "compat_disablestealthmonsters",
         "Stealth monsters are always visible, as in doom2.exe."},
        {zacompatflags, 1U << 4U, ZA, actors, "Floating spheres", "compat_nogravity_spheres",
         "Invulnerability, soul, mega and blur spheres float, as in Skulltag."},
        {zacompatflags, 1U << 3U, ZA, physics, "Doom random numbers", "compat_oldrandom",
         "Uses the random number table of Doom instead of the ZDoom generator."},
        {zacompatflags, 1U << 6U, ZA, physics, "Old explosion thrust", "compat_explosionthrust",
         "Explosions push sideways as in old ZDoom versions."},
        {zacompatflags, 1U << 24U, ZA, physics, "Infinitely tall splash damage", "compat_oldradiusdmg",
         "Splash damage reaches any height, as in Doom."},
        {zacompatflags, 1U << 7U, ZA, physics, "Drops fall through bridges", "compat_bridgedrops",
         "Things that do not block, such as flags, fall through invisible bridges."},
        {zacompatflags, 1U << 8U, ZA, physics, "Old vertical movement", "compat_oldzdoomzmovement",
         "Jumping and gravity work as in old ZDoom versions."},
        {zacompatflags, 1U << 12U, ZA, physics, "Skulltag jumping", "compat_skulltagjumping",
         "Jumping works as it did in Skulltag."},
        {zacompatflags, 1U << 17U, ZA, physics, "Limit air movement", "compat_limited_airmovement",
         "Players have less control while in the air."},
        {zacompatflags, 1U << 18U, ZA, physics, "Grab items like vanilla", "compat_plasmabump",
         "Players can grab items through walls with the plasma bump, as in vanilla."},
        {zacompatflags, 1U << 9U, ZA, physics, "Weapons lower fully", "compat_fullweaponlower",
         "A weapon lowers all the way before the next one comes up, as in vanilla."},
        {zacompatflags, 1U << 10U, ZA, physics, "Autoaim with gaps", "compat_autoaim",
         "Autoaim uses the three vanilla traces, which leave gaps up and down."},
        {zacompatflags, 1U << 26U, ZA, physics, "Switch weapons on pickup", "compat_oldweaponswitch",
         "Players always switch to a weapon they pick up, as in vanilla."},
        {zacompatflags, 1U << 19U, ZA, physics, "Respawn instantly", "compat_instantrespawn",
         "With forced respawn on, dead players come back at once."},
        {zacompatflags, 1U << 22U, ZA, rendering, "Old intermission screens", "compat_oldintermission",
         "Uses the intermission screens and music of doom2.exe."},
        {zacompatflags, 1U << 11U, ZA, sound, "West facing spawns are silent", "compat_silentwestspawns",
         "A player who spawns facing west makes no teleport sound, as in vanilla."},
        {zacompatflags, 1U << 21U, ZA, sound, "Original sound curve", "compat_originalsoundcurve",
         "Meant to use the sound curve of doom2.exe, but Zandronum never reads this bit."},
        {zacompatflags, 1U << 0U, ZA, scripts, "NET scripts run on clients", "compat_netscriptsareclientside",
         "ACS scripts marked NET run on the clients and not on the server."},
        {zacompatflags, 1U << 5U, ZA, scripts, "Player scripts outlive a disconnect",
         "compat_dont_stop_player_scripts_on_disconnect",
         "Scripts of a player who leaves keep running."},
        {zacompatflags, 1U << 13U, ZA, scripts, "Reset global ACS variables", "compat_resetglobalvarsonmapreset",
         "World and global ACS variables start over when the map resets."},
        {zacompatflags, 1U << 1U, ZA, online, "Clients send every button", "compat_clientssendfullbuttoninfo",
         "Clients send all their buttons to the server, which GetPlayerInput needs for the higher ones."},
        {zacompatflags, 1U << 2U, ZA, online, "No land command", "compat_noland",
         "The land console command is refused."},
        {zacompatflags, 1U << 14U, ZA, online, "No obituaries", "compat_noobituaries",
         "Messages about who killed whom are not printed."},
        {zacompatflags, 1U << 20U, ZA, online, "No taunts", "compat_disabletaunts",
         "The taunt command is refused."},
        {zacompatflags, 1U << 25U, ZA, online, "No crosshairs", "compat_nocrosshair",
         "Clients cannot draw a crosshair."},

});
// NOLINTEND(readability-trailing-comma)
// clang-format on

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

constexpr std::size_t SETS = static_cast<std::size_t>(Set::eternity) + 1;

// The bits each set gives a meaning to, per word.
constexpr std::array<std::array<std::uint32_t, WORDS>, SETS> KNOWN = [] {
    std::array<std::array<std::uint32_t, WORDS>, SETS> out{};

    for (const Option &each : OPTIONS) {
        for (std::size_t set = 0; set < SETS; ++set) {
            if ((each.sets & bit(static_cast<Set>(set))) != 0) {
                out[set][static_cast<std::size_t>(each.word)] |= each.mask;
            }
        }
    }

    return out;
}();

constexpr std::array UZDOOM_PRESETS = {
        Preset{"Default", 0U, 0},
        Preset{"Doom", 2820160259U, 1048834},
        Preset{"Doom (strict)", 3122215895U, 1606155},
        Preset{"Boom", 2148050944U, 3145984},
        Preset{"Boom (strict)", 2416552144U, 3670536},
        Preset{"MBF", 2240325632U, 1061120},
        Preset{"MBF (strict)", 2508826832U, 3158792},
        Preset{"MBF21", 2241374208U, 3158272},
        Preset{"MBF21 (strict)", 2509875408U, 3683080},
        Preset{"ZDoom 2.0.63", 536872960U, 3145968},
};

constexpr std::array GZDOOM_PRESETS = {
        Preset{"Default", 0U, 0},
        Preset{"Doom", 2820160259U, 16642},
        Preset{"Doom (strict)", 3122215895U, 49675},
        Preset{"Boom", 2148050944U, 16640},
        Preset{"Boom (strict)", 2416552144U, 16904},
        Preset{"MBF", 2240325632U, 28928},
        Preset{"MBF (strict)", 2508826832U, 29448},
        Preset{"MBF21", 2241374208U, 12544},
        Preset{"MBF21 (strict)", 2509875408U, 13064},
        Preset{"ZDoom 2.0.63", 536872960U, 16624},
};

constexpr std::array ZDOOM_PRESETS = {
        Preset{"Default", 0, 0},
        Preset{"Doom", 672676611U, 2},
        Preset{"Doom (strict)", 3122215895U, 11},
        Preset{"Boom", 567296U, 0},
        Preset{"Boom (strict)", 302622928U, 8},
        Preset{"MBF", 92841984U, 0},
        Preset{"ZDoom 2.0.63", 536872960U, 0},
};

constexpr std::array ZANDRONUM_PRESETS = {
        Preset{"Default", 0, 0},
        Preset{"Doom", 672676611U, 2},
        Preset{"Doom (strict)", 3122215895U, 3},
        Preset{"Boom", 567296U, 0},
        Preset{"Boom (strict)", 302622928U, 0},
        Preset{"MBF", 92841984U, 0},
        Preset{"ZDoom 2.0.63", 536872960U, 64},
};

}

const WordInfo &info(const Word word) {
    return WORD_INFO[static_cast<std::size_t>(word)];
}

std::string_view title(const Group group) {
    return TITLES[static_cast<std::size_t>(group)];
}

bool reads(const Set set, const Word word) {
    return (info(word).sets & bit(set)) != 0;
}

std::span<const Option> options() {
    return GROUPED;
}

std::uint32_t known(const Set set, const Word word) {
    return KNOWN[static_cast<std::size_t>(set)][static_cast<std::size_t>(word)];
}

std::span<const Preset> presets(const Set set) {
    switch (set) {
        case Set::uzdoom:
            return UZDOOM_PRESETS;
        case Set::gzdoom:
            return GZDOOM_PRESETS;
        case Set::zdoom:
            return ZDOOM_PRESETS;
        case Set::zandronum:
            return ZANDRONUM_PRESETS;
        case Set::none:
        case Set::eternity:
            break;
    }

    return {};
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

std::string spelled(const Set set, const std::uint32_t value) {
    // ZDoom 2.8 and Zandronum read it with strtol, which caps at 2^31 - 1 where long is 32 bits, as on Windows.
    // The same 32 bits as a negative number come through whole.
    if ((set == Set::zdoom || set == Set::zandronum) && value > std::numeric_limits<std::int32_t>::max()) {
        return std::to_string(static_cast<std::int32_t>(value));
    }

    return std::to_string(value);
}

std::string switchFor(const Set set, const Word word) {
    return (set == Set::eternity ? "-" : "+") + std::string(info(word).name);
}

}
