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
it, so the plugin's C sources will end up in `data.pak` on every publish if they are in the addon folder.

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

**`sounds/vanilla` is excluded from the package.** It is the game's own roger beep, built from Bohemia's
samples by `tools/make_vanilla_beeps.py`, and unlike the TFAR and ACRE sets alongside it those samples carry
no licence covering redistribution — so nothing we hand to a player contains them. The set stays selectable
in game: anyone who runs the script against their own install gets the folder and the beeps work. Build with
`LC_PACKAGE_VANILLA=1` to keep it in a package that is not going anywhere.

## Versioning and releases

Version numbers matter for the **plugin**, not the addon: the Reforger Workshop keeps every published
version of a mod, so the addon's history is recoverable from there. The plugin has no such store, and it
has to be matched to the mod it is bridged to, so it carries the scheme.

`MAJOR.STABLE.DEV`, in `plugin/src/lc_version.h`:

| Position | Bumped for | Example |
| --- | --- | --- |
| **Major** | The first public release, and each major feature afterwards | 2.0.0 → 3.0.0 for electronic warfare |
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
Protocol 10 (3.0 test builds; 2.0.x is 9).

```json
{
  "v": 10, "seq": 42, "inGame": true,
  "session": { "token": "...", "playerId": 1, "playerName": "Name",
               "tsChannel": "LimaCharlie", "tsChannelPassword": "",
               "modVersion": "1.0.0" },
  "self": { "alive": true, "pos": [x,y,z], "dir": [x,y,z], "tx": 2,
            "txFrequency": 45000, "txRadio": "123:1", "voiceRange": 20,
            "cleanFraction": 0.35, "beepFraction": 0.9, "unlimitedRx": false,
            "roomVolume": 96, "beepVolume": 1,
            "hearing": { "direct": { "gainDb": 4, "highPassHz": 200, "lowPassHz": 6000, "drive": 0.04 },
                         "radio":  { "gainDb": 0, "highPassHz": 0, "lowPassHz": 0, "drive": 0 } },
            "radios": [ { "id": "123:1", "freq": 45000, "range": 1500, "key": "US",
                          "rx": true, "ear": 1, "volume": 0.8, "beep": "tfar_sw",
                          "halfDuplex": 0, "src": "held", "pos": [x,y,z] } ],
            "sounds": [ { "seq": 3, "set": "ui", "name": "deny", "ear": 0, "volume": 1 } ], "ic": 1 },
  "players": [ { "id": 2, "alive": true, "pos": [x,y,z], "muffle": 0.6, "room": 0.35, "ic": 1 } ],
  "links":   [ { "id": 2, "radio": "88:1", "clearance": 35, "hills": [35, 12] } ]
}
```

- `self.pos`/`dir` are the **listener** (camera). `players[].pos` is the speaker's head.
- `self.alive` means "can hear anything", not literally alive — it is true for an unconscious player and
  for a bodiless Game Master. `players[].alive` is the other question, "can be heard", which an unconscious
  player cannot be. `LC_Life` owns both.
- Nothing touches `CLIENT_INPUT_DEACTIVATED`. TeamSpeak decides when a client is talking, by whatever the
  player has it set to, and the game only decides where that voice goes.
- `tx` mirrors `EVONTransmitType`: 0 none, 1 direct, 2 channel, 3 long range.
- `radios[].id` is `<radio RplId>:<transceiver number>`, unique among the player's own radios.
- `radios[].range` is the announced range, which is 1000000 for a Game Master with the editor open.
- `radios[].src` is where the radio physically is: `held` (in the inventory), `pack` (worn as the backpack:
  its radio entity is the character's `LoadoutBackpackArea` cloth), `borrowed` or `vehicle` (a radio off
  the body held by a **radio lease** of that kind, below), or `speaker` (reserved). An unknown name reads as
  `held`. New in protocol 10.
- `radios[].pos` is the radio's antenna, which is what the plugin announces in `RTX` when transmitting on
  it. For a held or worn radio it is `self.pos`, the position every transmission came from before; for a
  leased one it is the radio entity's world position. A radio with no `pos` is taken to be at `self.pos`.
  New in protocol 10.
