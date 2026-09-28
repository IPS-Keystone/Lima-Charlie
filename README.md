# Lima Charlie

TeamSpeak 3 voice integration for Arma Reforger: positional direct speech, and radios that garble with
distance and terrain instead of cutting out cleanly.

Two halves that must be installed and updated together — the Enfusion addon and a TeamSpeak 3 plugin,
bridged by two JSON files in the Reforger profile directory. Neither works alone.

## Features

- **Positional direct speech** with three voice levels — whisper 5 m, normal 20 m, shout 60 m — panned to
  the listener's facing, attenuated from behind, and muffled by walls and vehicles.
- **Radios that degrade rather than cut out.** Clear out to a share of the radio's range, then progressively
  garbled to the edge: radio band-pass, distortion, sample-and-hold, bit-crushing, dropouts and rising hiss,
  all driven off one signal-quality figure.
- **Terrain interception** on TFAR's model, so a ridge costs you range.
- **Per-radio settings** — ear (left, right, both), transmission beep set, volume, and which of four
  transmit keys sends on it.
- **Named channels** with colours, defined server-side, shown on the VON display.
- **Half duplex** as a per-prefab option, for sets that go deaf while transmitting.
- **AI hearing** — enemy AI turn towards players speaking out loud, with walls shortening how far a voice
  carries.
- **Game Master support** — the editor camera is the listener, and a Game Master can be given unlimited
  radio range.

## Documentation

Everything lives in [docs/](docs):

| Page | For |
| --- | --- |
| [installation.md](docs/installation.md) | Getting the plugin installed and TeamSpeak configured |
| [server-setup.md](docs/server-setup.md) | Standing up a server: TeamSpeak channel, mod, settings |
| [player-guide.md](docs/player-guide.md) | Controls, radios, what you hear and why |
| [server-settings.md](docs/server-settings.md) | Every server setting and how to set it |
| [how-it-works.md](docs/how-it-works.md) | The audio model, terrain, AI hearing, Game Master |
| [developer-reference.md](docs/developer-reference.md) | Bridge protocols, script map, building the plugin |
| [troubleshooting.md](docs/troubleshooting.md) | Diagnostics and known problems |

## Repository layout

```
addon/    the Enfusion addon, published to the Steam Workshop
plugin/   the TeamSpeak 3 plugin: C sources, offline tests, sound sets, build script
docs/     documentation
```

## Building the plugin

Needs Visual Studio 2022 Build Tools with the "Desktop development with C++" workload, and Python.

```
plugin\build.bat
```

That compiles `limacharlie_win64.dll`, compiles and runs the offline tests, and packages
`plugin/build/LimaCharlie_<version>.ts3_plugin` ready to install. A failing test stops the build. Nothing in
`plugin/build/` is tracked here — released packages belong on the Releases page.

## Credits

The transmission beep sound sets are not original work and are included under their own licences, which
ship alongside them in [plugin/sounds/](plugin/sounds):

- **Task Force Arrowhead Radio** (`tfar_sw`, `tfar_lr`, `tfar_ab`, `tfar_classic`) — © 2013 Michail
  Nikolaev, under the [Arma Public License Share Alike](https://www.bistudio.com/community/licenses/arma-public-license-share-alike).
  See `plugin/sounds/LICENSE_TFAR.txt`.
- **ACRE2** (`acre`) — Advanced Combat Radio Environment, under GPL-3.0. See
  `plugin/sounds/LICENSE_ACRE2.txt`.

The voice level icons are TFAR artwork under the same APL-SA licence.

[cJSON](https://github.com/DaveGamble/cJSON) is vendored in `plugin/third_party/cjson` under the MIT
licence. The TeamSpeak 3 plugin SDK headers in `plugin/third_party/ts3sdk` are TeamSpeak Systems GmbH's.

Design owes a great deal to TFAR and ACRE2, whose trust model and terrain maths this follows.
