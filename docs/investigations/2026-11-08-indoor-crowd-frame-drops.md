# Frame drops with many people inside buildings (2026-11-08)

Report (Duke): Lima Charlie causes frame drops when many people are inside buildings at the same time.
Scope: `main` = 2.0.6 (18d6afe), client logs in `Documents\My Games\ArmaReforger\logs`, ops server bundle
`cmw63k8ch0a86sc9r5gzi9sa3` (2026-11-07). Code read: `LC_Rooms.c`, `LC_Occlusion.c`, `LC_GameStateWriter.c`,
`LC_Client.Tick` callees, `LC_AIHearing.c`, plugin `lc_reverb.c`. **No timing was taken in this pass.**

## What the logs show
| Log | Server | LC | Room diag | Listener | Nearby |
|---|---|---|---|---|---|
| `logs_2026-11-07_17-46-38` (17:46–20:47, crashed: 1.9 physics, see Wiki) | [IPSK] Operations | 2.0.6 | **on** from 18:14 | indoors 19:59–20:05 Garage_USSR_02, 20:07 Workshop_02_grey, 20:12–20:31 Barracks_USSR_01, 20:19–20:20 Bunker_SPS, ~20:43 GarageMilitary_E_01 (all 5 mapped and saved to `$profile` 20:02–20:43); outdoors otherwise | max 4 |
| `logs_2026-11-08_17-35-37` (17:43–, still running at 17:59) | OTAC Primary | 2.0.6 | off | outdoors throughout | 24–29 |

8 Nov in detail (triage + every `[LC]` line to 17:59): no LC script errors; compile OK 8137 files / 16273 classes.
Every minute: 889–1022 writes (15–17 Hz: someone in the 28-peer channel is nearly always talking), 2,100–3,300
trace pairs (35–55/s, a fifth of the cap), muffle by rooms 0, room probes 0, `listener outdoors | layout none`. No
building line at all. **Duke says he was in a building near the start**, so LC failed to see that building:
regular diagnostics print `[LC] rooms listener <prefab> room N` within 3 s of the room model placing the listener
inside, and no room file in `$profile:LimaCharlie/rooms` was touched on 8 Nov. Map: **Ruha** (`GM_Ruha.ent`,
Workshop `Ruha_653CB36244ADBE0F`). Its building prefabs (extracted to `References/ARMods/Ruha_653CB36244ADBE0F`)
inherit vanilla `Building_Base.et`, which carries `WorldSubsceneComponent`, so the component is there. More likely
the mesh has no interior areas (`FindArea` 0 everywhere) or the building is not STATIC to the sphere query. Not
checked in game. Either way, in that building LC ran traces only (budget-capped, 31 pairs/s that minute), with no
mapping, paths or room muffle. It cannot be where a room-model cost came from. The plugin's peer
count wobbles 26↔28 about every 10 s (73 lines). That is people joining, dying and respawning (`LC_PEER_MAX_AGE_MS`
35 s is well above the 10 s hello), and it is only a log line, not a performance issue.

- No LC script errors. Client logs carry **no frame-time data**, so they cannot confirm or rule out LC.
- Duke's client never ran the indoor path with a crowd: on 7 Nov it was indoors with ≤4 nearby; on 8 Nov (28
  nearby) LC never placed it in a building (below). The 8 Nov session ran ~16 writes/s and ~38 trace pairs/s, well
  under the 240/s cap.
- **The ops server sets `roomDiagnosticLog: true`** in its `server.json` (server script.log 18:38:58 lists it in the
  overrides; the 11-07 client prints `room diagnostic log true` on 5 of 5 ops joins). Every client on an op runs the
  room diagnostic.

## Live test, 8 Nov 18:13–18:17 (OTAC on Ruha, Duke in a building ≈ 7449, 3547)
Watcher on `game_state.json` (2 Hz: position, `roomVolume`, muffled players) plus the regular summary lines.
- Duke: single-digit FPS inside; **standing still is fine, turning the camera or moving is bad**.
- LC never placed him indoors: `roomVolume` 0 throughout, summary `listener outdoors | layout none`, room probes 0.
  So no mapping, paths or room diagnostic ran. All ~20 nearby players were muffled by traces.
