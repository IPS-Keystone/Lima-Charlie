# Changelog: occlusion performance (branch `perf/occlusion-traces`)

Work from 8–10 Nov 2026, on top of 2.0.6 (main 18d6afe). Not merged, not pushed, not released.
Status: the trace changes compile in Workbench; the room-lookup change has passed the static check only.
Nothing has been run in game.

## Why
Frame drops with many players nearby, indoors and outdoors. Confirmed as Lima Charlie by Duke (same building,
similar player count, no lag without LC). Full evidence: `investigations/2026-11-08-indoor-crowd-frame-drops.md`.

## Performance
- **Only talking players are traced.** A player is traced the moment they start talking, then every 50 ms while
  either body moves, otherwise once a second. Before, silent players were traced every 250 ms too.
- **Silent players are only kept roughly current:** one per write, only while nobody at all is talking, every
  2 s if moving or 10 s if still.
- **Turning on the spot no longer causes retraces.** Movement is measured from the body, with a 0.5 m threshold
  (was the eye, 0.15 m). Traces still run eye to eye.
- **At most 3 traces per write** (was 12 pairs), and none after 2 ms of tracing in that write. Each write starts
  from a different player so nobody is starved.
- **Both outdoors and more than 30 m apart: clear, no trace.** Only when LC's room model and the engine's own
  interior estimate agree both are outside and neither is in a vehicle. Anything unsure is traced.
- **Room lookups are budgeted:** at most 3 building searches per write; the cheap same-building check is not
  counted. Silent players are looked up every 1 s moving / 3 s still (was 150–500 ms), and at once when they
  start talking.

## Unchanged on purpose
- One indoors and one outdoors is always traced, so someone in a doorway you can see stays clear.
- Same room is clear without a trace; different rooms use the room model plus a trace for talkers.
- Vehicles are still handled by the trace hitting the hull. Vanilla's seat-coverage method was looked at and
  not adopted: it only considers the listener's own seat.
- The second (reverse) trace stays; it now only runs for blocked pairs of talkers.

## Considered and dropped
- A physics-layer filter to keep trees out of the trace callback: vanilla gives trees and buildings the same
  layer mask, so no filter separates them.

## Diagnostics
- Summary line: new `far outdoors N` count.
- Room line: `[far outdoors]` for untraced outdoor pairs, and `e0.00` (the engine's interior value) where one
  was asked for.

## Docs
- `how-it-works.md` and `developer-reference.md` describe the new schedule.
- `investigations/2026-11-08-indoor-crowd-frame-drops.md`: logs, live test, recorded session, vanilla VON's
  occlusion model, the rework design and what was implemented.
- `WORKLOG.md` updated.
- Workspace wiki (`Wiki/game/audio-mixing.md`, committed to workspace main): vanilla VON occlusion uses no
  traces; `SoundWorld.CalculateInterirorAt`.

## Known issues found, not fixed
- Camo nets block traces although you can see through them.
- LC's room model does not detect Ruha's / the OTAC FOB's buildings (cause unknown).
- The ops server has `roomDiagnosticLog` on, which is the most expensive diagnostic.
- Whether worn gear costs trace callbacks is unknown; it never blocked a trace in ~2,500 logged cases.

## Still to do
- Recompile in Workbench and triage (room-lookup change).
- Ear test: doorway, through a wall, both outdoors near and beyond 30 m, first words through a wall.
- Recorded session at the FOB to compare hitch rate with 8 Nov.
- Measure the cost of `CalculateInterirorAt`.

## Commits
- db35359 Occlusion: trace talkers only, body move test, 3 traces a write, both-outdoors >30 m untraced
- caa11ca Worklog: occlusion branch compiles in Workbench
- e5aa652 Rooms: budget building searches per write, slow lookups for silent players
