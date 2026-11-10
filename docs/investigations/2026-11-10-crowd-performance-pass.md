# Crowd performance and bugfix pass (2026-11-10)

Scope: whole mod on `perf/occlusion-traces` (after e5aa652), read for work that grows with player count.
Asked by Duke: raycasts against props and worn gear, the cost of silent players, how others handle it.
**Nothing here was timed.** Compile evidence only; see the worklog entry of the same date.

## How others do it (from source, `References/`)
| | Who is processed | Occlusion | Budget |
|---|---|---|---|
| TFAR (`addons/core/functions/fnc_processPlayerPositions.sqf`, `defines.hpp`) | Near list (voice range + 40 m) rebuilt every 1 s; far players every 3.5 s. Silent near players are sent like talkers. | One `lineIntersectsSurfaces` eye to eye, max 10 hits, both units and their vehicles and any player skipped; cached once per pass | 30 near / 50 far a call, stop after 5 ms |
| ACRE2 (`addons/sys_core/fnc_speaking.sqf`, `fnc_findOcclusion.sqf`) | **Only the speakers list**, filled by the plugin's start/stop speaking callbacks. Silent players cost nothing. | One ray first; if blocked, a costly detour search, cached 0-0.33 s with a random expiry; none beyond 150 m | none |
| Coalition VON 2.1.30 (Workshop) | Everyone in range, every fixed frame | Head-to-head plus roof and window traces every tick; skips hits with `BaseLoadoutClothComponent` | none |
| Coalition VON (GitHub, Oct 2026) | In-range list every 0.25 s; gains every 0.05 s, non-talkers every 4th tick | Talkers every 0.25 s, others 1 s, staggered by `playerId % 5` | roof traces capped at 4 |
| Vanilla nametags (`SCR_NameTagRulesetBase.c:284-415`) | Round-robin index over tags | Camera layer first, Projectile retry with a filter whose verdict is cached per prefab | 3 traces a frame |
| Vanilla VON | n/a | No traces: per-end interior values (8 Nov write-up) | n/a |

Same three ideas everywhere: a near list rebuilt slowly, full-rate work only for talkers, and a per-frame cap
with a rotating start. LC already had the cap and talker-only tracing (8 Nov); this pass adds the other two.

## Worn gear and props in traces
- Vanilla never relies on `Exclude` for attachments: its filters walk `GetParent()` and drop anything attached to
  a character (`SCR_MeleeComponent.c:89-103`, `SCR_BaseGameMode.c:1179-1219`, `SCR_WeaponBlastComponent.c:234-270`).
  Helmets and armour vests carry `PhysicsOnWearEnabled 1`, so their colliders are live while worn.
- So a Projectile-mask trace between two heads meets both people's own helmets and vests, and those of anyone
  standing between: one `FilterCover` call each. That fits the 1-11 skips a trace in the 7 Nov logs.
  All of it is under 0.8 m wide and was already rejected by the width test (one `GetWorldBounds`). No trace was
  blocked by gear in ~2,500 logged.
- The gap was anything **carried and long**: a rifle or launcher is over 0.8 m, passes the width test, and
  could read as a wall. `IsNeverCover` now walks the parents like vanilla. Only wide entities reach that walk.
- Camo nets blocked traces. `CamoNet_` added to the see-through names (matches `Structures/Military/CamoNets`,
  not the canvas tents named `..._camonet`).
- Not done: caching the filter verdict per prefab (`GetPrefabName` is called for each wide entity a trace stops
  at, 1-2 a trace, at most 60 traces a second). Vanilla caches by prefab name string, which needs the same call.
- Not usable: `BaseWorld.AsyncTraceMove` takes no filter callback, so trees and props would block it.

## Silent players
Before: every write (10-20 Hz) looked up every player on the server (entity, eye position, distance), and for
each one within 60 m ran the room lookup, the muffle decision and built their JSON entry, talking or not.
With 20 Hz held whenever anyone anywhere was talking, a full server ran at the fast rate nearly all the time.

