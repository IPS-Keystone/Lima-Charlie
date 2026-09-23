# Developer reference

## Layout

```
ArmaReforgerWorkbench/addons/Lima Charlie/   the published mod, nothing else
  Configs/  Missions/  Scripts/  UI/  worlds/  addon.gproj

~/Lima Charlie/
  plugin/      TeamSpeak plugin: src, test, third_party, sounds, build.bat, package.py
  reference/   unpacked mods, ACRE2/TFAR PBOs, vanilla dumps, RESEARCH_NOTES.md
  docs/        this documentation
```

**Nothing but addon content may live in the addon folder.** Workbench packs `.c` files from anywhere inside
it, so the plugin's C sources were previously ending up in `data.pak` on every publish. Reference material
is kept out for a second reason: Workbench indexes it, and cloned `.meta` files hijack resource GUIDs.

## Building the plugin

```bash
"~/Lima Charlie/plugin/build.bat"
```

Needs Visual Studio 2022 Build Tools with the C++ workload; the script finds it through `vswhere` if `cl`
is not already on the path. It compiles the DLL, compiles and **runs** the offline tests, and packages
`build/LimaCharlie_<version>.ts3_plugin`. A failing test stops the build.

`build.bat` does `pushd "%~dp0"`, so it is path-independent and can be moved anywhere.

Version and protocol live in `src/lc_version.h`; see **Versioning and releases** below for what to bump.

`package.py` builds the `.ts3_plugin` zip with directory entries and DOS file attributes, mimicking TFAR's
package — TeamSpeak's installer extracted 0-byte files from the plain zip .NET produced.

## Versioning and releases

Version numbers matter for the **plugin**, not the addon: the Reforger Workshop keeps every published
version of a mod, so the addon's history is recoverable from there. The plugin has no such store, and it
has to be matched to the mod it is bridged to, so it carries the scheme.

`MAJOR.STABLE.DEV`, in `plugin/src/lc_version.h`:

| Position | Bumped for | Example |
| --- | --- | --- |
| **Major** | The first public release, and each major feature afterwards | 1.0.0 → 2.0.0 for electronic warfare |
| **Stable** | Bug fixes and small feature changes | 1.0.0 → 1.1.0 |
| **Dev** | Testing new features or changes | 1.1.0 → 1.1.1 |

The first public release is **1.0.0**.

New features are developed on their own GitHub branch, never straight onto `main`. The branch merges to
`main` as part of the major release that introduces the feature — so `main` only ever moves from one
released state to the next, and a half-finished feature never sits in it.

`LC_PROTOCOL_VERSION` is independent of all three and only moves when the bridge format changes
incompatibly; it must be bumped together with `LC_GameStateWriter.PROTOCOL_VERSION` on the game side. A
plugin rejects a game state whose protocol it does not recognise outright, so the two halves must ship
together whenever it changes.

## The bridge

Both files live in `$profile/LimaCharlie/`.

### `game_state.json` — game to plugin

Written at 20 Hz while anyone is talking, 10 Hz idle, and immediately on any change to transmit state,
voice range, terrain links or the sound queue. Radio state is rebuilt on the write itself rather than
compared every frame, so a radio setting reaches the plugin at the next scheduled write — at most 100 ms,
and in practice sooner, since the settings that change it all queue a sound, which forces a write.
Protocol 8.

```json
{
  "v": 8, "seq": 42, "inGame": true,
  "session": { "token": "...", "playerId": 1, "playerName": "Name",
               "tsChannel": "LimaCharlie", "tsChannelPassword": "",
               "modVersion": "1.0.0" },
  "self": { "alive": true, "pos": [x,y,z], "dir": [x,y,z], "tx": 2,
            "txFrequency": 45000, "txRadio": "123:1", "voiceRange": 20,
            "cleanFraction": 0.35, "beepFraction": 0.9, "unlimitedRx": false,
            "roomVolume": 96,
            "radios": [ { "id": "123:1", "freq": 45000, "range": 1500, "key": "US",
                          "rx": true, "ear": 1, "volume": 0.8, "beep": "tfar_sw",
                          "halfDuplex": 0 } ],
            "sounds": [ { "seq": 3, "set": "ui", "name": "deny", "ear": 0, "volume": 1 } ] },
  "players": [ { "id": 2, "alive": true, "pos": [x,y,z], "muffle": 0.6, "room": 0.35 } ],
  "links":   [ { "id": 2, "clearance": 35 } ]
}
```

