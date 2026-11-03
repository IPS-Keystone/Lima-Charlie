# Room occlusion and building paths: how they work, what they cost (2026-11-03)

Scope: `LC_Rooms.c`, `LC_Occlusion.c`, `LC_GameStateWriter.c` on `bugfix-2.0.6` (1d3e5f3), compared with `main`
(2.0.5, released). Question from Duke: how rooms and building paths work, outdoor listeners, caching, and
whether retracing, keeping or refreshing buildings costs performance, especially when people enter buildings
or use doors and windows.

Method: a code read done first, without the 2.0.6 session-log investigation. That investigation was read after
and compared at the end. Oracle: static op counts from source. Building sizes come from the 2.0.5 logs
(`Wiki/game/world-editor-traces.md`: apartment `ApartmentBuilding_USSR_5I01` = 117 areas, 310 portals, 185
usable). **Nothing here was timed in game.** Costs are call counts, not milliseconds.

## How it works

Everything runs on the client, once per game-state write: 20 Hz while anyone talks or you transmit, 10 Hz idle,
2 Hz without the plugin. The server is not involved. Only AI hearing traces there, and it uses no rooms.

| Piece | What it does | When | Kept |
|---|---|---|---|
| `Locate` (`LC_Rooms.c:320`) | Which building and area a point is in | Listener and each speaker within 60 m, at most every 500 ms **or** after 0.5 m of movement | `LC_RoomLocation` per player, forgotten 5 s after they leave range |
| fast path | One `FindArea` in the building they were already in | Still inside it | |
| `Search` (`:344`) | `QueryEntitiesBySphere` (0.5 m, STATIC). The callback walks each hit up its parents to a `WorldSubsceneComponent` and calls `FindArea` | Every recheck for anyone outdoors, or on leaving a building | |
| Layout (`LC_RoomLayout`) | Finds which areas each portal joins by probing the building with 0.75 m `FindPortalByOBB` boxes | Only for the building the **local** player is in, 128 probes a write until all portals are found or the grid ends (cap 40,000) | One per prefab for the whole `LC_Client` lifetime (one server session; it survives respawns and resets on reconnect or reload). Never refreshed, never evicted |
| Paths (`FindPaths`, `:545`) | Cheapest muffle from the listener's area to every area of one building | Indoors: from the listener's area. **Outdoors: from area 0 of the speaker's building** | 2.0.6: 8 slots keyed (subscene, layout, source area), LRU. Portals re-read every 250 ms while the slot is in use; search again only if a cost changed |
| Traces (`LC_Occlusion`) | 1 to 2 `TraceMove` with a filter callback | Every pair unless the room model says clear (same room, or one open doorway within 15 m). A room answer above 0 is only an upper bound, so the pair is **still traced** | Per player. Budget 12 a write; due after 50 ms talking / 250 ms silent, if either end moved 0.15 m, else every 1 s |

The outdoor-client case:
- Both outdoors: no room model. Traces only, plus a sphere query for each moving player.
- Listener outdoors, speaker inside building B: B's graph is used from area 0, **but only if this client has
  mapped B's prefab**, which takes having stood inside one. Otherwise traces decide. An outdoor client never maps
  anything.
- Speakers in different buildings: one path slot each. Speakers in the same building share a slot.

People entering or using a building:
- **Someone entering**: one sphere query to find the building, then one `FindArea` per recheck. It does not
  re-map the building or re-search paths, because their area is looked up in the listener's existing distance
  table. A new player in range is traced on the next write with budget left.
- **Door or window change**: picked up at the next 250 ms re-read of a slot in use, followed by one search.
  A swinging door changes `GetPortalOpening` on several re-reads, so expect about 4 searches per swing per slot.
  A broken window flips to sound-passing (0.6 → 0.2). That costs nothing extra, and the trace is skipped for
  speakers within 15 m through that opening.
- **The listener moving between rooms**: a new source area means a new slot, which means a full portal read and a
  search. Walking an apartment corridor does this every few seconds.

## Cost, 2.0.5 (released) vs 2.0.6

Apartment, listener inside, other players in other rooms, 20 Hz:

| | 2.0.5 | 2.0.6 |
|---|---|---|
| Portal state reads | all 185 usable portals (≈2–4 native calls each) **every write** ≈ 10k calls/s | same read every 250 ms ≈ 2.5k calls/s |
| Path solve | Bellman-Ford every write: up to 117 passes × 185 portals × 2 `Relax` **script calls**, early exit. At ~15 passes ≈ 5.5k calls a write, ≈ 110k/s | only when a cost changed; queue search over per-area adjacency, inline |
| Outdoor listener, speakers in 2+ mapped buildings | one cache entry: re-solved **within the same write** each time the player loop switches building | one slot per building |
| Mapping a prefab with an unfound portal | keeps probing above the building until 40,000 cells (≈310 writes, 15–30 s) | stops when the grid ends |
| Room diagnostics on | collider volume of the blocker (often the whole building) on every trace | only for the line about to print |

