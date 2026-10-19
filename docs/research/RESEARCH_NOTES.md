# TeamSpeak integration in Arma 3 and Arma Reforger: research notes

Researched 2026-09-14. Target is **TeamSpeak 3** (plugin SDK). All paths below are relative to this folder.

**Location:** `Documents\My Games\ArmaReforgerWorkbench\_RRT_reference`. This material used to live in `_reference/` inside the addon. It was moved out because Workbench indexed it and the cloned Coalition-VON repo's `.meta` files hijacked real resource GUIDs. Keep it outside `addons\`.

## 0. What's in this folder

| Folder | Contents | Source |
|---|---|---|
| `vanilla/scripts/` | All Reforger `.c` scripts (core + data paks) | `unpak.py` |
| `vanilla/Prefabs`, `vanilla/Configs` | VON / Radio / Transceiver related `.et` `.conf` `.layout` + `Character_Base.et`, `chimeraInputCommon.conf` | targeted extract |
| `CoalitionVON/` | Coalition VON 2.1.30 unpacked (scripts, prefabs, configs, layouts) | `unpak.py` |
| `506IRRU/` | 506th IRRU Enhanced Radio 1.0.38 unpacked | `unpak.py` |
| `a3/ACRE2/`, `a3/TFAR/` | Text/config files from every PBO (config.bin stays rapified) | `unpbo.py` (addon root) |
| `repos/acre2` | github.com/IDI-Systems/acre2 (SQF + C++ extensions + TS plugin) | git clone |
| `repos/TFAR` | github.com/michail-nikolaev/task-force-arma-3-radio | git clone |
| `repos/Coalition-VON` | github.com/CoalitionArma/Coalition-VON (Reforger mod + TS3 plugin, commit d1c2e48, 2026-08-28) | git clone |
| `repos/ts3client-pluginsdk` | github.com/TeamSpeak-Systems/ts3client-pluginsdk (API 26; headers copied to `plugin/third_party/ts3sdk` in the addon) | git clone |

**Licences** (they matter if code is reused): ACRE2 is GPLv3, TFAR is APL-SA, and Coalition VON is APL.

---

## 1. Vanilla Arma Reforger VON