- LC writes at most once a frame, so writes per minute is an FPS proxy below 20 FPS. Minute to 18:14:51 (inside):
  **572 writes (≈9.5 FPS)**, 2,015 trace pairs (34/s, ≈3.5 per write), *fewer* traces than the minute before
  (4,375). At the measured ~13 µs a pair this is well under 1 ms a second.
- A/B: Duke unticked the LC plugin in TeamSpeak. 18:16:23 `Plugin stopped answering`; game side dropped to ~2.2
  writes/s (11 seqs in 5 s), so its work fell 5–10×. The TeamSpeak side (5 ms polling of game_state.json, per-voice
  DSP) also stopped. Duke, same building: **"better, but not by much. Not single digits, still very bad."**
- Reading: LC (game side plus plugin) is a small part. Most of the drop stays with LC nearly idle. The camera
  sensitivity points at rendering or CPU-side culling of the crowd and the interior. Confounds: different
  minute, crowd not identical, no frame-time numbers, only Duke's impression. The small gain may also be the
  TeamSpeak process freeing CPU, not the game thread.

## Recorded session, 8 Nov 18:23:46–19:18:39 (OTAC on Ruha, until Duke left)
Recorder (session scratchpad `lc_watch.py`, not kept in the repo): every `game_state.json` write with its mtime gap,
a per-second row, every `[LC]` line. Below 10 FPS LC writes once a frame. Above that, a gap between writes is
under interval + one frame, so a gap over 150 ms means a frame of at least ~100 ms (a hitch).
3,276 seconds with the plugin up: a gap over 150 ms in 4.3% of seconds, fewer than 10 writes in 0.9%.
**LC placed Duke indoors for 0 seconds** of the whole session.

| Place (100 m cell) | Time there | Players nearby (avg) | Seconds with a hitch |
|---|---|---|---|
| x 6100 z 4300 | 236 s (19:14–19:18) | 6.3 | 15.7% (slow patch 19:14:43–50, 3 FPS) |
| x 6200 z 4300 | 432 s (19:04–19:14) | 8.9 | 12.7% |
| x 6500 z 4100 | 185 s | 9.6 | 10.8% |
| **x 7400 z 3500 (FOB building, ≈ 7449, 3542)** | 163 s | **3.2** | 8.6% |
| x 6400 z 4100 | 476 s (18:46–18:55) | **8.5** | **0.4%** |
| eight other cells | 60–243 s each | 0.5–5 | 0–1.5% |

- **Hitches follow the place, not the crowd.** 6400/4100 had ~8.5 players nearby and almost no hitches. The FOB
  building had about 3 and still hitched (95th-percentile gap 128 ms there after the 18:55 respawn, against 78–84 ms
  elsewhere). By player count alone, 10–14 nearby looks bad (20%), but nearly all those seconds are in the bad
  6100–6200 cells.
- **Radio is not the trigger:** hitch seconds 6.2% with no radio, 3.0% with one reception, 2.4% with two, 3.8%
  transmitting.
- 19:18:40: not a freeze. Pause menu 19:18:37, `scenario_exit` 19:18:38, disconnect 19:18:40.
- Duke (18:55, respawn): "building at FOB is particularly bad, even with few people."