- `self.pos`/`dir` are the **listener** (camera). `players[].pos` is the speaker's head.
- `self.alive` means "can hear anything", not literally alive — it is true for an unconscious player and
  for a bodiless Game Master.
- `tx` mirrors `EVONTransmitType`: 0 none, 1 direct, 2 channel, 3 long range.
- `radios[].id` is `<radio RplId>:<transceiver number>`, unique among the player's own radios.
- `radios[].range` is the announced range, which is 1000000 for a Game Master with the editor open.
- `players[]` holds only players within 60 m; `muffle` is 0 clear to 1 fully obstructed. `room` is how
  much of the listener's room that voice fills, which scales the reverb send only: 1 in the same room,
  0.35 elsewhere in the building, 0.1 from outdoors. New in protocol 8.
- `links[]` answers the plugin's `radioRx` requests with terrain clearance in metres, already scaled by the
  server's terrain setting.
- `sounds[]` is a queue with increasing `seq`; the plugin plays each once.
- `roomVolume` is the volume in m³ of the room the listener is standing in, 0 outdoors. It sizes the
  plugin's room reverb. New in protocol 8.

### `plugin_state.json` — plugin to game

Written atomically at 4 Hz or faster.

```json
{ "v": 8, "seq": 971, "gameSeq": 445, "pluginVersion": "1.0.6", "inGame": false,
  "tsConnected": true, "tsClientId": 3, "inGameChannel": false, "peers": 0,
  "selfTalking": true, "talking": "", "radioRx": "", "radioHeard": "" }
```

- `talking` — semicolon-delimited player ids currently speaking.
- `radioRx` — `playerId,x,y,z;` requests for terrain clearance the game should answer.
- `radioHeard` — `playerId,radioId,frequency,quality;` for every transmission actually being heard.

### `server.json` — server overrides

Holds every server setting under a plain name: `teamspeakChannel`, `teamspeakChannelPassword`,
`cleanRangePercent`, `beepRangePercent`, `terrainEffectPercent`, `gameMasterUnlimitedRange`, `aiHearing`,
`diagnosticLog`, `roomDiagnosticLog`, `channelNaming`, `channelLabels`.

Read last, after the defaults, `Settings.conf` and the mission header, and it wins over all of them because
it is the only layer an operator can change without republishing. Applied key by key through
`DoesKeyExist`, so a file with one key overrides one setting. Written out in full by `WriteTemplate()` the
first session it is missing. See [server-settings.md](server-settings.md) for the key formats.

## Plugin commands

Between plugins, over the TeamSpeak channel.

```
LC1|HELLO|<token>|<playerId>|<voiceRange>|<pluginVersion>|<modVersion>
LC1|RTX|<token>|<playerId>|<on>|<frequency>|<range>|<x>|<y>|<z>|<encryptionKey>
```

`HELLO` re-broadcasts every 10 seconds and whenever a newcomer needs to learn us; peers expire after 35
seconds. `RTX` refreshes every second while transmitting, on a move over 25 m, and once with `on=0` on
release.

Every field after the player id is optional, parsed left to right and skipped if absent, so a build that
sends a shorter `HELLO` is still understood — it just leaves those fields empty. The two versions exist for
the info panel below; `modVersion` is whatever the game put in `session.modVersion`, which comes from the
`LC_Version.VERSION` constant in the scripts.

## TeamSpeak info panel

