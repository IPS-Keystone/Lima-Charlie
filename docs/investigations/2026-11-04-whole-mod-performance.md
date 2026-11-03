# Whole-mod performance pass, measured (2026-11-04)

Scope: every game script in `addon/Scripts/Game/LC` and the TeamSpeak plugin in `plugin/src`, on
`bugfix-2.0.6` after f9480cc (the room fixes from `2026-11-03-room-occlusion-performance.md`).

Method:
1. Read every file and listed what runs per frame, per write (10–20 Hz) and per event.
2. Timed the hot paths inside Workbench 1.8.0 (SteamDiag) using benchmark NET API handlers in the dev-only
   ClaudeDevTools addon. Sources and raw output are in `bench/`.
   - Script-only paths ran with no world loaded.
   - Engine calls ran in play mode on testWorld: a real Everon house (`House_Mountain_E_1I01`, 10 areas, 26
     portals) and a spawned `ApartmentBuilding_USSR_5I01` (117 areas, 310 portals: the building from the unit
     logs).
   - Timing uses `System.GetTickCount` over thousands of iterations. Treat numbers as ±10%.
3. Compared with the 2.0.6 session-log investigation at the end.

The first three blocks of `bench/2026-11-04-world-bench-raw.txt` are invalid: integer coordinates bound as 0, so
the buildings were spawned on top of each other at the map corner. Only their per-call native costs
(`FindPortalByOBB`, portal reads, mapping batch) are usable, and they match the valid run.

## Measured costs

| What | Cost | How often | Per second (20 Hz writes) |
|---|---|---|---|
| **Room mapping, apartment** (`LC_RoomLayout.Build`, 128 probes) | **3.6 ms** a write | 313 writes, on first entry to the type each session | 73 ms/s for 15–30 s |
| Room mapping, village house | 0.45 ms a write | 85 writes | 9 ms/s for 4–8 s |
| `FindPortalByOBB`, one 0.75 m probe | 28 µs apartment, 3 µs house (~0.09 µs per portal of the building) | in mapping | |
| **game_state.json write** (open, truncate, write 2.3 KB, close) | **~0.4 ms** (Python proxy on the same disk, not the engine) | every write | ~8 ms/s |
| Writer JSON, 30 players nearby | 190 µs | every write | 3.8 ms/s |
| Writer JSON, 60 players nearby | 485 µs | every write | 9.7 ms/s |
| plugin_state.json poll (`FileExists` + `LoadFromFile` + seq) | 108 µs, of which 6 µs is the parse | every 50 ms, changed or not | 2.2 ms/s |
| Trace pair through a building (`CountObstacles`) | 13 µs | ≤ 12 a write | ≤ 3 ms/s |
| Trace pair through open air or a street | 0.5–3.5 µs | | |
| `LC_Rooms.Locate` outdoors (sphere query) / indoors (fast path) | 2 µs / 0.4 µs | per nearby player per recheck | < 1 ms/s |
| `GetMuffle`, paths cached | 1.45 µs | per nearby player per write | < 1 ms/s |
| Portal cost re-read, apartment (185 usable portals) | 55 µs | 4 a second per building in use | 0.2 ms/s |
| Path solve on an apartment-sized graph (synthetic, 117 areas, 185 portals): 2.0.5 Bellman-Ford | 780 µs | 2.0.5: every write, per source area | 2.0.5: ≥ 16 ms/s |
| Same graph, 2.0.6 queue search | 264 µs | 2.0.6: only when a door or window changes | |
| Radio id string + `map<string>` lookup (without the `FindComponent` in `GetId`) | 0.6 µs | 4+ per radio per write, per frame while a transmit key is held | |
| Script call overhead | ~0.045 µs a call | | |

Steady state, in a building that is already mapped, with 30 players nearby and someone talking: about
0.8–1 ms of game-thread work per write. That is roughly 15–20 ms a second, 1–2% of a 60 fps frame budget,
and about half of it is the file write.

The spike is mapping. The first time a client walks into an apartment block in a session, every write costs an
extra 3.6 ms for 15–30 s. At 60 fps that is a 3.6 ms hitch every third frame. It repeats for every new building
type and resets at every reconnect or scenario reload.

## Findings, ranked

1. **Mapping a big building is the largest cost LC has.**
   - It is 3.6 ms a write for 313 writes in the apartment, because each probe tests every one of the
     building's portals (28 µs × 128).
   - It also never finishes there. The apartment's grid is 73 × 36 × 27 ≈ 71,000 cells, so the 40,000 cap
     stops it a little over halfway along the building's depth. 87 of 310 portals are never found and fall back
     to traces. That matches the unit logs.

   Options, for Duke:
   - (a) Size the batch by portal count so a write costs a fixed amount, e.g. `cells = clamp(12000 / portals,
     8, 128)` ≈ 1 ms. The apartment then takes ~1,000 writes (1–2 min) instead of 313, but with no hitch.
   - (b) Save finished layouts to `$profile:LimaCharlie/rooms/` keyed by prefab plus area and portal counts
     (and an LC layout version), and load them on first use. Each type is then mapped once per client, ever,
     not once per session.
   - (c) Raise or remove the cap so big buildings finish (only sensible together with (a) or (b)).

   I recommend (a) plus (b). Not changed today: it trades mapping time against hitching, and that's Duke's call.