2.0.6 removes most of the recurring room work: roughly a 50× cut in steady-state script calls inside a big
building. It also removes the "each mapped building adds solver work everywhere" growth, as long as at most 8
slots are live at once.

## Findings: what is still worth fixing on 2.0.6

Ranked by expected gain in real play. None is measured.

1. **Path slots can still thrash** (`MAX_PATHS = 8`, `LC_Rooms.c:301`). LRU evicts the oldest slot even if it was
   used this write. With 9+ live (building, source area) pairs, for example an outdoor listener in a town where
   a platoon is spread over 9+ mapped houses within 60 m, every lookup misses. Each miss means a full portal read
   and a search, every write, which is back to 2.0.5 behaviour. Fix: raise to 32 (memory per slot is a float per
   area and per portal), or never evict a slot whose `m_iUsedTick == now`.
2. **Sphere queries for moving players are not throttled.** `Locate` rechecks after 0.5 m **or** 500 ms. A
   runner (≈5 m/s) or anyone in a vehicle crosses 0.5 m every write, so every moving outdoor player within 60 m
   costs a `QueryEntitiesBySphere` every write. That is not covered by the trace budget. Near buildings, the
   callback repeats `FindComponent` up the parent chain and `FindArea` for **every child** of the same building
   (doors, windows), because an outdoor hit only ends that entity's walk, not the query. Fix: a minimum recheck
   interval (100–150 ms) whatever the movement, and skip a subscene already tested in this query. A convoy or a
   squad moving through a town is the worst case.
3. **Prefab-name lookup per speaker per write** (`GetLayout`, `:421`, called from `GetMuffle` whenever someone is
   indoors and the areas differ). `SCR_ResourceNameUtils.GetPrefabName` walks the prefab's container ancestors
   doing `GetResourceName().Contains("/")`, then does a string-keyed map lookup. Fix: keep the `LC_RoomLayout` on
   `LC_RoomLocation` when its building changes in `Search`.
4. **Trace filter does name and cast work before the cheap test** (`LC_Occlusion.FilterCover`, `:146`). Six casts
   plus `GetPrefabName` plus a map lookup run on every entity a trace meets, before the width test. Width < 0.8 m
   rejects either way, so testing width first gives the same result and skips that work for posts, props and
   narrow frames.
5. **Portal costs are read per slot, not per building.** Slots for different source areas of one building each
   read all portals. It matters when the listener walks through a big building (a new slot per room). Fix: one
   cost array per subscene with a revision number, shared by its slots. Lower priority than 1–4.
6. **Traces still run for most indoor pairs.** The room answer is only an upper bound unless it is 0, so an
   apartment with several people in different rooms still spends the 12-trace budget whenever they shuffle (two
   traces each, from the eyes). This is by design: a path cost can't see line of sight through a window. It is
   likely the largest remaining indoor cost after 2.0.6. No change suggested without a measurement.
7. **Risk, unverified:** the layout is per prefab name. If a destroyed or variant building keeps its prefab name
   but has fewer portals, the stored portal indexes are passed to `IsPortalEnabled` and similar without a
   `< GetPortalCount()` check. Areas are guarded, portals are not.

Not a problem:
- **Memory.** Layouts are a few KB per house type, tens of KB for the apartment, and bounded by the prefabs you
  enter in one session. Path slots are tiny. Muffle samples are dropped after 5 s. The see-through cache is
  bounded by prefab count.
- **Mapping** runs once per prefab per session, only where you stand, and is now finite. Its cost (128
  `FindPortalByOBB` a write, each likely testing every portal of the subscene natively) is a transient of about
  15 s per new building type.

## Compared with the 2.0.6 investigation (read after the above)

Same findings: the per-write Bellman-Ford with its "handful of areas" assumption, the scanner never finishing on
grid exhaustion, the per-trace collider volume under diagnostics, and the outdoor single-entry cache thrash. All
four are fixed on `bugfix-2.0.6` and confirmed in the diff above. Its hypothesis that "mapped layouts add
area-wide solver work over a session" matches the 2.0.5 code. 2.0.6 turns that cost into a 250 ms portal re-read
per live slot.

New here, not in that investigation:
- the 8-slot cache can still thrash (1)
- unthrottled sphere queries for movers, and repeated per-child callbacks (2)
- the per-write prefab-name lookup (3)
- the trace filter order (4)
- per-slot portal reads (5)
- the unguarded portal index (7)

Its tests still apply, and they are the oracle for all of this: the 20–25 min apartment vs shelter run with
frame times (diagnostics off), then with rooms bypassed. Add a town run with a moving squad outdoors for (1) and
(2).

## Follow-up, measured (2026-11-04)

Fixes 1-5 and 7 landed in f9480cc. Timed in Workbench afterwards (`2026-11-04-whole-mod-performance.md`): a
sphere query costs ~2 µs and a trace pair through a building ~13 µs, so fixes 2 and 4 save little. The real cost
is mapping: 3.6 ms a write for 313 writes in the apartment, which also stops at the 40,000-cell cap with 87
portals unfound. Finding 6 ("traces are likely the largest remaining indoor cost") is wrong.
