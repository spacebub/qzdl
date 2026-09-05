# ZDL

A launcher for Doom engine source ports. Pick a port, pick a game, add whatever you want on top of it, and launch.

![The library](assets/screenshots/library.png)

## Install

Download the build for your system from the
[releases page](https://github.com/spacebub/qzdl/releases).

ZDL is only the launcher. The source port and the game data (IWADs/IPK3s) are yours to get separately.

## Quick start

1. Open **Settings** and add your source port executables and your IWADs. Each
   one is named after the file it points at.
2. Back on the library, press **New profile**, or click a game to play it as it
   is.
3. Pick a source port and a game, add any PWADs or patches under **Add-ons**,
   then press **Launch**.

Add-ons reach the source port in the order they are listed, and each one can be
switched off without being removed.

## Profiles

A profile is a saved set of launch options: port, game, add-ons, map, skill,
multiplayer and the rest. Nothing ties it to a particular game, so it can be a
game, a single mod, a multiplayer setup or anything else worth coming back to.
The heading at the top of the page is how you switch between them.

## Per profile port settings

Everything above is what ZDL hands the source port. The port's own settings,
controls, video and sound, normally live in one file it shares between every
launch.

Turn on **A config file per profile** in Settings and each profile is given a
config file of its own instead. One profile can opt back out with **Use the
port's own settings** on its page.

## Per profile port settings

A profile tries to set a port's saves directory to its own configuration dir
in  the current OS' data dir. See [Where things are kept](#Where-things-are-kept).

## DOS ports

A source port can be marked as a DOS program when it is added or edited, and is
then launched inside DOSBox. Point Settings at a DOSBox, or leave it empty and
whichever one the machine already has is used.

## Where things are kept

|                          | Linux                 | Windows          |
|--------------------------|-----------------------|------------------|
| Settings (`zdl.json`)    | `~/.config/qzdl`      | `%APPDATA%\qZDL` |
| Per profile port configs | `~/.local/share/qzdl` | `%APPDATA%\qZDL` |

A `zdl.json` next to the executable is used when there is no per user config,
which is what keeps a portable setup portable. A config from an older ZDL is
found in its old location and converted on first run.

Single launch configurations can also be exported to and imported from `.zdl`
files, which other Doom tools read too.

## Building

See [COMPILE.md](COMPILE.md).

## License

Copyright (c) 2023-2026 spacebub  
Copyright (c) 2018-2019 Lcferrum  
Copyright (c) 2004-2012 ZDL Software Foundation  

GNU General Public License, version 3 or later. See [LICENSE](LICENSE).

Uses the miniz and yyjson libraries. See [AUTHORS](AUTHORS).