2. **The bridge file write is the biggest steady-state cost: ~0.4 ms a write, synchronous** (proxy, not
   measured in engine).
   - It is part of the file-bridge design. RestApi was rejected for latency, so there's no cheap code fix.
   - Possible trims: skip a write when everything but `seq` is unchanged (rare in play), or tell players to
     exclude `Documents\My Games\ArmaReforger\profile\LimaCharlie` from antivirus real-time scanning, if an
     in-engine measurement shows scanning is most of the cost.
   - Next step: time `FileIO.OpenFile` / `WriteLine` / `Close` in engine to confirm.

3. **Writer JSON appends to the growing document 5 times per player.**
   - 190 µs (30 players) and 485 µs (60 players) a write.
   - Building each player's object first and appending once measured 165 µs and 340 µs (−13% / −30%).
     `SCR_StringHelper.Join` is no better.
   - Safe and small, in `LC_GameStateWriter.BuildInGameJson`.

4. **The plugin-state poll loads and parses the file every 50 ms even when the plugin hasn't rewritten it.**
   - 108 µs each, almost all file IO. Enforce has no mtime call, so a cheaper "has it changed" check isn't
     available.
   - Leave as is unless the in-engine file measurement says otherwise.

5. **Radio ids are rebuilt repeatedly** (not timed with real radios).
   - `LC_Radio.GetId` does `FindComponent(RplComponent)` plus string building. It runs 4 times per radio per
     write (id, ear, volume, beep set), plus `GetRadioMode` does another `FindComponent`.
   - It also runs per radio every frame while an LC transmit key is held (`GetKeyEntry`, `GetAssignedKey`), and
     per radio per heard transmission in `LC_VonDisplayFeed.FindTransceiver`.
   - A Game Master with 12 entries pays it ~60 times a write.
   - Fix: cache the id (and duplex mode) on a modded `SCR_VONEntryRadio`. Both are fixed for the entry's life.
     Then `GetId` is a field read everywhere.

6. **Per-frame client tick is small.**
   - `RefreshRadioEntries` walks `m_aEntries` twice with casts, and `GetVONController` does a `FindComponent`
     two or three times a frame. A few µs per frame.
   - Caching the controller is trivial. Caching the entry list needs vanilla `AddEntry` / `RemoveEntry` hooks,
     which isn't worth the risk for the gain.

7. **Rooms and traces in steady state are now cheap.**
   - Locate 0.4–2 µs, cached `GetMuffle` 1.45 µs, portal re-read 55 µs per building per 250 ms, traces ≤ 0.16 ms
     a write.
   - That revises yesterday's guesses: the sphere query (fix 2) and the trace filter order (fix 4) were real
     but small savings. Traces are not the dominant indoor cost; mapping is.
   - 2.0.5's per-write Bellman-Ford (≥ 0.8 ms per solve in a big building, several per write when outdoors
     between buildings) was a genuine cost, and 2.0.6 removes it.

8. **TeamSpeak plugin: nothing urgent.** It runs on TeamSpeak's threads, not the game's.
   - The worker loop runs every 5 ms in game: two file stats, four TeamSpeak lookups, voice targets for every
     peer, and a plugin-state body built and compared.
   - The audio path per voice per sample is a few biquads plus one `tanh` on radio.
   - Minor:
     - `publish_voice` recomputes pans with `pow`/`cos`/`sin` every 5 ms even when nothing moved. Needed for the
       muffle glide, so leave it.
     - `lc_peers_list` copies two version strings per peer per loop that `publish_voice` never reads.
     - `timeBeginPeriod(1)` while in game is a deliberate power trade-off.
   - Correctness, not speed: the audio snapshot is double-buffered without a sequence check. If the worker
     published twice inside one audio callback, the audio thread could read a half-written snapshot. Unlikely
     at 5 ms vs microseconds of callback work. Noted only.

9. **`LC_Hearing.GetCombined`** allocates a profile and an array every write while a hearing source is active.
   Trivial.

## Compared with the 2.0.6 investigation (read after the above)

- It flagged mapping as "128 probes per write for ~15 s" but did not know the per-probe cost. The measurement
  makes it the top item: 3.6 ms a write in the apartment, plus the cap stopping before the building is covered.
- It listed the per-tick radio costs (`RefreshRadioEntries`, `GetId`, `FindTransceiver`) as unmeasured. The
  string side is now measured (0.6 µs); the `FindComponent` side still isn't. This report suggests a concrete
  fix (cache on the entry).
- Its trace concern ("`FilterCover` is a script callback per entity") is now measured: 13 µs for a pair through
  a building, so traces are not a stutter source on their own.
- It did not cover the file write, the plugin-state poll, or the writer JSON. These are measured here.

## Not measured

- In-engine file write time.
- `FindComponent` cost in `GetId`.
- Frame times in a real multiplayer session.

The 20–25 min apartment vs shelter run with frame times from the 2.0.6 plan is still the test that settles
how much of this a player feels.
