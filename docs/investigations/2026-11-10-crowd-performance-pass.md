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

## Still open
- Unmeasured. Oracle: a recorded crowd session as on 8 Nov (hitch %, writes and traces a minute in the summary
  line), and Duke's ear test.
- The plugin's talking list is kept as last read when the plugin stops answering, so a talker at that moment
  stays "talking" until the plugin is back. Harmless at the 2 Hz no-plugin rate; not changed.
- `s_aInteriorRequests` is static; statics reset when a modded scenario ends, which would free requests the
  engine may still hold. Not observed to crash. Unverified either way.