Now (`LC_GameStateWriter.BuildInGameJson`):
| Player | Per write | Refreshed |
|---|---|---|
| Out of range | one map lookup | when they could be back: (distance - 60 m) / 60 m/s, at most 2 s |
| Not streamed in | one map lookup | every 500 ms |
| Nearby, silent | one map lookup, one string append (their last entry) | 4 Hz, staggered by player id |
| Nearby, talking | everything, as before | every write |

The listener jumping more than 10 m between writes (respawn, teleport, fast free camera) makes everyone due.

Responsiveness when a silent player starts talking: the plugin plays them at once from their last entry (at
most 250 ms old: about 1.5 m for someone sprinting, more in a vehicle). The game learns of it at its next plugin
state poll (50 ms), which now forces a write on that frame with a fresh position, room and trace. Before, that
write waited for the schedule (up to 100 ms). The plugin glides muffle over 200 ms as before.

Write rate: 20 Hz only while someone nearby talks or the local player does; otherwise 10 Hz. Radio voices have
no position, and their terrain links already force a write when they change.

## Server
`LC_AIHearing`: each talker reports once a second, and every enemy AI within the voice radius ran a cover check
(up to 2 traces). Unbounded in talkers x AI. Now no trace when the AI is within the two-wall range (the answer
is yes whatever is found), and 60 checks a second across the server; past that, one wall is assumed.

## Layer filter bench (10 Nov, evening): not worth doing
Question: would `TraceParam.TargetLayers` keep gear, weapons, characters and bushes away from `FilterCover`?
Bench `bench/LCBench_Layers.c`, raw output `bench/2026-11-10-layers-bench-raw.txt`. Workbench play mode on
GM_Arland, 600 random head-height rays of 4-60 m in each of the two densest towns, then 24 rays through a
spawned `Character_US_Rifleman` and through an `M1025`. Timing is `GetTickCount` over 15,000 traces a variant
(the 24-ray body timings are too few to mean anything).

| Variant | Town 1 blocked / calls per trace / us | Town 2 | Verdicts changed |
|---|---|---|---|
| Projectile mask (now) | 207 / 4.01 / 9.40 | 125 / 4.03 / 7.73 | - |
| T Static\|Dynamic\|Vehicles\|Terrain | 215 / 3.98 / 8.67 | 132 / 3.98 / 7.53 | 8 and 11 of 600 |
| T FireGeometry | 207 / 4.01 / 9.53 | 125 / 4.03 / 7.67 | 0 |
| T FireGeometry\|Terrain | 207 / 4.01 / 9.47 | 125 / 4.03 / 7.73 | 0 |
| T Static\|Vehicles\|Terrain | 214 / 4.00 / 8.67 | 132 / 3.98 / 7.20 | 9 and 11 of 600 |

- **What reaches the filter is trees**: 58% of calls in town 1, 75% in town 2. Then destructible props,
  buildings, powerlines. No layer set removes trees (their colliders share the Static bit with buildings).
- **A geared rifleman is one call, not many**: 24 rays through him gave 24 `character` calls and no call for
  anything worn or carried, under every variant. So the "1-11 skips a trace" in the 7 Nov logs were trees and
  props, not gear, and the section above overstated what gear costs. One soldier prefab only; mod gear untested.
- The collision-layer variants still meet the character, save under 1% of calls, and change 1.5-2% of answers
  (plank fences, wooden walls and metal gates start to block; a stone wall and a container stop).
  `TargetLayers = FireGeometry` is the same as the Projectile mask in every number.
- The Humvee blocks 24 of 24 under every variant.
- Conclusion: a trace is about 8-9 us in a town with 4 filter calls in it, and the calls are not where the time
  goes. Fewer traces is the lever (earshot, reverse trace), not cheaper ones. Layer filter dropped.

## Earshot and the reverse trace (10 Nov, evening)
- Plugin publishes `ranges` (`id:metres,` per peer). Writer: a talker beyond their range + 5 m is handled as a
  silent player (no talker traces, no live room search, no fast write rate); beyond range + 10 m their cached
  entry is rewritten like any silent player's. Coming into earshot while talking counts as speech starting.
  Lag to expect: someone who starts shouting is treated as out of earshot until the plugin has passed the new
  range on (one plugin state write and one 50 ms poll).