Conclusion of the live work: the frame drops Duke sees on Ruha are tied to particular buildings and areas. LC's
room model never engaged there (LC saw no building at all), and its trace and write work was small and no higher
in the bad places. LC is at most a minor share (plugin-off A/B: "better, but not by much"). Still open: what the
FOB building is (scenario-placed, so not STATIC to LC's query? unverified). Also the ops-server room diagnostic
(#1 below), which these OTAC tests could not exercise because it was off there.

## Duke's hypothesis: occlusion rechecks when moving (8 Nov, after the test)
- Origin: traced from the listener's **eyes** (`EyePosition`), not the camera, unless the camera is > 20 m from the
  body (free camera). A pair is retraced once either end moves 0.15 m, after 50 ms (talking) or 250 ms (silent);
  standing still, once a second. Turning in place can move the eyes past 0.15 m (unmeasured), so moving and turning
  both raise the trace rate. The hypothesis fits the "still is fine" symptom.
- **Correction to the plugin-off A/B:** with the plugin off, LC still writes at 2 Hz and each write still has the
  full 12-pair trace budget, so traces could still run at up to 24 pairs/s against ~34/s in the bad minute. That
  A/B cut writes 5–10×, but traces only by about a third. It does not clear traces.
- Recorded data, per minute (54 minutes): trace pairs against hitch seconds, correlation **0.34**. Confounded:
  traces scale with players nearby. Heavy-trace minutes elsewhere were clean (18:41 2,989 traces / 0 hitch seconds;
  18:42 2,496 / 0; 18:45–18:49 1,500–1,800 / 0). The FOB minute to 18:55:52 had 771 traces and 10 hitch seconds.
  So trace *volume* does not cause hitches. Not ruled out: a trace *through particular buildings* (the FOB, the
  x 6100–6200 z 4300 area) costing far more than the 13 µs measured in the apartment.
- No server setting turns occlusion off (keys: gameMasterUnlimitedRange, aiHearing, unconsciousCanSpeak,
  roomDiagnosticLog, hideRadioRoles, hideDirectSpeechDisplay, diagnosticLog), so there is no in-game A/B
  without a mod update.
- Replication: LC sends 4 RPCs (session request, settings, session once; an unreliable voice report once a second
  while talking). It reads positions the engine already replicates. Nothing per frame goes over the network.
- Network, full list (grep of `Rpc(`, `RplRpc`, replication and radio setters in `addon/Scripts/Game/LC`):
  `RpcAsk_LC_RequestSession` → `RpcDo_LC_ReceiveSettings` + `RpcDo_LC_ReceiveSession` (once per connection, retried
  every 2 s until answered); `RpcAsk_LC_ReportVoice` (unreliable, once a second while you talk). On the server that
  triggers `LC_AIHearing.Broadcast` → AI danger event → `CarriesThroughCover`, up to 2 traces per enemy AI in range.
  That is server CPU, which shows as desync, not client FPS. `LC_SetFrequency` → vanilla
  `RadioTransceiver.SetFrequency` (vanilla replication, only when you change channel). `LC_Radio` reads the radio's
  `RplComponent` id locally (no traffic). Traces, sphere queries and room lookups are local physics and world
  queries with no network side. The one indirect link: other players' positions arrive by vanilla replication, and
  each 0.15 m of their movement makes their pair due for a retrace.
- Server-side occlusion would be worse. The server would trace every listener-speaker pair (N², 3,600 pairs for 60
  players) and send each client its own row. That costs bandwidth, adds latency, and loads the server, whose FPS
  every player feels. Clients already have the geometry streamed. TFAR and ACRE also trace on the client.
- Next, once the mod can be updated: time each write (`System.GetTickCount` around `BuildInGameJson`) and log
  writes over a few ms with trace count and place. Spread traces over frames (1–2 a frame, not 12 in one write
  frame). Leave rotation out of the listener move test (body origin, or a larger threshold).

## Confirmed LC (Duke, 8 Nov evening)
- **"Lag disappears whilst in the same building, with similar player numbers, without Lima Charlie running. It is
  an LC issue."** This overrides the "LC is a minor share" reading above. That reading leaned on the plugin-off
  A/B, which did not stop the traces.
- Per-write gap by players nearby, from the recording (share of writes followed by a gap over 150 ms):

  | Place | 1–2 nearby | 3–5 | 6–9 | 10+ |
  |---|---|---|---|---|
  | FOB building (7440–7470, 3530–3570) | 0.2% | **3.7%** | (23 writes) | – |
  | x 6100–6300 z 4300–4400 | – | 0.1% | 0.7% | **3.9%** |
  | x 6400–6600 z 4100–4200 (clean) | – | 0.0% | 0.0% | 1.0% |

  The cost is **per nearby player, and the cost per player depends on the place**. Of LC's work, that fits per-pair
  work done against local geometry: occlusion traces (`LC_Occlusion.CountObstacles` → 2 `TraceMove` with the
  `FilterCover` script callback) and, second, the speaker's `LC_Rooms.Locate` → `Search` sphere query
  (outdoors-as-seen-by-LC every ≥150 ms while moving). Traces fit "turning is bad" better, since the listener's
  eye moving 0.15 m retraces every pair. Locate needs 0.5 m.
- The place: mission **OTAC Operation Borderless OP_Bord7** (`Worlds/OP_Bord7.ent`, Workshop
  `6A6284A193140945`, extracted to `References/ARMods/OTAC-OperationBorderless_6A6284A193140945`). Its
  `OP_Bord7_Layers/HQ.layer` places furniture, crates, an M2 and IFAKs 6–22 m from 7449,3542, and the US HQ spawn
  point 29 m away. The building shell is not in the HQ layer, and no top-level Ruha map entity is within 35 m (Ruha
  nests children under parents, not parsed). So it is a Ruha house the mission furnished, or something nested.
  Unresolved.
- Suspected mechanism, unverified: a trace through a furnished interior meets many small static props (tables,
  crates, chairs, IFAKs, ammo, the HQ arsenal). Each one is a `FilterCover` script callback, rejected by width, and
  the trace carries on. If the engine re-casts after each rejection, cost grows with props crossed × pairs.
  Twelve pairs in one write frame could then hitch. The 13 µs apartment figure was measured in an empty building.
- Oracle: Workbench play mode on `OP_Bord7.ent` (ClaudeDevTools, dependency OTAC-OperationBorderless + its
  Workshop deps via `-addonsDir`). Time `CountObstacles` and `Locate` from 7449,3542 to points 5–60 m around,
  count `FilterCover` calls per trace, and compare with the 6400,4100 area.

## Not buildings: outdoors too (Duke, 8 Nov evening)
- Duke: **"The frame issues happened extensively outside the building as well"**. He thinks building-path caching
  is a separate problem. The logs agree that room paths are not the cause here. Over the whole recording LC placed him
  indoors for 0 s, with room probes 0 and muffle-by-rooms 0 in every summary, so no mapping, path search or portal
  reads ran. The hitches come from LC's always-on per-player work: `Locate` (sphere query for anyone LC
  sees as outdoors), `CountObstacles` (1 trace for a clear pair, 2 for a pair behind cover), the JSON and the file write.
- Seconds with a hitch, by players nearby and by how many of them are behind cover (muffled > 0):

  | Players nearby | 0 muffled | 1–3 muffled | 4+ muffled |
  |---|---|---|---|
  | 1–5 | 0.3% (912 s) | 3.2% (474 s) | – |
  | 6–9 | 0.4% (514 s) | 2.9% (489 s) | 5.7% (262 s) |
  | 10+ | 26.4% (106 s) | 24.1% (108 s) | 15.8% (240 s) |

  Below 10 players, hitches follow the number of occluded pairs, each of which costs a second trace. From 10 players
  they are high whatever the cover. That is where the 12-pair budget fills every write, and where per-player
  `Locate` and JSON grow too. **Prime suspect: the occlusion traces**, and what they cost on Ruha's ground (very dense
  vegetation and fence layers, plus props: every entity a trace meets is a `FilterCover` script callback before
  it is rejected).
  Second: `Locate` sphere queries. Unverified until timed.