### 1.1 Components and flow
```
Input (VONDirect / VONDirectToggle / VONChannel / VONTransceiverCycle / VONLongRangeToggle)
   │
SCR_VONController  (ScriptComponent on SCR_PlayerController, client only)   scripts/Game/VON/SCR_VONController.c
   │  entries: SCR_VONEntry (direct) / SCR_VONEntryRadio (one per BaseTransceiver)
   │  ActivateVON → SetActiveTransmit(entry):
   │        VoNComponent.SetCommMethod(ECommMethod.DIRECT | SQUAD_RADIO)
   │        VoNComponent.SetTransmitRadio(BaseTransceiver or null)
   │        VoNComponent.SetCapture(true/false)
   ▼
SCR_VoNComponent : VoNComponent  (on the character; Character_Base.et → Filename "Sounds/VON/von.acp")
   ENGINE (native, not scriptable): mic capture → OPUS → replicated through the game server → playback
   Script callbacks only:
     OnCapture(BaseTransceiver transmitter)              – polled each frame while recording
     OnReceive(int playerId, bool isSenderEditor, BaseTransceiver receiver, int frequency, float quality)
     OnVoNUsed(int senderId)                             – server side
```
- `ECommMethod`: DIRECT, SQUAD_RADIO, DIRECT_AI, GAME_MASTER_DIRECT, GAME_MASTER_RADIO.
- There is **no script access to voice samples**. Script can only select the method/transceiver, start and stop capture, react to callbacks, and set audio-graph variables (`AudioSystem.SetVariableByName`) that the `.acp` sound graph reads.
- `HandleUpdateVONControllersSystem` ticks `SCR_VONController.Update()`.
- Game-level switches: `GetGame().GetVONCanTransmitCrossFaction()` (server config `VONCanTransmitCrossFaction`), `IsVONUIDisabledByServer()`, `IsVONDirectSpeechUIDisabledByServer()`.
- The official wiki page (<https://community.bistudio.com/wiki/Arma_Reforger:Audio:_Voice_over_Network>) was blocked by Cloudflare verification and was not read. Search snippets say: VoNComponent gets OPUS packets from `AudioCaptureDevice`, and a receiver hears the `VoN_RADIO` sound event when in range, on the same frequency and with the same encryption key.

### 1.2 Radios
- `BaseRadioComponent` (generated): `GetTransceiver(i)`, `TransceiversCount()`, `Set/GetEncryptionKey`, `SetPower/IsPowered`, `SetTransceiverFrequency`, plus invokers for frequency, range, power and encryption changes.
- `BaseTransceiver`: `Get/SetFrequency` (kHz), min/max/resolution, `Get/SetRange` (m, server only), `IsMuted/SetMuteState`, `GetRadio()`, `BeginTransmission(BaseRadioMessage)`.
- `RadioManagerEntity.GetTransceiversInRange(pos, range, out list)`.
- Prefab example `Prefabs/Items/Equipment/Radios/Radio_ANPRC68.et`: `"Encryption key"`, `RadioTransceiver { ChannelFrequency 32000, "Transmitting Range" 1300, Min 32000, Max 68000, "Frequency resolution" 200 }`, plus `SCR_RadioComponent` for animation/gadget.
- Faction key: `SCR_Faction.GetFactionRadioEncryptionKey()`.
- The engine decides reception: frequency match, transmitter range and encryption key match.

### 1.3 Script-reachable I/O (relevant to a TS bridge)
- **No DLL/extension mechanism** (Arma 3's `callExtension` has no equivalent).
- `FileIO.OpenFile`, `FileExists`, `MakeDirectory`, `DeleteFile`/`CopyFile` (only `$profile:`, `$logs:`, `$saves:`). `JsonSaveContext.SaveToFile` / `JsonLoadContext.LoadFromFile`.
- `RestApi.GetContext(url)` → `RestContext.GET/POST/PUT/DELETE` (async callback or `_now`), `SetHeaders`, `SetTimeout`. Whether it can reach `http://127.0.0.1` is **untested**.

---

## 2. 506th IRRU Enhanced Radio (vanilla VON, no TeamSpeak)
This mod shows how far vanilla VON can be pushed from script:
- `modded SCR_VONController` adds TX start/end beeps, a key-spam lockout, alternate-channel PTT (`IRRU_AlternateChannel`), frequency entry UI and ear routing/volume per transceiver. It also sends a **key-state RPC** to the server (`RpcAsk_IRRU_KeyState`), which `IRRU_RFPropagationNetworkComponent` relays so receivers get squelch/dead-key even without voice packets.
- `modded SCR_VoNComponent.OnReceive`: squelch state machine (`IRRU_RadioRxSquelch`), and a terrain RF model (`IRRU_RFPropagationModel`: 4/3-earth, knife-edge diffraction, sampling every 50 m) plus jammers. Results are written to `.acp` variables `SignalQuality` and `JamStrength` via `AudioSystem.SetVariableByName(..., RadioEarRouting.conf)`.
- Documented engine limitation: audio variables are **global slots**, read once when a sound event starts for a packet. They are not per-stream, so simultaneous transmissions race each other.

---

## 3. Coalition VON (Reforger ↔ TeamSpeak 3)

### 3.1 Architecture
```
 ┌────────────── Game server ──────────────┐
 │ CVON_VONGameModeComponent               │  VONServerSettings.json ($profile of server):
 │  RplProp: TS channel name/pw, TS IP/pw, │    VONChannelName, VONChannelPassword,
 │  per-player voice range, TS client IDs  │    TeamspeakServerIP, TeamspeakServerPassword
 │  AddLocalVONBroadcasts(): recipient     │
 │  filter by freq / time-dev / faction key│
 └──────▲───────────────────────┬──────────┘
        │ RPC (PTT start/stop,  │ Owner RPC AddLocalVONBroadcast / RemoveLocalVONBroadcast
        │ TS id, radio changes) ▼
 ┌────── Each game client (script) ────────┐     files in Documents\My Games\ArmaReforger\profile\
 │ modded SCR_VONController (vanilla VON   │ ──► VONData.json    (every fixed frame, dirty-checked)
 │  disabled: ActivateVON returns false)   │ ──► RadioData.json  (on radio change)
 │ CVON_RadioComponent (own radio model)   │ ◄─► VONServerData.json (game writes channel info,
 └─────────────────────────────────────────┘                        plugin writes TSClientID)
                                                         ▲ polled by mtime
 ┌────── TeamSpeak 3 client plugin (C) ────┐             │
 │ worker thread 200 ms: read JSON, auto-  │─────────────┘
 │ connect, move channel, mic on/off, mute │
 │ onEditPostProcessVoiceDataEvent: DSP    │   voice audio itself goes via the TS server
 └─────────────────────────────────────────┘
```

### 3.2 Game side (`CoalitionVON/scripts/Game`)
- **Opt-in**: everything checks `CVON_VONGameModeComponent.GetInstance()` (component on the game-mode prefab). Otherwise the vanilla path runs.
- **Radios**: `CVON_RadioComponent` has RplProps `m_sFrequency` (string, e.g. "55500"), `m_aChannels`, `m_iCurrentChannel`, `m_iTimeDeviation` ("espionage"), `m_sFactionKey`, `m_bPower`, `m_HandMicHolder`. Also `m_eRadioType` SHORT/LONG and `m_iRadioRange`. Channel presets come from a `CVON_FreqConfig` conf plus per-faction overrides, and shared frequencies across factions via `CVON_SharedFrequencyObject`. Changes go client → PlayerController RPC → server → `Replication.BumpMe()`.
- **Keys**: `VONChannel` = SR (radio 0), `VONLongRange` = LR (radio 1), `VONMediumRange` = radio 2, plus rotate active, channel up/down, ear L/R/Both, open radio. Direct speech uses **TeamSpeak's own PTT/VAD**, not a game key.
- **Radio transmission**: PTT builds `CVON_VONContainer` (type, sender RplId, freq, volume, TS client id, faction key, playerId, time-dev; custom snapshot codec) → `RpcAsk_BroadcastLocalVONToServer` → server loops players whose reported freqs (`m_PlayerFreqArray`, sent by clients from `WriteJSON`) and actual radios match freq + time deviation + faction key (if encryption on) → Owner RPC to each with sender origin and radio range. Release → `RemoveLocalVONBroadcasts` to all.
- **Direct speech**: no network message. Each client, every fixed frame, adds a DIRECT entry for every living player within that player's voice range (replicated; steps 3/8/25/40/60 m).
- **Client-side acoustics** (computed in script, sent to plugin as numbers):
  - Equal-power stereo pan from listener transform, with −45 dB at the voice range (shape exponent 1.6) → `LeftGain`/`RightGain`.
  - Occlusion by physics traces (building −12 dB, vehicle −18 dB) → `MuffledDecibels`.
  - Radio `ConnectionQuality`: a piecewise curve of distance vs radio range.
  - Babble: `SameLanguage` false when factions differ and babble is enabled.
- **VONData.json** (keyed by the *sender's TS client ID*):
  `{"IsTransmitting":bool, "<tsId>":{VONType 0/1, Frequency, LeftGain, RightGain, MuffledDecibels, ConnectionQuality, FactionKey, PlayerId, BehindIntensity, SameLanguage}}`
- **RadioData.json** (keyed by radio RplId, powered radios only; empty when headset is lowered): `{Freq, TimeDeviation, Volume 0-9, Stereo 0 both/1 right/2 left, FactionKey}`.
- **Identity mapping**: the plugin writes its own `TSClientID` into VONServerData.json. The game reads it (1 s cadence) → `RpcAsk_SetTeamspeakClientId` → server → replicated `m_aPlayerIds[]` / `m_aPlayerClientIds[]`. Clients look up other players' TS ids from that. If the id is 0 and `m_bTeamspeakChecks` is set, a blocking warning menu opens.
- On controller destruction it writes `InGame=false`, so the plugin returns the user to their previous channel.

### 3.3 TeamSpeak 3 plugin (`repos/Coalition-VON/CoalitionTeamspeakPlugin/src/plugin.c`, API version 26, Windows only)
- **Paths are hardcoded**: `SHGetKnownFolderPath(Documents)\My Games\ArmaReforger\profile\*.json`. This breaks if the game runs with a custom `-profile`.
- **Worker thread** (200 ms tick):
  - Reloads files by mtime: VON 50 ms, Radio 300 ms, Server 500 ms. It uses double-buffered snapshots with atomic index swap, so the audio thread never locks.
  - Rewrites VONServerData.json with `TSClientID` and plugin version when they change.
  - Auto-connect: MessageBox confirm, then `guiConnect` (and `stopConnection` for the previous one).
  - `requestClientMove` into `VONChannelName` with password and backoff; returns to the previous channel when `InGame` goes false.
  - Mic: `IsTransmitting` → `setClientSelfVariableAsInt(CLIENT_INPUT_DEACTIVATED, INPUT_ACTIVE)`, which only reverts what the plugin itself opened.
  - **Proximity muting**: every client in the channel without a VONData entry gets `requestMuteClients`; they're unmuted when an entry appears. Out of game, all are unmuted.
- **Audio** in `ts3plugin_onEditPostProcessVoiceDataEvent` (per talker, after TS mixing prep):
  - Only active while in the VON channel. No entry → silence.
  - RADIO entry matching a local radio (freq, time-dev, faction) → ACRE-like chain: ×3 boost, 35 Hz ring mod, foldback distortion, LP 4 kHz / HP 750 Hz, plus pink/white noise scaled by quality. Volume step/9, ear select.
  - Otherwise → direct DSP: mono collapse, re-pan with L/R gains, 3-stage LP muffle (8 kHz→1 kHz by dB), babble (2 kHz ring mod + LPs + soft clip). The behind-head filter is present but disabled.
  - TS's own 3D API is not used.

### 3.4 Observations
- Simple and robust to build (no DLL in game), but latency is file-poll based: game write each frame, plugin 50 ms poll, 200 ms worker. Every client also writes to disk continuously.
- Server-authoritative radio recipient selection; client-authoritative acoustics.
- Relies on one shared TS channel with mute/unmute plus per-client DSP. PTT mic control for radio comes from the game; direct speech comes from TS.

---

## 4. ACRE2 (Arma 3)

### 4.1 Layers and transport
```
SQF (addons/sys_core, sys_rpc, sys_io, sys_radio, sys_signal …)
  └─ "acre" callExtension  (acre_x64.dll, extensions/src/ACRE2Arma/arma2ts/CallExt_DllMain.cpp)
        commands: 0 open, 1 close, 2 write, 3 read, 4 reset
        └─ Named pipes  \\.\pipe\acre_comm_pipe_toTS  /  \\.\pipe\acre_comm_pipe_fromTS
              created (server side) by the TS plugin: ACRE2Core/NamedPipeServer.cpp (send + read threads)
              └─ ACRE2TS plugin (acre2_win64.dll) → ACRE2Core: CEngine, CRpcEngine, sound mixer, DSP
Plugin ⇄ other plugins: TS plugin commands (ts3Functions.sendPluginCommand / ts3plugin_onPluginCommandEvent, name "acre2")
```
- Text RPC messages are `procedure:param,param,`. SQF registers handlers with `sys_rpc addProcedure` (e.g. `handleGetClientID`, `localStartSpeaking`, `remoteStartSpeaking`, `remoteStopSpeaking`, `pong`, `gen`). The plugin registers `RPC_FUNCTION`s (`updateSpeakingData`, `getClientID`, `setTs3ChannelDetails`, `setMuted`, `playSound`, `ext_remoteStartSpeaking` …).
- A separate game-side extension, `ACRE2Arma` (sys_signal), runs the signal/propagation math (arcade, LOS multipath, Longley-Rice/ITM).

### 4.2 Identity
Every 3 s SQF calls `getClientID(netId, UID)` → plugin → `sendPluginCommand ext_handleGetClientID(tsId, netId)` to other TS clients → each remote plugin passes `handleGetClientID` to its game → `objectFromNetId` → `playerList [[tsId, unit]]`. **The mapping travels over TeamSpeak, not the Arma network.**

### 4.3 Speaking
1. PTT → plugin `localStartSpeaking(type, radioId)`. It opens the mic (`CLIENT_INPUT_DEACTIVATED`), unless VAD is on. It broadcasts `ext_remoteStartSpeaking(tsId, languageId, netId, speakingType, radioId, curveScale)` via TS plugin command, and tells its own game `localStartSpeaking`.
2. Receiving plugins forward `remoteStartSpeaking` to their game. Receiving SQF computes, per speaker: direct (position, head vector, volume), or radio (for each receiving radio: volume, signalQuality from sys_signal, signal model, loudspeaker flag, radio position). It streams `updateSpeakingData` messages to the plugin (types: `d` direct, `i` intercom, `z` zeus, `r` radio, `s` spectator, `g` god).
3. Plugin (`ACRE2Core/updateSpeakingData.h`) builds per-speaker mixer channels (4800 samples) with effect inserts `acre_babbel`, `acre_radio`, `acre_volume` and mixdown `acre_positional`. It unmutes radio speakers in TS.
4. Audio (`ACRE2TS/TsCallbacks_sound.cpp`):
   - `onEditPlaybackVoiceDataEvent` copies each client's samples into ACRE's engine.
   - `onEditMixedPlaybackVoiceDataEvent` renders ACRE's own positional mix into the final buffer, using the speaker mask (stereo/5.1/7.1).
   - `onEditCapturedVoiceDataEvent` handles the local mic.
   - Occlusion/position is handled by ACRE's own DSP, not TS 3D.
- Channel management: `setTs3ChannelDetails`, `moveToServerTS3Channel` / `moveToPreviousTS3Channel`. Muting uses `requestMuteClients/requestUnmuteClients`.

---

## 5. TFAR (Arma 3)

### 5.1 Transport
- `"task_force_radio_pipe" callExtension` (`task_force_radio_pipe_x64.dll`). The current implementation is **shared memory** (`SharedMemoryTransfer`; plugin side `ts/src/SharedMemoryHandler.cpp`): named objects `Local\TFARSHAMEM_EVTREQ`, `Local\TFARSHAMEM_EVTRESP`, `Local\TFARSHAMEM_MTX` plus a file mapping.
  - One sync request/response slot (`SignalObjectAndWait`, timeout).
  - An async message queue.
  - A `NamedPipeTransfer` legacy path still exists.
- Commands are tab-separated; a trailing `~` means async ("always returns OK"):
  `FREQ`, `POS`, `IS_SPEAKING`, `IS_SPEAKING_BULK`, `TANGENT`/`TANGENT_LR`/`TANGENT_DD`, `KILLED`, `SPEAKERS`, `DFRAME`, `TS_INFO` (PING/VERSION/SERVER/CHANNEL…), `SETCFG`, `MISSIONEND`, `RELEASE_ALL_TANGENTS`, `RadioTwrAdd/Del`, `TRACK`, `collectDebugInfo`.

### 5.2 Data model: the plugin decides who you hear
- `FREQ` (≤ every 2 s unless changed) sends own SW radios `"freq+code|volume|stereo|radio"`, LR radios, alive, voice range, nickname, underwater, terrain interception coefficient, global volume, receiving distance multiplier and speaker distance.
- `POS` for **every player** (near: up to 30/frame; far: 50 per `TFAR_FAR_PLAYER_UPDATE_TIME`): position, view direction, canSpeak, canUseSW/LR/DD, vehicle id + isolation, terrain interception, voice volume, object interception.
- `SPEAKERS` carries external-speaker radios.
- The plugin (`CommandProcessor.cpp`, `clientData.cpp`, `serverData.cpp`) stores the world state and computes direct audibility, radio range and frequency matching itself.

### 5.3 Identity and signalling
- **Nickname-based**: the plugin sets `CLIENT_NICKNAME` to the in-game name while in game and restores it afterwards. `CLIENT_META_DATA` marks the plugin as enabled and carries its version.
- `TANGENT` → plugin toggles the mic (`CLIENT_INPUT_DEACTIVATED` / VAD preprocessor toggle), then `sendPluginCommand` to the current channel with transmission info (frequency, range, subtype digital/airborne/…).
- **Serious mode**: TS channel `TaskForceRadio` (pw `123`, configurable via `SETCFG`). `requestClientMove` into it; dead and alive players are muted from each other; non-TFAR users are muted.

### 5.4 Audio
`ts3plugin_onEditPostProcessVoiceDataEvent` → `processVoiceData()` (`ts/src/plugin.cpp:455`):
- **Direct**: distance ≤ voiceVolume+15 m. Object-interception filter, vehicle isolation filter, clunk HRTF-ish **ITD** plus `applyILD` (level difference), compressor.
- **Radio**: `clientData->isOverRadio()` (range, frequency, antennas) → per radio `RadioEffect` (SW/LR/DD/airborne/digital profiles), volume, stereo.
- `onEditCapturedVoiceDataEvent` plays own voice through nearby loudspeakers.
- It also uses `systemset3DListenerAttributes`, `channelset3DAttributes` and `onCustom3dRolloffCalculationClientEvent` to neutralise TS's own 3D.

---

## 6. Side-by-side

| | ACRE2 | TFAR | Coalition VON |
|---|---|---|---|
| Game ↔ plugin | `callExtension` → named pipes | `callExtension` → shared memory | JSON files in `$profile:` polled by plugin |
| Player ↔ TS id | netId broadcast via TS plugin commands | TS nickname = in-game name | Plugin writes TS id to file → game → server replicates |
| Who signals "X is transmitting" | Plugin → TS plugin command | Plugin → TS plugin command | Game → game server RPC → recipients |
| Who decides audibility | Game (SQF + signal extension) per receiver | Plugin (from POS/FREQ world state) | Server picks radio recipients; client computes gains |
| Positional audio | Own DSP mixdown (`acre_positional`) | Own ITD/ILD + filters | Script L/R gains → plugin re-pan |
| Mic control | `CLIENT_INPUT_DEACTIVATED` | same + VAD toggle | same (radio only; direct uses TS PTT) |
| Silencing others | TS mute/unmute + DSP | Serious-mode mutes + DSP zeroing | Mute anyone without an entry + DSP |
| Channel | Moves to configured channel | Serious channel | Auto-connect + move to server-provided channel |

## 7. TS3 plugin SDK essentials used by all three
- **Required exports**: `ts3plugin_name`, `_version`, `_apiVersion`, `_author`, `_description`, `_setFunctionPointers(TS3Functions)`, `_init`, `_shutdown`. Coalition declares API 26; ACRE and TFAR ship compat shims (`api_compat.asm`) for several API versions.
- **Voice hooks**:
  - `onEditPlaybackVoiceDataEvent(sch, clientID, samples, count, channels)`: per talker, before 3D positioning.
  - `onEditPostProcessVoiceDataEvent(..., channelSpeakerArray, channelFillMask)`: per talker, after positioning, multichannel.
  - `onEditMixedPlaybackVoiceDataEvent`: final mix.
  - `onEditCapturedVoiceDataEvent(..., int* edited)`: own mic; bit 2 = will be sent.
- **Control**:
  - `setClientSelfVariableAsInt(CLIENT_INPUT_DEACTIVATED)` + `flushClientSelfUpdates`
  - `get/setPreProcessorConfigValue("vad")`
  - `requestMuteClients` / `requestUnmuteClients`
  - `requestClientMove`, `guiConnect`
  - `sendPluginCommand` / `onPluginCommandEvent`
  - `CLIENT_NICKNAME`, `CLIENT_META_DATA`
  - `systemset3DListenerAttributes`, `channelset3DAttributes`, `onCustom3dRolloffCalculationClientEvent`
  - `onTalkStatusChangeEvent`

## 8. Points that will matter for a Reforger TS3 mod (considerations, not design)
1. **Bridge**: only script-reachable IPC exists. The options are Coalition-style files (proven) or `RestContext` to a localhost HTTP server inside the plugin (unverified; needs testing).
2. **Profile path**: the plugin must find the Reforger profile folder. Coalition hardcodes it; Workbench/`-profile` users differ.
3. **Vanilla VON must be suppressed**, otherwise voice goes out twice (engine VON + TS). Coalition overrides `ActivateVON/DeactivateVON`. 506th shows the alternative: keep engine VON and enhance it.
4. **Radio model**: reuse vanilla `BaseRadioComponent`/`RadioTransceiver` items (frequency in kHz, range, encryption key, existing UI/radial menu), or a custom component like `CVON_RadioComponent`.
5. **Where to decide audibility**: server (Coalition, anti-cheat friendly, extra RPC hop), sending client (ACRE-like), or each receiver's plugin (TFAR, needs full world state per client).
6. **Identity**: server-replicated TS id (Coalition), TS plugin-command broadcast (ACRE), or nickname (TFAR, fragile).
7. **Console crossplay**: Xbox/PS players can't run TeamSpeak, and PC-only servers need TS checks (Coalition's `m_bTeamspeakChecks`).

## 9. Bridge latency measurements (Workbench 1.8.0.13, 2026-09-14)
Test code: the game side was `Scripts/Game/RRT/Spike/RRT_BridgeSpike.c`. The plugin side was `rrt_spike.c`, `rrt_bridge_http.c` and `rrt_bridge_file.c`, run by a standalone `rrt_bridge_host.exe`. All of it was deleted once these measurements were recorded.

| Route | Round trip | Notes |
|---|---|---|
| JSON files in `$profile:RRT/` (plugin polls every 1 ms, writes response atomically) | avg 10–12 ms, p95 13–19 ms | about 2 frames at 6–9 ms per frame; both directions |
| `RestContext.POST` to `http://127.0.0.1:47777`, awaiting each reply | avg ~103 ms, flat for 18 B and 16 KB bodies | plugin handles each request in under 1 ms; turning off delayed ACK changed nothing |
| `RestContext.POST` without awaiting, every 20 ms | arrivals in bursts of ~5 every ~108 ms; send delay 0–100 ms sawtooth; reply avg 70 ms | the engine batches **outgoing** requests on a ~100 ms tick |

What the engine sends: HTTP/1.1, `Transfer-Encoding: chunked` bodies, `Connection: Upgrade, HTTP2-Settings` / `Upgrade: h2c` (ignoring the upgrade is fine), `User-Agent: Workbench/<version>`, `Content-Type: application/x-www-form-urlencoded`. `RestContext.SetTimeout` only accepts 3–600 s.

**Decision:** use file IPC for the game↔plugin bridge. RestApi's ~100 ms batching makes it unsuitable for PTT, positions or talk state.
