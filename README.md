# ZDL4

[![Tests](https://github.com/spacebub/qzdl/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/spacebub/qzdl/actions/workflows/ci.yml)
[![Release](https://github.com/spacebub/qzdl/actions/workflows/release.yml/badge.svg)](https://github.com/spacebub/qzdl/actions/workflows/release.yml)
[![GitHub release](https://img.shields.io/github/v/release/spacebub/qzdl)](https://github.com/spacebub/qzdl/releases/latest)
[![winget](https://img.shields.io/winget/v/spacebub.qzdl)](https://github.com/microsoft/winget-pkgs/tree/master/manifests/s/spacebub/qzdl)

A launcher for Doom engine source ports. Pick a port, pick a game, add whatever
you want on top of it, and launch.

![The library](assets/screenshots/library.png)

## Install

Grab the build for your system from the
[releases page](https://github.com/spacebub/qzdl/releases), or on Windows:

```
winget install spacebub.qzdl
```

ZDL4 is only the launcher. The source port and the game data (IWADs/IPK3s) are
yours to get separately.

## Quick start

1. Open **Engines** and add the source ports you have, or fetch one under
   **Get more**. Games go on the games shelf in the library.
2. On the library, press **New profile**, or click a game to play it as it is.
3. Pick a port and a game, add PWADs or patches under **Add-ons**, and press
   **Launch**.

Add-ons load in the order listed, and each can be switched off without being
removed. A folder works as an add-on too, which is how an unpacked mod is
loaded. In the file picker, click a folder row to open it, or its right edge to
pick it.

## Engines

**Installed** lists the ports in this config, plus any found on the machine.
From a port card you can rename the port, point it at another file, open it in
a file manager or remove it.

**Get more** lists the ports ZDL4 can fetch. These are the ports it knows best.
**Install** gets the build for this system, unpacks it and adds it to the
installed ones. **Update** appears once a newer release is out, and profiles
stay pointed at the port. Removing a fetched port deletes what was unpacked. A
DOS port is marked as one and launches inside DOSBox.

Releases are looked up on GitHub when the page opens. Nothing else is sent
anywhere, and nothing is downloaded until asked for. A port with no build for
this system says so and links to the project page.

Before unpacking, every download is checked against the size and SHA-256
checksum from the release, or a checksum built into ZDL4 for the few ports
fetched elsewhere. A mismatch is thrown away. That catches a corrupt or swapped file,
not a compromised project. Downloads are kept and reused on a reinstall, checked
again each time. Settings shows how much room they take.

## Profiles

A profile is a saved launch: port, game, add-ons, map, skill, multiplayer and
the rest. It can be a game, a single mod, a multiplayer setup or anything else
worth coming back to. Switch between them from the heading at the top of the
page.

The profile card shows a title screen: the one from the last add-on that has
one, or the game title otherwise.

## Per profile port settings

Port settings such as controls, video and sound normally live in one file
shared by every launch. Turn on **A config file per profile** in Settings to
give each profile a separate file. A profile can opt out again with **Use the
port's own settings** on the profile page.

## Saves directory

Where the port allows it, each profile saves into a separate folder under the
data directory. See [Where things are kept](#where-things-are-kept).

## Custom launch commands

The profile page builds the command line. If the page cannot describe the
launch you want, turn on **Override** under Command line and write the command
yourself. These words stand for
what the page already knows, so no path needs typing or upkeep:

| Word            | Stands for                                             |
|-----------------|--------------------------------------------------------|
| `{source_port}` | the source port this profile is set to                 |
| `{game}`        | the game this profile is set to                        |
| `{addon_n}`     | an add-on from the list, `{addon_1}` up to `{addon_n}` |
| `{profile}`     | the profile folder, which holds the three below        |
| `{cfgdir}`      | the port config written for this profile               |
| `{extracfg}`    | a vanilla port's second config, `-extra` on the name   |
| `{savedir}`     | the saves folder, `{profile}/saves`                    |

A word with nothing to fill it, such as `{addon_3}` on a profile with two
add-ons, is flagged under the field, and the profile will not launch until it
is fixed. Everything else is passed on as typed, so the launch can be wrapped
in anything the system has: `{source_port} -iwad {game}`. The field starts with
the command ZDL4 would run, written with these words, so you edit a line rather
than start from blank.

## DOS ports

Mark a port as a DOS program when adding or editing it and it launches inside
DOSBox. The DOS ports ZDL4 fetches are marked already. Point Settings at a
DOSBox, or leave it empty to use whichever one the machine has.

The port runs from the folder it lives in, as under DOS, so a batch file that
starts a port works too. When the port has to find the game itself, ZDL4 passes
the folder through `DOOMWADDIR`, and copies the game under the expected name if
the file is called something else. A DOS profile runs DOSBox full screen unless
**Full screen** is off on the profile page, and closes DOSBox with the port
unless **Auto close** is off.

## Where things are kept

|                             | Linux                              | Windows                       |
|-----------------------------|------------------------------------|-------------------------------|
| Settings (`zdl.json`)       | `~/.config/qzdl`                   | `%APPDATA%\qZDL`              |
| Per profile port configs    | `~/.local/share/qzdl/profiles`     | `%APPDATA%\qZDL\profiles`     |
| Fetched source ports        | `~/.local/share/qzdl/source_ports` | `%APPDATA%\qZDL\source_ports` |
| What they were fetched from | `~/.local/share/qzdl/downloads`    | `%APPDATA%\qZDL\downloads`    |

If there is no per-user config, ZDL4 uses a `zdl.json` next to the executable,
so a portable setup stays portable. On first run, a config from an older ZDL is
picked up from the old location and converted. A single launch can be exported to
and imported from a `.zdl` file.

## Building

See [COMPILE.md](COMPILE.md).

## License

Copyright (c) 2023-2026 spacebub  
Copyright (c) 2018-2019 Lcferrum  
Copyright (c) 2004-2012 ZDL Software Foundation  

GNU General Public License, version 3. See [LICENSE](LICENSE).

Uses the SDL, Blend2D, miniz, stb, md5, sha256 and yyjson libraries. See [AUTHORS](AUTHORS.md).