- Room paths and mapping stay a separate question (see `2026-11-03-room-occlusion-performance.md`).

## What scales with people indoors (2.0.6)
Bounded by design: one write per 50–100 ms, at most 12 trace pairs a write (`OCCLUSION_BUDGET`), mapping only for the
building the local player stands in. Measured costs are in `2026-11-04-whole-mod-performance.md`.

| # | Candidate | When | Cost | Evidence |
|---|---|---|---|---|
| 1 | **Room diagnostic collider volume.** On the write that prints the line (1/s), every trace refreshed in that write calls `DescribeCover(true)` → `DescribeSolidity` → `MeshObjectVolumeCalculator.GetVolumeFromColliders` on its blocker (`LC_GameStateWriter.TraceMuffle`, `LC_Occlusion.c:223`). Indoors the blocker is the building itself (whole apartment block). With a crowd moving indoors, the budget is full: up to 12 walks of every collider of the building in one frame, once a second. The line only lists 6 players, so up to 6 of them are wasted. | Only with `roomDiagnosticLog` on: true on the ops server | **Unmeasured.** Walks every collider of the entity | Code; ops server setting |
| 2 | Mapping a building type the client has not mapped (`LC_RoomLayout.Build`) | First time each client stands in that type; saved to `$profile` after, reset by a game version change | 0.45 ms/write (house) to 1.13 ms/write (apartment), ~3 ms on a save write every 4,000 cells; apartment ≈ 1,433 writes | Measured 2026-11-04 (WB 1.8) |
| 3 | Trace budget kept full by a moving crowd indoors (room answer > 0 is an upper bound, so the pair is still traced) | Building clears, people shuffling | 12 × 13 µs ≈ 0.16 ms/write, measured in an empty building; a crowd adds filter callbacks per character met | Measured empty; crowd unmeasured |
| 4 | game_state.json write | Every write, 20 Hz while anyone talks | ~0.4 ms (Python proxy) | Not indoor-specific |

