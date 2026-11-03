# 2026-11-03 unit session: logs vs reported bugs

Mod **2.0.5** on every client (protocol 9); plugin 2.0.5, except Linstone on **2.0.1** (missed in my first pass).
Sections below "Findings by report" were written before reading the external handover
(`Downloads\LimaCharlie_Handoff_2026-11-03\`, FPS-focused). "Comparison with the external handover" was written after.
Clocks: Tim and Linstone logs are UTC+11; Parko's are **UTC+10:30**, so add 30 min to compare them. My Parko
times below are local.

## Logs
| Log | Player | Span | LC diagnostics |
|---|---|---|---|
| `logs_2026-11-03_18-06-12` | Parko (id 2) | 18:06–20:19 | off (61 LC lines) |
| `logs_2026-11-03_20-19-43` | Parko, after restart | 20:19– | game_state + rooms, from 20:20 |
| `TIM_logs_2026-11-03_18-48-48` | Timberly (id 1), GM | 18:48–21:25 | from ~20:49 |
| `Linstone` `logs_2026-11-03_19-21-11` | Linstone (id 6), GM at times | 19:21–21:20 **crash** | from ~20:50 |

Server settings: clean radio 35%, beep 90%, terrain 50%, GM unlimited range on.
Diagnostics were only on for roughly the last hour. The brief and any terrain-masked traffic may fall outside that.
Analysis scripts are in the session scratchpad (`radios.py`, `peer.py`, `brief.py`) and are not kept.

## Findings by report

### GM radio channels scrambled: **cause found (vanilla trait + our positional default)**
- Transmit keys default to the radio entry at the key's position in vanilla `m_aEntries`
  (`LC_RadioSettings.GetKeyEntry`, `LC_VONController.LC_GetRadioEntries`). The radial menu follows the same order.
- Vanilla `SCR_VONController.RemoveEntry` uses `array.RemoveItem`, which moves the **last** element into the hole.
- Evidence, Tim's `game_state` at 20:49:39 → 20:49:47: `[GM:1-4, 2823:1, 2823:2, 2769:1, 2769:2]` became
  `[2769:2, 2769:1, 2823:2, 2823:1]` when the editor closed. That is exactly four swap-removals from the front.
  TX1–TX4 changed channels, and channel 1↔2 of each radio swapped.
- While the editor is open with its entries first, TX1–4 default to the **GM editor transceivers**.
- Same trigger for anyone: dropping a radio, or any radio entry leaving the list. Picking one up appends and
  moves nothing. (Corrected: vanilla 2.0.x adds no vehicle radios to the list, so leaving a vehicle is not a
  trigger. The 4-channel radio in the logs is the GM editor's own.)
- Fix options: order our list ourselves (radio first-seen order, then transceiver number) and/or a modded
  `RemoveEntry` that uses `RemoveOrdered` so the vanilla radial stays put too. Explicit assignments (by id) are
  unaffected.

### Radio stutter / "dropping radios helps": **one confirmed contributor (exceptions) plus code costs**
- 748 VM exceptions `NULL pointer to instance. Variable 'm_wRole'` across the four logs, all with the stack
  `LC_Client.Tick → LC_VonDisplayFeed.ShowReceiving:89 → SCR_VonDisplay.OnReceive:226 → LC_VonDisplay:17 →
  OT_VonDisplay.c:29` (OTAC Core). Bursts at 19:38, 20:26–27 and 20:52–54 on several clients at once, peaking
  ~271/min (Linstone 20:26).
- Mechanism: with every incoming VON slot in use, vanilla puts the next speaker on the "+N" widget, which has no
  `m_wRole`. OTAC's override dereferences it, and the throw skips `m_bIsActive = true`. So our 250 ms keepalive or
  20 Hz plugin seq re-raises it until a slot frees. Each throw writes a stack trace to console, error and crash logs.
  The throw also aborts the rest of `Tick` (`m_StatusNotice.Update`).
- No radio means no receive and no feed, which fits "dropping radios helps". Bug is OTAC's (unguarded widget). Our
  mitigation: don't re-raise `OnReceive` for a player sitting in the additional-speakers list (needs a small
  modded `SCR_VonDisplay` helper), or feed only on change.
- Per-tick radio cost that scales with radios carried: `RefreshRadioEntries` every frame; each write builds
  radios JSON with `LC_Radio.GetId` (FindComponent + string concat) several times per entry; `FindTransceiver` does
  the same per heard transmission. A GM carries 8 player entries plus 4 editor entries. This is not measured.

### Indoor raycast / room stutter (apartments, shuffling, window break): **code hot spots, not measured**
- No frame-time data in any log, so this needs a profiler or A/B run to confirm.
- Rooms: every write (20 Hz while anyone talks), indoors, `LC_Rooms.Update` bumps the generation and
  `EnsureDistances` re-reads every usable portal and runs Bellman-Ford over all areas. The comment assumes "a
  handful of areas". `ApartmentBuilding_USSR_5I01` logs **117 areas, 310 portals, 185 usable when done** (109 was a
  mid-mapping sample), so up to ~117 × 185 × 2 = 43k `Relax` calls per solve in script.
- First entry to a building type: mapping spends 128 `FindPortalByOBB` probes per write (2,560/s) for ~15 s
  (`mapping` lines, up to 6,528 cells in the apartment).
- Traces: 2 `TraceMove` per due speaker, budget 12 per write (≤ 480 traces/s). A speaker is due after 50 ms
  (talking) or 250 ms when either end moves 0.15 m, which confirms that shuffling drives traces.
  `FilterCover` is a script callback per entity met (casts, `GetPrefabName` string work, bounds). In practice the
  callbacks are few: no trace in the logs skipped more than 6 entities (skip1 ×2622, skip2 ×634, skip3 ×190,
  skip4 ×143, skip6 ×8).
- Window-break spike, followed up below: the glass-debris idea is **refuted**.

#### Window break (follow-up)
- **Nothing in LC runs on a break.** There is no damage hook. The only change LC sees is vanilla
  `SCR_WindowHitZone` flipping that window's portal to sound-passing. The room model already re-reads every portal
  on every write, so that costs nothing extra.
- **No debris on the apartment's windows.** Its panes (`Win_Soviet_A_*` → `Glass_Soviet_A_*` →
  `DestructibleGlass_Base.et`) break into a damaged model swap (mesh and physics) plus a shatter particle. Only the
  glass-block windows spawn debris, and `SCR_BaseDebrisSmallEntity` is in `IsNeverCover` (one cheap skip each).
- **Panes never reach the traces.** Across all logs the trace's cover is the building shell (9,923), a window
  frame (`Win_Soviet_A_195x142_3_B` 264), a door or furniture, and never a `Glass_*`. Panes are under the 0.8 m
  width cutoff. The frame survives the break, so a trace through a broken window still reads muffled.
- **After the break LC does less, not more.** The window portal goes from blocking (0.6) to passing (0.2). A
  speaker within 15 m through that one opening resolves as clear from the room model and **skips the trace**.
  That is one LC reason for "frames get better once the occluder is gone", though it only covers speakers
  behind that window.
- **One logged break, no visible hitch.** Apartment portal 7 went `B` → `S` at 21:02:10.85 (Tim) and 21:02:11.80
  (Linstone), with both inside. The 1 Hz diagnostic lines stayed on time (Tim gaps 1.075/0.946 s, Linstone
  1.070/1.001 s; session median 1.02 s, only 2 gaps over 1.2 s in ~3,600). Linstone's writes dipped to 14/s and
  then 16/s (usually 18–19), so a hitch of up to ~0.2 s there is possible. A freeze that ends before the next due
  line is invisible to this check.
- So the break-time cost is the engine's destruction work (model and physics swap, particle, sound). If it
  feels "major", it is most likely landing on frames already loaded by LC's per-write room solve and traces. The
  fix target stays the per-write work above, not anything break-specific.
- Test to settle it: in an apartment, break windows with (a) TeamSpeak running and (b) TeamSpeak closed. With no
  plugin, the LC writer drops to one write per 500 ms (`NO_PLUGIN_INTERVAL_MS`), about a tenth of the room and trace
  work. If the spike is the same in both runs, it is vanilla. A vanilla-only run (no LC) is the stricter control.
- "Character location, not head": **refuted**. Speakers are `EyePosition()` and the listener is the head
  (or the camera when it is >20 m from the body).

### Tim's voice rising and falling during the brief: **two candidates, the brief itself not in the diagnostic window**
- Positions are character eyes, so a GM camera does not move Tim's voice.
- Occlusion flicker is real in the logs. At 2–8 m inside buildings Tim's muffle toggles 0 ↔ 0.6 ↔ 0.9 second to
  second (e.g. Parko 20:22:08–20:22:47, Linstone 21:06:26–21:06:37). In the plugin that is −5.4/−8.1 dB plus a
  low-pass jumping 18 kHz → ~1.8/1 kHz, with no smoothing beyond one buffer's ramp. People shuffling in a room
  would make a talker pump.
- In the two windows where Tim talked ≥30 s at a fixed 15 m outdoors (20:43:59, 21:14:03) every LC input was
  constant. Any pumping there is outside LC: TeamSpeak capture processing or a Windows/driver mic "enhancement" AGC
  on Tim's side. Ask whether it happened outdoors too.
- The radio limiter is stateless `tanh` (no attack/release), so it cannot pump.

### No reverb in larger buildings: **by design in the plugin curve**
- `lc_reverb_room_params`: wet peaks at 600 m³ (0.10) and fades to 0 at 6,000 m³. Seen volumes: hangar
  `AircraftShelter_E_01` 3,894 m³ → wet ≈ 0.019; `Building_Base` 3,145 m³ → ≈ 0.028; apartment rooms 26–123 m³ →
  0.04 (small-room floor, decay 0.62). Large spaces are deliberately near-dry. A big building made of small rooms
  only ever gets small-room reverb. Buildings with no room model read as outdoors (no reverb at all).
- A tuning call for Duke (curve and wet levels), not a defect.

### Terrain: FPS cost, and the masked player hears clearly but is heard garbled: **not in these logs**
- Every link in all logs has `clearance 0, hills []`, and every `radioHeard` quality is 1.00, so no masked
  traffic was captured.
- Code: terrain is ≤ 127 `GetSurfaceY` per link, at most once a second or per 10 m move. It's cheap, and an FPS hit
  from it alone is unlikely. Hills changing ≥ 1 m bumps the link revision, which forces a full write, including the
  room and trace work above. That's the more plausible cost.
- Asymmetry candidates: each receiver judges against the **transmitter's** radio range (different sets give
  different answers); and if the receiver's game never answers the link, the plugin assumes line of sight after
  `LC_LINK_WAIT_MS` (clear audio). Needs a two-player test across a ridge with diagnostics on.

### Performance worse over time: **no LC accumulation found**
- `LC_Client` start/stop balanced in all logs (no duplicate Tick). Layout cache bounded per building type; muffle
  cache forgets after 5 s; link map drops stale entries.
- The diagnostic logging used here adds ~1 game_state JSON per second to the log, which is minor.
- Other mods in the same logs: OTAC `OTAC_AdaptiveClothVisualComponent` 304 exceptions (`m_World`, `m_Callqueue`),
  plus `m_iOrder`/`m_MainEntity`. Not ours.

### Crash (Linstone 21:20:22)
- Native `Access violation. Illegal write ... at 0xfffffffefffffffc` right after a large `MATERIAL (E)` dump. No
  script frame. Not attributable from the log; the `.mdmp` is in the handover.

## Comparison with the external handover (read after the above)
Their scope is FPS only (no scramble, voice pumping, reverb or terrain). It also adds Duke's in-session
observations I did not have: leaving the building does not restore FPS but leaving the built-up area for the
runway does; a fresh server start at the apartment starts better and degrades; the airfield bunker was fine.

**Agree, and checked against source:**
- OTAC overflow: vanilla returns `true` early for `m_bIsAdditional` (`SCR_VonDisplay.c:277`). OTAC
  (`OT_VonDisplay.c:29`) then dereferences `m_wRole`, and its channel styling has the same flaw. Same 748 total
  (205/52/68/423). Their counts are synchronised in Sydney time across clients.
  - Addition from me: the throw skips `m_bIsActive = true`, so our 20 Hz feed re-throws for that speaker until a
    slot frees.
  - Their "LC cannot prevent it" holds only for a guard in our `UpdateTransmission`. Not re-raising `OnReceive` for
    an overflowed speaker does prevent it.
- Rooms solver invalidated every write and a scale mismatch with the code's assumption (same as mine).
- Shuffling drives traces; endpoints are eyes, not feet (same as mine).
- Radio links are terrain samples, not building rays (same).

**New to me, verified:**
- **Room scanner never finishes on grid exhaustion** (`LC_Rooms.Build`): `Advance()` false → `break`, and `Finish()`
  only on all-found or cap. Next write resumes above the grid, probing layers outside the building until 40,000.
  Every layout but the hospital (all portals found) ends at 40,064. This costs extra mapping time per building type,
  once. It's a real bug but bounded.
- **Room diagnostics run native geometry per trace:** with `roomDiagnosticLog` on, every refreshed trace runs
  `DescribeCover → DescribeSolidity → MeshObjectVolumeCalculator.GetVolumeFromColliders` on the blocker. That's
  often the whole 54 m apartment, up to 12 × 20/s. This runs before the 1/s log throttle. The whole post-reload
  phase (where the window and drop observations were made) ran with this on. It isn't the first phase's cause
  (flags were off there).
- **Outdoor listeners use an indoor speaker's mapped graph**, and the distance cache holds one
  (subscene, source area). Several speakers in different mapped buildings re-solve within one write.
- **LC state resets at the 20:47 session reload** (Stop/Start inside Tim's one process).
- **Bunker vs apartment:** same 40,064 probes. The shelter's graph is empty (0 usable) so it never reaches the
  solver, and ~64% of its pairs are same-room shortcuts, vs ~16% in the apartment. This weakens "probe count" or
  "building size" as the explanation and points at the recurring path/trace work.
- **Portal 7 `0B → 0S` at 21:02 Sydney on all three clients**, matching `SCR_WindowHitZone` (destroyed → passing).

**Their open item, done here: diagnostic-flag audit.** With both flags off, the optional work is fully skipped:
`LogDiagnostic` (writer and reader), `LogRoomDiagnostic`, `AppendRoomDiagnostic` and `DescribeCover`/collider
volume are all behind the flag. What remains is a string reset and two int counters, reset each write. The JSON
is built anyway because the plugin needs it. Status `Print`s fire only on change. Nothing accumulates. So the
first-phase slowdown is not hidden logging.

**Where I revise my view:**
- "No LC accumulation" was too narrow. Memory doesn't grow, but **capability does**. Each building type mapped
  adds graph solving for every pair touching that building, indoors or out (from ~16 s after first entry), with the
  single-entry cache thrashing between buildings. That gives an area-wide, worsening-over-session cost, which fits
  "runway fixes it, leaving the building doesn't". It also resets at a session reload, which fits "fresh start is
  better". Still a hypothesis.
- "Bursts on several clients at once": right for Tim/Linstone, and true for Parko too once +30 min is applied.

**Where I'd push back on theirs:**
- They treat "dropping radios helps" as unexplained. The exception storm is a concrete radio-only mechanism, since
  radio receive is the only trigger. Plus per-radio per-tick string/FindComponent work (a GM carries 12 entries).
- Their recommendations are all measurement. Agreed for causality, but three fixes are cheap and safe to ship
  first: the scanner `Finish()`, collider volume only when a diagnostic line is due, and not re-feeding overflowed
  speakers.

## Next
1. Fix candidates for a 2.0.6:
   - stable radio order (scramble);
   - no `OnReceive` re-raise for overflowed speakers;
   - scanner finishes on grid exhaustion, with the cap checked inside the loop;
   - collider volume only on a due diagnostic line;
   - room distances cached per (subscene, source area) and invalidated on portal state change, not every write,
     with an adjacency search instead of full-edge passes;
   - smooth muffle steps in the plugin.
2. Report the OTAC Core `OT_VonDisplay` guard (`m_bIsAdditional` / null widgets) to OTAC.
3. Tests:
   - a GM editor open/close with 2 radios (scramble repro);
   - 6+ simultaneous radio talkers with OTAC;
   - a 20–25 min apartment vs shelter run, diagnostics off, with frame times, then the same with rooms bypassed;
   - a ridge test for terrain asymmetry.
4. Bring Linstone's plugin to 2.0.5.