`ts3plugin_infoData` fills the panel under a client in the TeamSpeak info frame:

```
Lima Charlie
Connected to game: Yes
Playing: Yes
Plugin version: 1.0.0
Addon version: 1.0.0
```

For yourself it reads `lc_core_status`, which reports the live game state. For anyone else it reads the last
`HELLO` that client sent, so a player with no plugin, or whose game is closed, shows nothing at all rather
than a stale entry. "Playing" is only shown for yourself, because it comes from `self.alive` which nobody
broadcasts. An empty version field renders as `unknown`, which is what you see against a peer running a
build that predates this.

## Timings

| Constant | Value | Where |
| --- | --- | --- |
| Plugin worker poll | 5 ms in game, 50 ms idle | `lc_core.c` |
| Game state stale | 3000 ms | `lc_core.c` |
| Transmit stop debounce | 300 ms | `lc_core.c` |
| Terrain link wait | 300 ms | `lc_core.c` |
| Transmission max age | 3500 ms | `lc_core.c` |
| Occlusion refresh | 100 ms, 60 m | `LC_GameStateWriter.c` |
| Terrain link refresh | 1000 ms, or 10 m of movement | `LC_RadioLinks.c` |
| AI hearing report | 1000 ms | `LC_Client.c` |
| Sound event retention | 1000 ms | `LC_SoundQueue.c` |

The worker only raises the Windows timer resolution to 1 ms while a game is running; the setting is
process-wide and costs power system-wide, and TeamSpeak is usually open far longer than Reforger is.

## Script map

| Area | Files |
| --- | --- |
| Client loop | `LC_Client.c` |
| Bridge | `LC_GameStateWriter.c`, `LC_PluginStateReader.c`, `LC_Json.c`, `LC_SoundQueue.c` |
| Session and settings | `LC_Session.c`, `LC_Settings.c`, `LC_ServerSettings.c`, `LC_MissionHeader.c`, `LC_PlayerController.c`, `LC_BaseGameMode.c` |
| Radios | `LC_Radio.c`, `LC_RadioSettings.c`, `LC_RadioMode.c`, `LC_RadioLinks.c`, `LC_Terrain.c`, `LC_VONEntryRadio.c`, `LC_FrequencyInput.c` |
| Channel names | `LC_ChannelLabel.c`, `LC_ChannelLabels.c` |
| Voice | `LC_VONController.c`, `LC_VoiceLevel.c`, `LC_Occlusion.c`, `LC_Rooms.c` |
| AI | `LC_AIHearing.c`, `LC_AIConfigComponent.c`, `LC_VoiceDangerEvent.c` |
| UI | `LC_Hud.c`, `LC_VonDisplay.c`, `LC_VonDisplayFeed.c`, `LC_VONMenu.c`, `LC_VONEntryComponent.c` |

## Enfusion notes worth keeping

- A `modded` class that carries container attributes **must repeat `[BaseContainerProps()]`**, or prefabs
  fail to load that member. It is not inherited.
- `SCR_MissionHeader` declares no `[BaseContainerProps()]`, matching vanilla.
- `.conf` files at the same path in different addons **layer and merge by object GUID**.
- Menu-visible keybinds need `FilterPreset "click"` on the `InputSourceValue` plus a matching `Filter`
  block, and a row in `keyBindingMenu.conf` with `m_sPreset`. Without them a binding works but shows as
  unbound.
- Vanilla RPCs take at most 8 arguments. The settings RPC is at 8.
- `set` and `event` are reserved in Enforce Script.
- There is no compile-time detection of other addons, so soft compatibility needs a separate compat addon.

## Reading packed game files

`unpak.py` in `Common Context` extracts Reforger `.pak` archives; `unpbo.py` does Arma 3 `.pbo`. Vanilla
scripts are spread across `addons/data/data*.pak` in the game install — `SCR_VONController.c` and most AI
and VON code are in `data007.pak`.