Ruled out by the code read:
- Per-speaker room work is a `FindArea` (0.1 µs) or a sphere query (2 µs) every ≥150 ms, plus a cached path lookup
  (1.45 µs). Path slots (32) and cost tables (16) do not thrash at plausible counts.
- Plugin room reverb is one shared bus (`lc_reverb.c`, 2 channels), not one per speaker, and it runs in TeamSpeak's
  process, not on the game thread.
- AI hearing (server) runs at most once a second per talking player, without the diagnostic.

## Conclusion
Only #1 fits the report: it is indoor-only, it grows with how many people are being traced, and it is on for every op.
It would show as a hitch about once a second. Its size is the open question. #2 is real but small and once per
building type per client. #3–4 stay under ~1 ms a write going by the measurements we have.
Nothing found in steady-state LC work with diagnostics off comes near a visible frame drop. If drops stay with the
diagnostic off, look at vanilla next: characters indoors, interior lights and shadows, vanilla interior audio.

## Oracles (cheapest first)
1. Ops server `server.json`: `"roomDiagnosticLog": false`, then the same indoor crowd. Drops gone = #1.
2. Workbench bench (ClaudeDevTools, as on 2026-11-04): time `GetVolumeFromColliders(apartment, Projectile)` in play
   mode. ≥1 ms each means #1 is a 5–12+ ms spike.
3. If drops remain: an A/B with LC unloaded in the same building with the same crowd, and frame times (`System.GetFPS`
   min per minute could go into the regular summary line, beside LC's own write time from `System.GetTickCount`).

## Fix options for #1 (not done; Duke's call)
- (a) Measure solidity only for the players the line prints (≤ `DIAGNOSTIC_MAX_PLAYERS`), or only for the first one.
- (b) Drop `DescribeSolidity`. It was an experiment to separate see-through railings from walls. The width test and
  the name list are what ship.
- In both cases: keep `roomDiagnosticLog` off on the ops server unless troubleshooting (docs already call it "the
  most expensive diagnostic").

## Rework: how vanilla VON occludes, and how LC could trace far less (8 Nov, design only)
Duke asked whether occlusion can be reworked along vanilla VON's lines (it knows in-vehicle, indoors, other person
indoors) with fewer raycasts.

### Vanilla VON uses no traces at all
`Sounds/VON/von.acp` feeds a low-pass cutoff from `Sounds/_SharedData/Signals/Character/Character_Occlusion.sig`.
Inputs: per voice source `Interior` (0–1), `RoomSize`, `IsInPlayerVehicle`, `UnderPlayerControl`; listener globals
`GInterior`, `GRoomSize`, `GVehicleInterior`, `GCurrVehicleCoverage`. The graph:
- building = (source `RoomSize` ≠ `GRoomSize`) × not the player's own character × ramp(max(`Interior`, `GInterior`)
  from 0.5 to 1). "Same room" is literally "same room size value"; both outdoors = clear.