- `players[]` holds only players within 60 m; `muffle` is 0 clear to 1 fully obstructed. `room` is how
  much of the listener's room that voice fills, which scales the reverb send only: 1 in the same room,
  0.35 elsewhere in the building, 0.1 from outdoors. New in protocol 8.
- `self.ic` is the vehicle intercom channel the listener is on (crew 1, cargo 2, misc 4; 0 none) and
  `players[].ic` that player's channel on the listener's own vehicle intercom, left out (0) for anyone not on
  it. The game decides who shares a vehicle; the plugin only compares bits. Both are optional, so the
  protocol stayed 10 for them: an older plugin ignores them, an older game sends none. See "Vehicle intercom".
- `links[]` answers the plugin's `radioRx` requests with the terrain to that transmitting radio, `id` being
  its player and `radio` its `radios[].id` on the transmitter's side (per radio since protocol 10, because
  two radios of one player need not be in one place): `hills` is every
  hill above the line of sight, as its height in metres, highest first (up to 8), and `clearance` the
  highest. Both are already scaled by the server's terrain setting. `hills` is new in 2.0.5 without a
  protocol bump: a plugin that finds no `hills` treats `clearance` as one hill, and a 2.0.4 plugin reads only
  `clearance` with its old TFAR formula, so mismatched halves still work but disagree on how much a hill
  costs.
- `sounds[]` is a queue with increasing `seq`; the plugin plays each once.
- `roomVolume` is the volume in m³ of the room the listener is standing in, 0 outdoors. It sizes the
  plugin's room reverb. New in protocol 8.
- `beepVolume` is the player's one beep volume for every channel, 0..1, clamped on the way in. The plugin
  multiplies it into `radios[].volume` wherever it plays a beep and nowhere else, so it never touches a
  voice. New in protocol 9.