- `LC_Occlusion.Compute(..., known)`: with a non-zero `known`, a blocked forward trace returns `known` and the
  reverse trace is skipped. Writer passes the cached muffle when it is > 0, not marked for retrace, and counted
  under 1 s ago. AI hearing still counts from both ends every time.

## Still open
- Unmeasured. Oracle: a recorded crowd session as on 8 Nov (hitch %, writes and traces a minute in the summary
  line), and Duke's ear test.
- The plugin's talking list is kept as last read when the plugin stops answering, so a talker at that moment
  stays "talking" until the plugin is back. Harmless at the 2 Hz no-plugin rate; not changed.
- `s_aInteriorRequests` is static; statics reset when a modded scenario ends, which would free requests the
  engine may still hold. Not observed to crash. Unverified either way.

## External handover compared (LC_Performance_Handoff_2026-11-10.zip, read 10 Nov)
Two outside reviews of 2.0.6 `18d6afe`: patch 0001 (game scripts), patch 0002 (plugin), two unfixed issues, offline
evidence. Their own caveat: nothing compiled in Workbench or run in game. Nothing from it is applied here.

Their field report adds something ours lacks: with 36-40 players the frame rate **degraded over time** (a
warehouse within 5 minutes, open fields within 20), only while moving, client only, no memory growth, and not
with vanilla VON. Their scheduler model shows the 2.0.6 trace count per minute is flat over time, so scheduling
does not explain a drift. Neither do their patches or this branch. Open: what in LC grows over a session.

| Their item | Checked here | Against `perf/occlusion-traces` |
|---|---|---|
| 0001 starvation: fixed player order + empty budget = players at the end of the list never traced, new samples stay "clear" | True of `main` (no rotation there) | Already fixed: rotating start (db35359) |
| 0001 traces spread over frames, 4 a frame, outside the write | n/a | Covered differently: 3 a write, writes at most one a frame, 2 ms cap, talkers only |
| 0001 move threshold scaled by distance, max(base, 5% of distance) | Not in the branch | **Worth taking**: a 0.5 m step at 50 m changes nothing. Small change to `TraceMuffle` |
| 0001 carried gear never cover (parent walk) | Same reading of vanilla | Already done (7224630). Their extra evidence: `FireGeo` preset strings in the M16 body and handguard, worn helmet and vest .xob files (a text search of binaries) |
| 0001 `GetPrefabName` runs before the see-through cache lookup | True (`IsSeeThrough`) | Unchanged; 1-2 calls a trace, at most 60 traces a second |
| 0001 room-diagnostic solidity allowance | n/a | Not needed while traces stay inside the write |
| 0002 biquad denormal guard in `lc_audio.c` `biquad_tick` | `git apply --check` passes on this branch; the reverb comb has the same guard at 1e-8 (`lc_reverb.c:140`); no FTZ/DAZ set anywhere in `plugin/src` | **Worth taking.** Their numbers: 39 muffled voices in silence 0.861 -> 0.306 ms per 20 ms block, same PCM hash. TeamSpeak's audio thread, not the game's. Not rebuilt here |
| Follow-up 1: `log_radio_levels` logs a line per received radio target every second with diagnostics off | True: only the 1 s timer guards it (`lc_core.c:576-582`) | Open. Needs the diagnostic level in `game_state.json` (additive field) |
| Follow-up 2: audio snapshot double buffer can be overwritten under a reader | Known from the 4 Nov pass; correctness, not speed | Open |

0001 as a whole does not apply to this branch (`LC_Occlusion.c`, `how-it-works.md` conflict) and should not be
applied: everything in it except the distance-scaled threshold is already here in another form.

Taken on Duke's go (10 Nov): 0002 as given, and the distance-scaled threshold written into this branch's
`TraceMuffle`. Plugin tests pass and the Game module compiles; neither was timed or run in game.