- vehicle = `GCurrVehicleCoverage` × (in the player's vehicle or not, weighted by `GVehicleInterior`). Coverage comes
  from each compartment's `BaseCoverage` in the vehicle prefab (UAZ469 driver 0.15); there is no script getter.
- Sum → exponential curve → cutoff frequency.
So vanilla classifies each end once (a point query), never the path between them. Walls between two people who are
both outdoors are ignored, and so are two rooms of the same size.

What script can use from this (vanilla precedent for each):
- Listener: `AudioSystem.GetVariableValue("GInterior"|"GRoomSize"|"GVehicleInterior"|"GCurrVehicleCoverage",
  "{A60F08955792B575}Sounds/_SharedData/Variables/GlobalVariables.conf")` — the engine updates them every frame
  for the camera (`SCR_BuildingSoundComponent.c:135`, `SCR_SoundCinematicTrack.c:114-121`). Free to read.
- Anyone else: `ChimeraWorld.GetSoundWorld().CalculateInterirorAt(pos, InteriorRequestCallback)` → async
  `OnCompleted(float interior, float roomSize)` (`SCR_AudioSource.c:124`). Vanilla calls it for **every non-exterior
  sound it plays** (`SCR_SoundManagerModule.PlayAudioSource`), so it is built to be called often and its work is
  off the script thread. Cost and update latency unmeasured (bench).
- Unknown: whether the engine's interior works in Ruha's / OTAC's buildings, where LC's own room model found none.

### Why LC costs what it does
Per nearby player (≤60 m) LC traces the path: 1 `TraceMove` if clear, 2 if blocked, and every entity a trace meets
calls back into script (`FilterCover`: bounds, up to six casts, then a prefab-name lookup). On Ruha's vegetation and
FOB props that is many script calls per trace. Scheduling multiplies it:
- **Silent players are traced too**: every 250 ms once either end moves 0.15 m, else every 1 s
  (`LC_GameStateWriter.c:38-41`, `TraceMuffle` :361). Nobody hears a silent player, so this is almost all waste.
- **The listener moving makes every pair due at once** (`moved` tests the listener end too), so up to the 12-pair
  budget fires in one frame, 4×/s while silent, 20×/s while someone talks.
- **The listener point is `EyePosition()`** (`:559-563`). Turning the head or camera in place moves the eye a few cm
  to ~0.2 m, enough to pass 0.15 m: this fits "standing still is fine, rotating the camera is bad".
- The budget is a pair count, not time, and all of it lands in one frame.

### Rework options, cheapest and safest first
1. **Trace only players who are talking** (plus a prime on talk start). Silent players keep their last value and
   go stale. With 20 nearby and 1–3 talking, traces drop roughly 10×. No change to what anyone hears.
2. **Move test on the body, not the eye**: listener and speaker points for the move test = entity origin (+ fixed
   height); keep the eye for the trace itself. Rotation stops triggering retraces. Raise `OCCLUSION_MOVE_M` to ~0.5 m.
3. **Spread the budget**: at most 2–3 traces per frame (rotate through players), and a time cap measured with
   `System.GetTickCount` rather than a pair count.
4. **Classify ends first, trace only when it matters (vanilla's idea)**: per player keep an interior/roomSize
   state (LC rooms where they resolve, else `CalculateInterirorAt`, refreshed on ≥1 m moves or every 2 s; listener
   from `GInterior`/`GRoomSize`). Then: same room → 0, no trace; one indoors and one out, or different rooms → a
   muffle without a trace (or one confirming trace for talking players only); both outdoors → trace only talking
   players within, say, 30 m (outdoor walls are rare; vanilla ignores them). Most pairs then cost zero traces.
5. **Fewer script callbacks per trace**: set `TargetLayers` to layers that hold walls and hulls but not
   `Vegetation`/`Foliage`, so trees never reach `FilterCover` (needs a check of which layers buildings, fences and
   vehicles use; vanilla sets `TargetLayers = FireGeometry` in `SCR_TerrainHelper.c:181`). Or drop the reverse trace
   and call any blocked path one obstacle unless the room model says otherwise.
6. **Vehicles**: listener side from `GVehicleInterior`/`GCurrVehicleCoverage`; same vehicle already returns 0
   without a trace (`LC_Occlusion.c:60`).

1–3 are small, local changes to `TraceMuffle`/`GetSpeakerPosition` with the same results; 4–5 change behaviour and
want Duke's ear test. All of it waits until the mod can be updated. Oracle: the bench (time per write and
`FilterCover` calls per trace at the bad FOB spot, before and after), then a recorded session like 8 Nov.

### Implemented (8 Nov, branch `perf/occlusion-traces`, not built or tested in game)
Duke: implement everything except muffling indoor/outdoor pairs without a trace ("I don't want people standing in
doorways to be muffled"). Indoor/outdoor stays traced: a trace through an open doorway is clear, which is the
"better way" for doorways. All in `LC_GameStateWriter.c`:
1. Talkers only: a talking player is traced at once when speech starts, then every 50 ms while a body moves, else
   every 1 s. Silent players get one trace a write at most, only while nobody at all is talking, every 2 s moving
   / 10 s still, so speech does not start from a result minutes old (the plugin glides muffle over 200 ms).
2. Move test on bodies (`GetOrigin`, or the free camera), 0.5 m. Traces still run eye to eye.
3. 3 traces a write (writes ≤ 1 a frame), stop after 2 ms of tracing, players visited from a rotating start.
4. Same room: clear, untraced (as before). Different rooms: room muffle, traced only for talkers. Indoor/outdoor:
   traced. Both outdoors > 30 m: clear untraced, only if LC rooms + engine (`GInterior` < 0.5 for the listener,
   `CalculateInterirorAt` < 0.5 for the speaker, answer within 2 m of where they are) agree and neither is in a
   vehicle. Pending requests are owned by a static array until answered (vanilla never frees one pending).
5. **Not done: a layer mask that keeps trees out of `FilterCover`.** Vanilla's layer presets (`ArmaReforger.gproj`)
   give `Tree` and `BuildingView` the identical mask 0xa08002 (`TreeFireView` = `BuildingFireView` = 0xa0c002), so
   no mask separates trees from buildings. Props (0xa00300) could be masked out, but HESCO/sandbag/fence presets
   are in their .xob and unverified, so that would risk losing muffle behind FOB walls. The reverse trace stays: it
   now only runs for blocked pairs of talkers.
Diagnostics: summary line gains `far outdoors N` (also counted in "by trace"); room line gains `e0.00` (engine
interior) when one was asked for, and `[far outdoors]`.
Oracles: Workbench compile + `reforger_triage_logs`; Duke's ear test (doorway, wall, both outdoors near/far,
start of speech through a wall); a recorded session at the FOB like 8 Nov (hitch %, `traces` per minute).
Unknowns: `CalculateInterirorAt` cost/latency and its values in Ruha buildings (the `e` field will show them).

### Vehicles: vanilla vs LC
Vanilla muffles only by the **listener's** seat: `GCurrVehicleCoverage` (the seat's `BaseCoverage`, UAZ driver
0.15) muffles everyone outside your vehicle; people in it are clear. A speaker inside a closed vehicle is not
muffled for someone outside by that term. LC's trace hits a hull at either end, so speaker-in-APC, turned-out
hatches and open mounts all come out right, at the cost of traces. Vanilla's is cheaper and seat-aware, not better.
Possible hybrid (not done): skip the trace when the listener's own `GCurrVehicleCoverage` says the seat is open.

### Room lookups budgeted too (10 Nov, same branch, static check only)
Hitches at 10+ players did not depend on how many were muffled, which points at per-player work: `Locate` ran a
sphere query per nearby player every 150-500 ms with no cap. Now: 3 building searches a write (`SEARCH_BUDGET`; the
`FindArea` fast path for someone still in their building is not counted), an unserved location stays due, silent
players are rechecked every 1 s moving / 3 s still and at once when they start talking.
Worn gear: ~2,500 blocked traces in the 7 Nov room lines, none blocked by gear or weapons. Whether gear costs
`FilterCover` skips is unknown (skip counts 1-11 a trace, contents not logged). Camo nets block traces (bug).