- `hearing` is optional and present only while another mod reports the local player's hearing through
  `LC_Hearing` (see [Hearing hook for other mods](#hearing-hook-for-other-mods)). It is already combined
  across mods and range-checked. 0 means unchanged in every field. New in 2.0.5 without a protocol bump:
  an older plugin ignores it and plays voices as before, a newer plugin reads its absence as unchanged.
- `players[].alive` follows the server's `unconsciousCanSpeak` setting: with it on, an unconscious player
  is still audible. Radios are refused while unconscious either way, by vanilla's own activation.

### `plugin_state.json` — plugin to game

Written atomically at 4 Hz or faster.

```json
{ "v": 9, "seq": 971, "gameSeq": 445, "pluginVersion": "2.0.0", "inGame": false,
  "tsConnected": true, "tsClientId": 3, "inGameChannel": false, "peers": 0,
  "selfTalking": true, "micMuted": false, "talking": "", "radioRx": "", "radioHeard": "" }
```

- `micMuted` — TeamSpeak is holding the microphone shut, by its local mute, its mute toggle, or a muted
  speaker. Nothing here sets it; it is reported so the status notice can say why nobody can hear you.
- `talking` — semicolon-delimited player ids currently speaking.
- `radioRx` — `playerId,radioId,src,x,y,z;` requests for terrain clearance the game should answer, one per
  transmitting radio. `x,y,z` is the antenna from its `RTX`, or the talker's head where the plugin has them in
  `players[]` and `src` is `held` or `pack`; the game likewise traces from a streamed-in talker's eyes only for
  a radio on the body. `radioId` and `src` are new in protocol 10.
- `radioHeard` — `playerId,radioId,frequency,quality;` for every transmission actually being heard.

### `server.json` — server overrides

Holds every server setting under a plain name: `teamspeakChannel`, `teamspeakChannelPassword`,
`cleanRangePercent`, `beepRangePercent`, `terrainEffectPercent`, `gameMasterUnlimitedRange`, `aiHearing`,
`diagnostics` (`OFF`/`REGULAR`/`COMPREHENSIVE`; legacy `diagnosticLog: true` = `COMPREHENSIVE`),
`roomDiagnosticLog`, `channelNaming`, `channelLabels`. On the session RPC these travel in the flags int:
bit 0 comprehensive, bit 1 room log, bit 2 unconscious speech, bit 3 regular (set for comprehensive too),
bit 4 `hideRadioRoles`, bit 5 `hideDirectSpeechDisplay`. Also `hideRadioRoles`, `hideDirectSpeechDisplay` in server.json.

Read last, after the defaults, `Settings.conf` and the mission header, and it wins over all of them because
it is the only layer an operator can change without republishing. Applied key by key through
`DoesKeyExist`, so a file with one key overrides one setting. Written out in full by `WriteTemplate()` the
first session it is missing. See [server-settings.md](server-settings.md) for the key formats.

## Radio leases (3.0)

Access to a radio that is not in your own inventory (a borrowed manpack's handset, a vehicle radio from a
seat, a vehicle's infantry phone; later SATCOM links) is arbitrated by the server. Only access: what is said
stays client-trusted. Access is not exclusive: any number of players can hold one radio, and its owner
never needs a lease.

- **Where:** `LC_RadioLeases` (`Radio/LC_RadioLeases.c`), one per game mode
  (`SCR_BaseGameMode.LC_GetRadioLeases()`, or `LC_RadioLeases.Get()`). The server keeps the leases and their
  rules and publishes them as `(radio RplId, player id, kind)` triples in one `[RplProp] array<int>` on the
  modded `SCR_BaseGameMode` (`LC_BaseGameMode.c`). No radio prefab needs a component, and late joiners get
  the state with the game mode.
- **Keys:** a radio is its entity's `RplComponent.Id()`, the same number as `LC_Radio.GetRadioKey` (the part of
  `radios[].id` before the colon).
- **Kinds** (`LC_ERadioLeaseKind`): `BORROWED` → `src` `borrowed`, `VEHICLE` → `src` `vehicle`, `PHONE` (the
  leased entity is the vehicle, which needs an `LC_InfantryPhoneComponent` instead of a radio; never listed).
- **Queries** also work for any player: `GetHolderKind(playerId, radioId)` (-1 if none).
- **Client API:** `Request(radio, kind)`, `Release(radio)`, `ReleaseAll()`, `IsHolder(playerId, radio)`,
  `GetHolders(radio, out playerIds)`, `GetOwnLeases(out radios, out kinds)`, `GetOwnKind(radioId)`,
  `GetOnOwnLeasesChanged()` (fires on your machine when a lease of yours is granted or ends),
  `GetOwnRevision()`.
- **RPCs** (`LC_PlayerController.c`, always on behalf of the controller's own player):
  `RpcAsk_LC_RequestRadioLease(RplId, kind)`, `RpcAsk_LC_ReleaseRadioLease(RplId)`, and
  `RpcDo_LC_RadioLeaseRefused(RplId, refusal)` back to the owner. A grant has no answer of its own: it
  arrives with the replicated state.
- **Server checks on a request:** the player controls a living character (unconscious is fine), the radio
  exists and has a `BaseRadioComponent` (`PHONE`: an `LC_InfantryPhoneComponent`), it is not in the requester's own inventory, and every rule allows
  it. Asking again for a radio already held is a grant that changes nothing. Refusals
  (`LC_ERadioLeaseRefusal`) are logged on both ends: `[LC] Radio lease refused: ...`.
- **Rules** (`Radio/LC_RadioLease.c`), subclasses of `LC_RadioLeaseRule` (`Allows(holder, radio, granting)`):
  - `LC_RadioLeaseDistanceRule(m)`: holder within m of the radio entity (0.5 m slack while held).
    `BORROWED` gets `HANDSET_CABLE_M` (4 m) by default.
  - `LC_RadioLeaseSeatRule(vehicle, seats)`: holder sits in the vehicle in one of the `LC_ERadioLeaseSeat`
    flags (pilot / commander / gunner / cargo; commander = `TURRET` slot named `CommanderTurret`). `VEHICLE`
    gets the vehicle the radio is on (topmost parent with a compartment manager) with pilot + commander +
    gunner. Override `IsSeatAllowed(slot)` for vehicles that name seats differently.
  - `LC_RadioLeasePhoneRule(m)`: a distance rule measured from the infantry phone (its action context), not
    the vehicle's origin; `LC_RadioLeaseOnFootRule`: holder in no seat of any vehicle (re-checked on seat
    events). `PHONE` gets both, with the phone's cable length, and only if the vehicle has an intercom.
  - Distance rules measure from `GetAnchor(radio)` (the radio entity's position); override it for another point.
  - Which rules a lease gets: override `LC_RadioLeases.CollectRules(kind, radio, holder, rules)` in a
    `modded class`. No rules means refused, so no kind reaches a radio across the map.
- **Ends:** explicit release; death (`OnPlayerKilled` / `OnPlayerKilledHandled`); disconnect
  (`OnPlayerDisconnected`); and a server check every 500 ms, run only while leases exist, that ends a lease
  whose radio entity is gone, whose player now controls a different entity (respawn, possession), whose
  holder is dead, or whose rules no longer allow it. A seat rule also re-checks on the holder's seat events
  (`SCR_CompartmentAccessComponent` entered / left; a seat switch is checked on the entry only). Logged as
  `[LC] Radio lease granted / ended: player, radio, kind or reason`.
- **Bridge:** `LC_Client` adds an `SCR_VONEntryRadio` per transceiver of each radio you hold (not added to the
  VON menu) after vanilla's entries, so transmit keys of carried radios keep their places, and drops it if
  vanilla lists that radio itself. Only radios with an `SCR_RadioComponent` gadget are listed (vanilla's
  unused vehicle radios have none). `radios[]` then lists them with `src` `borrowed` / `vehicle` and the
  entity's position; the plugin needs no change.
- **Trying it** (Workbench / diag exe only, `#ifdef ENABLE_DIAG`, `Radio/LC_RadioLeaseDiag.c`): Diag menu
  → *Lima Charlie* → *Lease nearest radio* (nearest radio entity within 4 m that is not yours, including a
  manpack someone nearby wears) or *Lease this vehicle's radio* (the vehicle's own radio or its first radio
  child). Unticking either releases all your leases. Results are in the log.

## Vehicle intercom (3.0)

Crew / cargo / misc channels per vehicle. Direct speech from a player on your channel of your vehicle is
heard a second time, through the intercom: clear and centred, no distance, direction, muffling or room, a
light band-pass (250 Hz - 5 kHz, `lc_audio.c` `LC_INTERCOM_*`) and the listener's radio-path hearing profile
(a headset mod). The normal direct path is untouched, so the person beside you is heard both ways. A whisper
reaches the far end of the vehicle: intercom ignores voice range.

- **Where it is:** `LC_VehicleCommsComponent` (`Vehicle/`, already on every vanilla vehicle base through the
  prefab overrides) has `m_bIntercom` (default on; `m_bMilitaryOnly` covers it too, so civilian cars and
  trucks have none) and `m_iIntercomCrewSeats` (`LC_ERadioLeaseSeat` flags, default pilot + commander +
  gunner): those seats start on crew, every other seat on cargo. `GetSeatChannel(slot)` is the override point.
- **Who is on it:** every machine works it out for itself, from replicated seat occupancy (and, for the
  infantry phone, the replicated lease). Only channel *choices* are replicated: `[RplProp] m_aIntercomChoices`
  on the comms component, (player id, channel) pairs for players who picked something other than their seat's
  start. The server writes it (`ServerSetIntercomChoice`, via `SCR_PlayerController.LC_SetIntercomChannel`,
  refused for players not on that intercom), dropping pairs of players who have since left. Entering a
  vehicle that still holds an old pick of yours resets it to the seat's.
- **Client:** `LC_VehicleSeat` (the vehicle reconciler) lists `LC_VONEntryIntercom` last in the VON menu
  while you are on an intercom and holds your pick (heard locally at once, before the server confirms).
  `LC_GameStateWriter` writes `self.ic` from it and `players[].ic` from `GetPlayerIntercomChannel`.
- **The entry** (`Vehicle/LC_Intercom.c`): an `SCR_VONEntry` on vanilla's VON entry layout; scroll, the
  cycle-channel key or the action key cycle No Intercom -> Crew -> Cargo -> Misc. It never becomes the active
  entry and has no transmit key. Three patches make a non-radio entry safe in the VON list:
  `SCR_VONController.SetEntryActive` refuses it (`Voice/LC_VONController.c`; otherwise picking it turns the
  radio key into direct speech), `SCR_RadioComponent.RadioToggle` null-checks the radio entry cast
  (`Radio/LC_RadioToggle.c`, vanilla body otherwise), and `SCR_VONMenu.OnAdjustEntry` cycles it without the
  squad/platoon frequency table (`Radio/LC_VONMenu.c`).

## Infantry phone (3.0)

A handset outside a vehicle that puts whoever holds it on the vehicle's intercom, so infantry can talk to
the crew. Vanilla BTR-70 and LAV-25 have one at the passenger door: the exterior door context that leads to
the cargo seats, the right-hand one where there are two (BTR-70 `door_r01`: both side doors serve every
cargo seat and the driver, and cargo seats list the right one first; LAV-25 `door_back_right`, the right rear
door). Any other vehicle gets one from **one prefab override and no script**.

- **Pieces** (`Vehicle/LC_InfantryPhone.c`): `LC_InfantryPhoneComponent` on the vehicle (attributes: action
  context name, no default (empty = the vehicle's origin); cable length, default 5 m; intercom channel,
  default Crew; handset prop, default `Prefabs/LC/Props/LC_InfantryPhoneHandset.et`) and
  `LC_InfantryPhoneAction` in the vehicle's `ActionsManagerComponent`, in that context. It needs
  `LC_VehicleCommsComponent` with its intercom on the same vehicle; every vanilla vehicle base already has
  it (a military-only intercom has no phone on a civilian vehicle either).
- **How it works:** the action (only shown on foot, local effect only) asks for a `PHONE` lease on the
  vehicle, or gives it back; picking up one phone hangs up any other. The server holds it under
  `LC_RadioLeasePhoneRule` (cable length from the context's world position, `GetPhonePosition`, falling back
  to the vehicle origin if the context is missing) and `LC_RadioLeaseOnFootRule`. It ends on walking off,
  the action, getting into any seat, death, respawn or disconnect, like any lease. While it is held,
  `LC_VehicleCommsComponent.GetPhoneChannel` puts the holder on the phone's channel on every machine (so
  `players[].ic` and the server's membership check pick them up), and `LC_VehicleSeat.FindPhoneIntercom`
  gives the holder the Intercom entry in the radio menu, where they can pick another channel like any seat.
  Nothing new in the bridge or the plugin.
- **Handset pose** (`Radio/LC_HandsetItem.c`): on a `PHONE` grant the server spawns
  `Prefabs/LC/Items/LC_InfantryPhoneHandsetItem.et` (an item with `LC_HandsetGadgetComponent`, no mesh, the
  AN/PRC-68's player animation instance, `m_eAnimVariable ANPRC68`) into the holder's inventory, and deletes it
  when the lease ends (`LC_RadioLeases.End`); dropping it ends the lease ("handset dropped"). The holder's own
  machine takes it into the left hand once when it arrives (`SCR_GadgetManagerComponent.SetGadgetMode IN_HAND`,
  retried every 500 ms up to 6 times), so the character holds it like a radio; the engine replicates that
  pose. Not inspected (inspection hides world actions and forces free look) and not forced back: pulling out
  something else ends the pose, the phone stays held. Item prefab: `m_sHandsetItemPrefab` on the phone
  component; override point `LC_RadioLeases.GetHandsetItemPrefab(kind, radio)`.
- **Handset and cable** (`Radio/LC_HandsetCable.c`, generic; research in
  `docs/research/2026-11-02-handset-cable.md`): while a phone is held, every client puts the handset prop
  (the vanilla wall phone's handset model, no physics) on the held handset item (on the `LeftHandProp` bone
  until the item is in hand), centred on the model's own bounds (`GRIP_OFFSET`, `HANDSET_ANGLES`), and draws a
  thin black cable (`Assets/LC/LC_HandsetCable.emat`) from the bottom of the handset to the phone point, sagging with slack (straight line x 1.15 + 0.5 m, capped at
  the cable length) and kept above the terrain. Drawn locally from the replicated lease (`LC_HandsetCables`,
  ticked by `LC_Client`), so nothing new is replicated; no physics. Hidden and not updated beyond 100 m of
  the camera or while holder or vehicle is streamed out; gone when the lease ends. The mesh is rebuilt
  (`MeshObject.Create`) only when an end moves more than 1 cm. For the manpack handset (A): add `BORROWED`
  to `LC_HandsetCables.HasCable` and give `GetAnchor` / `GetLength` / `GetHandsetPrefab` its radio;
  `LC_HandsetCable` itself needs no change.
- **Adding it to a modded vehicle.** In your mod, override the vehicle's prefab (or its base) at its own
  path and GUID, holding only the new blocks. Reuse the vehicle's own `ActionsManagerComponent` block ID
  (find it in its prefab chain; vanilla vehicles inherit `{C97BE5489221AE18}`): a fresh ID would add a second
  actions manager instead of extending the first. The new component and action get fresh IDs. Pick an
  exterior context the vehicle already has, preferably the door to its cargo seats (the door index in the
  `CargoCompartmentSlot`s' `DoorInfoList` points into the vehicle's `CompartmentDoorInfo` list, which names
  its `ContextName`), and name it in both places: `m_sContextName` on the component and the action's
  `ParentContextList`. The phone's distance rule and its cable are measured from that context's position.
  The worked example, `Prefabs/Vehicles/Wheeled/LAV25/LAV25_base.et` in this addon (the BTR-70's is the same
  with `door_r01`):

  ```
  Vehicle : "{543A50BF3A42AD47}Prefabs/Vehicles/Core/Wheeled_APC_Base.et" {
   ID "BBCBA43A9778AE21"
   components {
    LC_InfantryPhoneComponent "{0A96636490FC9A7E}" {
     m_sContextName "door_back_right"
    }
    ActionsManagerComponent "{C97BE5489221AE18}" {
     additionalActions {
      LC_InfantryPhoneAction "{F3A04750554AEE9D}" {
       ParentContextList {
        "door_back_right"
       }
       UIInfo SCR_ActionUIInfo "{5959B6D4C6644BB1}" {
        Name "Infantry Phone"
       }
      }
     }
    }
   }
  }
  ```

  The first two lines are copied from the vehicle's own prefab (its parent and `ID`). Its `.meta` names the
  overridden resource (`{1B8EB269EECA2DF9}Prefabs/Vehicles/Wheeled/LAV25/LAV25_base.et`). A vehicle mod of
  your own can instead add the two blocks in Workbench. `m_sContextName` has no default: left out, the phone
  hangs at the vehicle's origin. Workbench needs a restart to register
  new override files; check the result with the resolved prefab (one `ActionsManagerComponent`, the phone
  action beside the vehicle's own). The action's name comes from script (Pick Up / Hang Up Infantry Phone).

## Hearing hook for other mods

Lets another mod (an ear protection headset, earplugs) change how the local player hears Lima Charlie
voices, without either mod depending on the other: a separate compat addon that depends on both calls it.
Lima Charlie knows nothing about the other mod's modes; the compat turns each mode into effects.

```c
LC_HearingProfile profile = new LC_HearingProfile();   // every field 0 = unchanged
profile.m_fDirectGainDb = 4;          // nearby voices, dB, -40 .. +12
profile.m_fDirectHighPassHz = 200;    // 0 = no filter, else 20 .. 20000
profile.m_fDirectLowPassHz = 6000;    // 0 = no filter, else 20 .. 20000
profile.m_fDirectDrive = 0.04;        // soft clipper, 0 none .. 1 heavy
// m_fRadioGainDb, m_fRadioHighPassHz, m_fRadioLowPassHz, m_fRadioDrive: the same for radio voices
LC_Hearing.SetLocalProfile("MyMod_EarPro", profile);   // null clears this source only
```

- **Contract.** Classes and field names are frozen once released: fields may be added (defaulting to 0 =
  unchanged), never renamed or removed. `SetLocalProfile` copies the values.
- **Sources.** Each mod reports under its own stable key; clearing one never clears another.
- **Expiry.** A source not refreshed within 1 s (`LC_Hearing.EXPIRY_MS`, measured on
  `System.GetTickCount()`, the clock the bridge writer uses) is dropped, so refresh at least once a
  second and clear explicitly on Off, no headset or teardown. The expiry is the fallback, not the plan.
  A late `null` from a previous world clears the source until its next refresh.
- **Ranges.** Each value is clamped on the way in and again by the plugin. 0 stays 0 before any clamp, so an
  unset cut-off never turns into a 20 Hz filter. Anything not a finite number reads as 0.
- **Combining sources.** Gains add (then clamp to -40 .. +12); filters stack in series, so the highest
  high-pass and the lowest low-pass win; the strongest drive wins.
- **Where it acts.** Nearby voices: after distance, panning and occlusion muffling, before the room reverb
  send, so a room's tail is heard through the headset too. Radio voices: after the radio effect (band,
  garble), before the ear the radio is set to. Radio beeps and UI sounds are never touched. Filters are
  12 dB/octave (one biquad each, Q 0.707). Drive is a `tanh` soft clipper level matched at speech level,
  multiplier `1 + 9 x drive`. Gain changes ramp across one audio buffer; cut-off changes keep the
  filter's history; a filter coming in starts from silence.
- **What it cannot do.** It is an approximation of a headset, not a copy of a game audio chain: no EQ bands,
  resonance or compressor, and nothing a gunshot in the game could drive, since TeamSpeak never hears the
  game.
- **Seeing it work.** The game logs `[LC] Hearing {...}` whenever the combined profile changes and
  `[LC] Hearing unchanged` when it ends; the plugin logs `Hearing: direct ...; radio ...` on each change
  to the TeamSpeak log. With the server's troubleshooting setting on, it is in every logged `game_state`.
- **Versions.** The classes exist from mod 2.0.5; the plugin acts on them from 2.0.5. An older plugin
  silently leaves voices unchanged; the game logs the plugin's version as `[LC] Plugin <version>: ...`.

## Plugin commands

Between plugins, over the TeamSpeak channel.

```
LC1|HELLO|<token>|<playerId>|<voiceRange>|<pluginVersion>|<modVersion>
LC1|RTX|<token>|<playerId>|<on>|<frequency>|<range>|<x>|<y>|<z>|<radioId>|<src>|<encryptionKey>
```

`HELLO` re-broadcasts every 10 seconds and whenever a newcomer needs to learn us; peers expire after 35
seconds. `RTX` refreshes every second while transmitting, on a move over 25 m, and once with `on=0` on
release.

`RTX` carries the transmitting radio's antenna (`radios[].pos`), its id (`radios[].id`, with `|`, `,` and `;`
replaced by `_`) and its source (`radios[].src` by name); receivers measure distance and look up terrain
from that radio, not from the player. Before protocol 10 there was no radio id or source and the position
was the talker's own. A protocol 9 `RTX` is rejected by a 3.0 plugin, and a 2.0.x plugin reads a 3.0 one with
a mismatched key, so mixed builds simply do not hear each other on the radio. A change of radio restarts the
300 ms terrain wait, since the new antenna has not been traced.

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

## How beeps reach your ears

Beeps are WAVs in the plugin's `sounds/<set>/` folders, handed to TeamSpeak's own player with
`ts3Functions.playWaveFile`. That API takes a path and nothing else, so the ear and the volume are baked into
a rendered copy: `sounds/beepcache/<set>_<name>_e<ear>_v<step>.wav`, 48 kHz 16-bit stereo, written the first
time that combination is asked for and reused after. Volume is quantised to twenty steps to keep the number
of files down, and the cache is emptied on every load so a plugin update cannot leave an old rendering of a
changed beep behind.

Before 1.0.13 they were written into the mixed playback buffer instead, in
`ts3plugin_onEditMixedPlaybackVoiceDataEvent`. That buffer is shared with every plugin the client has loaded,
handed to each in load order, and a plugin that overwrites rather than adds to it destroys whatever is
already there. Coalition VON did that on a tester's machine and the beeps tore; nothing about our own mixing
could have prevented it, because we load first. TeamSpeak's player is out of reach of all of them.

That code is still there as a fallback, used when the cache directory cannot be made or written — a
read-only install, or a locked-down profile. The log says which happened.

Voices are unaffected and still go through `ts3plugin_onEditPostProcessVoiceDataEvent` per speaker.

## Timings

| Constant | Value | Where |
| --- | --- | --- |
| Plugin worker poll | 5 ms in game, 50 ms idle | `lc_core.c` |
| Game state stale | 3000 ms | `lc_core.c` |
| Game state grace | 300000 ms | `lc_core.c` |
| Transmit stop debounce | 300 ms | `lc_core.c` |
| Terrain link wait | 300 ms | `lc_radio.h` |
| Transmission max age | 3500 ms | `lc_core.c` |
| Occlusion refresh | 100 ms, 60 m | `LC_GameStateWriter.c` |
| Terrain link refresh | 1000 ms, or 10 m of movement | `LC_RadioLinks.c` |
| AI hearing report | 1000 ms | `LC_Client.c` |
| Sound event retention | 1000 ms | `LC_SoundQueue.c` |

Those two windows answer different questions. **Stale** (3 s) means the game is not ticking — alt-tabbed,
minimised, hitching or gone — and only ends the radio transmission, so a frozen game cannot hold a transmit
key open on the net. **Grace** (5 min) is how long a game that has stopped writing still counts as being
played, for the TeamSpeak channel, the peer list and the voices. A normal exit writes `inGame:false`, which
is immediate, so the grace only decides how long a crashed game keeps you in the game channel — and that is
much cheaper than moving everyone out of the channel and back in whenever they alt-tab, since every move is
a notification for everyone else in it.

The worker only raises the Windows timer resolution to 1 ms while a game is running; the setting is
process-wide and costs power system-wide, and TeamSpeak is usually open far longer than Reforger is.

## Script map

| Area | Files |
| --- | --- |
| Client loop | `LC_Client.c` |
| Bridge | `LC_GameStateWriter.c`, `LC_PluginStateReader.c`, `LC_Json.c`, `LC_SoundQueue.c` |
| Session and settings | `LC_Session.c`, `LC_Settings.c`, `LC_ServerSettings.c`, `LC_MissionHeader.c`, `LC_PlayerController.c`, `LC_BaseGameMode.c` |
| Radios | `LC_Radio.c`, `LC_RadioSettings.c`, `LC_RadioMode.c`, `LC_RadioLinks.c`, `LC_Terrain.c`, `LC_VONEntryRadio.c`, `LC_FrequencyInput.c` |
| Radio leases | `LC_RadioLeases.c`, `LC_RadioLease.c` (kinds, refusals, rules), `LC_RadioLeaseDiag.c` (diag only), `LC_HandsetCable.c` (held handset prop + cable, drawn per client) |
| Vehicles | `LC_VehicleCommsComponent.c`, `LC_VehicleSeat.c`, `LC_Intercom.c`, `LC_InfantryPhone.c`, `LC_VehicleRadioSeatCondition.c` |
| Channel names | `LC_ChannelLabel.c`, `LC_ChannelLabels.c` |
| Voice | `LC_VONController.c`, `LC_VoiceLevel.c`, `LC_Occlusion.c`, `LC_Rooms.c`, `LC_Hearing.c` (hook for other mods) |
| AI | `LC_AIHearing.c`, `LC_AIConfigComponent.c`, `LC_VoiceDangerEvent.c` |
| UI | `LC_Hud.c`, `LC_VonDisplay.c`, `LC_VonDisplayFeed.c`, `LC_VONMenu.c`, `LC_VONEntryComponent.c` |

## Enfusion notes

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
