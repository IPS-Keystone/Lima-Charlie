# Work log — Lima Charlie

The running journal (`_Rules/PROJECT-DEFAULTS.md` → Working loop). Read the top at the start of a
session; update after each meaningful step and before ending one. Never rewrite or delete old entries.

## Done
- 3.0 workstream B (2026-11-02, feature/3.0-comms): vehicle radios, key fill/mute, intercom, infantry phone (BTR-70, LAV-25). All compiled in Workbench; **none tested in game** (test list: 2026-11-02 part 4 entry).
- 3.0 groundwork step 2 (2026-11-02, feature/3.0-comms): server-arbitrated radio leases (`LC_RadioLeases` on the game mode, replicated triples, distance + seat rules, released on death/disconnect/radio gone/rule fail/explicit). Compiled (Workbench 5700 files, no LC errors); not yet exercised in play mode.
- 3.0 groundwork step 3 (2026-10-31, feature/3.0-comms): receive quality behind `lc_radio_receive` (`lc_rx_inputs` → `lc_rx_result`, lc_radio.h); placeholders for jam/relay/SATCOM are comments only. Behaviour unchanged; pinned by `test_radio_receive`.
- 2.0.5 released 2026-10-29 (tag v2.0.5): hearing hook for ear-pro mods, louder radios + limiter, terrain as shares per hill. Protocol still 9.
- 2.0.4 released (mod + plugin), protocol 9 (was mis-noted as 6); radio keys open the TeamSpeak mic, transmit-key picker (CHANGELOG.md).
- Direct speech with voice levels, occlusion (room model + traces), room reverb, radios with garble and
  terrain, AI hearing, Game Master support (docs/how-it-works.md).
- Verified in multiplayer earlier: attenuation, muffling, range switching (2-player test, 2026-09-14).

## To do
- 3.0 comms expansion (feature branch `feature/3.0-comms`): manpacks/speakers/handsets, vehicle
  radios/infantry phone/intercom, EW/jamming/SATCOM. Plan + open questions: docs/design/3.0-comms-expansion.md.
  Order: shared foundation + vehicles first, then manpacks, then EW/relays/SATCOM. Testing local only.
  `main` 2.0.5 merged in 2026-10-31. Next: survey (vehicle radios, VON entry API, compartments), then the
  foundation (radio source/position, leases, one quality function).
  Handover with current state, collisions (`radio-tuning`) and next steps: docs/design/3.0-handover.md.
- Bugfixes keep shipping from `main` as 2.x.y; merge `main` into the feature branch after each release. `bugfix-2.0.6` (c56799c) merged in 2026-11-04, then `main` (2.0.6 release, 18d6afe) the same day.
- Not yet tested: retail game client with BattlEye.
- Decide whether `WorkshopCoverPicture.*` and the new `PROJECT.md` / `docs/research/` / this file get committed.

## Touch-ups
- 3.0 leases: leased radios reach `radios[]` and the transmit keys but not the VON radial menu (the consumer adds a menu entry, e.g. vehicle reconciler); radios without an `SCR_RadioComponent` gadget (vanilla's unused vehicle radios) are leased but never listed.
- 3.0: switching to another radio on the same frequency restarts the 300 ms terrain wait (links are per radio since `d00d805`); briefly clear until the answer lands.
- 3.0: receiving side still measures from `self.pos`, not its own radio's antenna; needed for vehicle radios (B).
- Terrain answers can be missed: plugin assumes line of sight after 300 ms without reporting it (docs/troubleshooting.md, Known fragilities).
- Wishlist not started: simplex/duplex, vehicle intercom component, babble languages, relays/satcom, EMP.

---

## 2026-10-29
- Wrote docs/design/3.0-handover.md. Found Duke's `radio-tuning` branch (2.0.5, unmerged) rewrites the
  terrain/quality code the groundwork touches and adds the same survey file: wait for it to reach `main`
  and merge `main` here before the plugin-side groundwork.

## 2026-10-28
- Duke's second round recorded (design doc, "Second round"): branch renamed to `feature/3.0-comms`, test
  builds 3.0.N; any seat any intercom channel; jammers player-switchable/destroyable and jam transmissions
  from inside their radius with noise; SATCOM no delay, forced half-duplex, duplicates your radios, Link
  Radio action (non-exclusive, auto-unlink), jammer-immune, unlimited TX / normal RX; relays player + GM
  placeable on the vanilla relay prop; direction finding is a stretch goal for another branch.
- Duke's answers recorded in docs/design/3.0-comms-expansion.md (Decisions): local testing; foundation +
  vehicles first; owner keeps transmitting while a handset is borrowed; intercom on direct speech with a
  No Intercom option, as a 4-option cycling entry in the VON menu; jammers as GM props; SATCOM a placeable
  inventory item. Designed relays and SATCOM as one relay path; explained direction finding. Added Duke's
  survey docs/research/2026-10-28-terrain-autoconnect-vehicle-comms.md to the branch.
- Created the 3.0 feature branch `claude/gallant-knuth-zna9uk` from `main` 71b7e2b (cloud session) and wrote
  docs/design/3.0-comms-expansion.md: scope, branch strategy for parallel bugfix releases, shared foundation
  (radio source/position, server leases for borrowed/vehicle radios, one quality function), per-workstream
  plan, provisional bridge changes, open questions. No code yet.
- Note: GitHub `main` lacks the local survey `docs/research/2026-10-28-terrain-autoconnect-vehicle-comms.md`
  (referenced from the workspace wiki). (Later the same day: Duke attached it; now on this branch.)

## 2026-10-19
- Migrated into `AReforgerWorkspace/Projects/Lima Charlie Non-Enfusion` (repo root, `addon` junction);
  `git status` clean against HEAD ee92f7b. Research notes moved to `docs/research/`. Worklog seeded from
  PROJECT.md, CHANGELOG.md and docs/ — no new development this session.

## 2026-10-28
- OTAC Ear Pro compat survey (2026-10-27): coexists with LC as is, no shared files/classes; its audio
  shapes the game mixer only. Agreed route: generic LC hook + a separate compat addon on the OTAC side.
- Checked local vs GitHub: local `main` = origin/main 71b7e2b (2.0.4 + docs), 9 commits past tag v2.0.1;
  GitHub's latest release is still v2.0.1 (2.0.2-2.0.4 never got GitHub releases). Nothing uncommitted.
- Built the hook on `hearing-hook`, answering OTAC's review (zero = bypass before clamps, per-source
  clearing, combination rules, 1 s expiry on `System.GetTickCount()`, room reverb heard through the
  headset, radio separate, no protocol bump). Contract in docs/developer-reference.md.
  Evidence: `plugin\build.bat` clean at /W4, "All checks passed" incl. new parse + headset DSP tests;
  `reforger_check_enforce` clean; Workbench log 07:45 Game module OK (5697 files) with LC loaded.
- Open: in-game test with a caller; tune with the OTAC side (High voice gain, drive amount).
- Branch `radio-tuning` (on top of `hearing-hook`): radio gain 0.8 -> 1.85 with a soft limiter (a04a4ea).
  Measured through lc_audio: full-volume radio was 5-6 dB under a talker beside you; now +2.1 dB at speech
  level, +1.5 raised, level at a shout, no clipping. Tests pass. Not yet heard in game.
- Survey written: docs/research/2026-10-28-terrain-autoconnect-vehicle-comms.md (terrain options A-D, CVON
  3.0.1 auto-connect, vehicle radios/intercoms in CVON/TFAR/ACRE2). Waiting on Duke: terrain option, go for
  auto-connect, vehicle comms scope.
- Radio gain 1.85 -> 1.72 (+18% over a voice beside you at speaking level), channels default to 80% (11405a3).
- Terrain A+B built, **uncommitted** on `radio-tuning`, awaiting Duke's OK on the numbers: game clearance is
  now the worst ground's height above the line of sight (no near-end blow-up, no 10 m minimum); plugin takes
  share = 0.5 * (1 - e^(-h/60)) of the range (no d/2000 term). Tests pass, Workbench compile OK 09:34.
  Docs (how-it-works Terrain, LC_Settings terrain description "100 is the TFAR model") still to update.
- Terrain revised (still **uncommitted**): game sends every hill above line of sight (`links[].hills`, top 8,
  `clearance` = highest kept for old plugins); plugin combines them as shares of what is left, per hill
  0.8 * (1 - e^(-h/95)), total cap 90%. Small hills as before, big ones stronger (250 m: 49% -> 74%).
  Build + tests pass, Workbench compile OK 09:44. Awaiting Duke's OK on the numbers; docs still to do.
- Terrain approved by Duke and committed with docs (how-it-works Terrain table, developer-reference links[],
  player guide, server settings/setup, in-game terrain setting description, CHANGELOG).

## 2026-10-29
- Duke is about to publish 2.0.5 (hearing hook + radio-tuning). Hook files unchanged since bb57f84; radio
  path now ends in the radio soft limiter after channel volume.
- OTAC handover revision 2 written for the OTAC side (outside the repo, Documents/Claude/
  LC-OTAC-EarPro-Handover.md): final contract, answers to OTAC's 28 Oct review, example rewritten for
  their static WriteSignals/UpdateLocalHearing (signatures to verify against their source), joint tests.

## 2026-10-29
- Release notes since 2.0.1: docs/release-notes-2.0.5.md; CHANGELOG 2.0.3 now lists the server.json log line.
- Clean plugin build from radio-tuning 46a874e: plugin/build/LimaCharlie_2.0.5.ts3_plugin (package.ini 2.0.5,
  DLL carries 2.0.5, vanilla beeps excluded), all checks passed. Not yet tested in game.
- New on GitHub, not ours this session: feature/3.0-comms and claude/gallant-knuth-zna9uk (3.0 plan docs only).

## 2026-10-31
- feature/3.0-comms: merged `main` 2.0.5 (`324d2e9`; only PROJECT.md/WORKLOG conflicts); handover updated.
- Groundwork step 3 done (`100fb82`, `lc_radio_receive`): rebuilt here, "All checks passed".
- Surveys of handover step 2 written from source: docs/research/2026-10-31-vehicle-prefabs-compartments.md,
  -von-entry-api.md, -cvon-vehicle-radio-approach.md (CVON has no licence). Design they settle is in the plan
  (B → Survey results): LC component via base-prefab delta overrides; vehicle radio = LC radio child entity
  listed as a normal VON radio entry, tuned via server RPC; seat events + one client reconciler; intercom entry
  never active (+ RadioToggle null fix); infantry phone on `push_rear` (BTR70, LAV25).
- Open for Workbench: two mods' overrides of one base prefab both survive; proxy tuning a vehicle-owned radio;
  compartment events on non-owner clients. Next: groundwork 1 (radio source/antenna in bridge, protocol 10)
  and 2 (server lease).
- Duke's third round recorded in the plan: relays repeat a configurable band; SATCOM needs open sky (server
  upward traces on deploy + every ~10 s, one replicated bool); release 3.0.0, tests now `3.0.0-test.N`;
  infantry phone BTR-70/LAV-25 only, added to modded vehicles by one prefab override (component + action);
  old branch deleted by Duke. Still open: SATCOM factions, jammer strength (explained, 0-100% recommended).
- Jammer strength decided: GM attribute 0-100%, default 100% (peak jam at the jammer). Only SATCOM factions open.
- SATCOM factions: US and USSR arsenals for now; modded factions add it to their own. No open questions left.
- Started groundwork 1 (radio source + antenna position in bridge, per-radio links, protocol 10).
- Groundwork 1 landed (protocol 10, `3.0.0-test.1` both halves): `radios[]` gain `src` (held/pack/borrowed/
  vehicle/speaker; pack = radio entity is the worn `LoadoutBackpackArea` cloth, vanilla precedent
  SCR_RadioOperatorRoleCallsign) and `pos` (= self.pos for body radios, i.e. what RTX carried before). RTX
  carries antenna pos + radio id + src; `radioRx` is `playerId,radioId,src,x,y,z;`; `links[]` gain `radio` and
  are keyed per transmitting radio (game LC_RadioLinks + plugin `lc_game_state_find_link`). Head override
  (plugin players[] / game EyePosition) kept only for held/pack. Radio change restarts the 300 ms terrain wait.
  Evidence: build "All checks passed", 0 warnings (new tests: RTX round trip, per-radio links, held radio =
  old result; mutation of `lc_radio_transmitter_position` caught by 3 checks); enforce_check clean (37 files);
  Workbench compile `Module: Game; loaded 5697x files; 11156x classes` (logs_2026-10-31_18-38-44, no LC errors).
  Not tested in game: Duke to run 2 clients on TeamSpeak (radio heard as before, terrain as before, manpack).
- Verified groundwork 1 (`d00d805`) myself: plugin rebuild "All checks passed" (3.0.0-test.1 package), enforce_check 37 files clean, Workbench log 18:38 Game module 5697 files, no script errors. Needs Duke's 2-client test.

## 2026-11-02
- Groundwork 2 landed: server-arbitrated radio leases. `LC_RadioLeases` (Radio/LC_RadioLeases.c) lives on the
  modded `SCR_BaseGameMode`, keyed by the radio entity's RplId, replicated as one `[RplProp] array<int>` of
  (radio, player, kind) triples: no radio prefab override, JIP via the RplProp, any client can ask who holds a
  radio. Non-exclusive. Kinds BORROWED/VEHICLE map to `src` borrowed/vehicle. Rules (Radio/LC_RadioLease.c):
  distance (handset 4 m, 0.5 m slack while held), seat (ECompartmentType + `CommanderTurret`, default pilot +
  commander + gunner, `IsSeatAllowed` override point); `CollectRules` is the override point for consumers; no
  rules = refused. Server refuses dead / no character / no radio / own radio / out of range / wrong seat;
  duplicate requests are idempotent grants. Ends: release RPC, OnPlayerKilled(+Handled), OnPlayerDisconnected,
  500 ms check only while leases exist (radio gone, controlled entity changed, dead, rule fails), seat events
  of the holder for seat rules. RPCs on LC_PlayerController (request, release, refused). LC_Client appends one
  SCR_VONEntryRadio per transceiver of each held radio after vanilla's (not in the VON menu), so `radios[]`
  lists it with src + the entity's world position and TX keys reach it; plugin unchanged (already parses src/pos,
  protocol stays 10). Diag-only path: Diag menu > Lima Charlie > "Lease nearest radio" / "Lease this vehicle's
  radio" (LC_RadioLeaseDiag.c, `#ifdef ENABLE_DIAG`); unticking releases all.
- Evidence: enforce_check 40 files, no problems; Workbench compile logs_2026-11-02_10-08-21
  `Module: Game; loaded 5700x files; 11171x classes` (5697 + 3 new), no SCRIPT (E), no LC warnings. Plugin not
  touched. Not exercised in play mode: the MCP has no way to tick a diag option or give input, and there is
  no radio placed near a spawn to lease.
- Duke to test (Workbench SP): drop a manpack (or stand by an AI radioman), Diag menu > Lima Charlie > Lease
  nearest radio: log `[LC] Radio lease granted` + `[LC] Radio leases held: 1`, radio appears in game_state.json
  `radios[]` with src borrowed; walk > 4.5 m away: `ended ... OUT_OF_RANGE`. In an M151/UAZ driver seat, Lease
  this vehicle's radio: granted; move to a cargo seat: `ended ... WRONG_SEAT` (not listed in radios[]: no gadget).
- Workstream B part 1+2 (vehicle radios), session started from the 3.0 lead session:
  - WB check 1 answered (Workbench 14:54, ClaudeDevTools dev-only override of `Wheeled_APC_Base` adding a dummy
    `SignalsManagerComponent` beside LC's): a placed BTR-70 listed both LC_VehicleCommsComponent and two
    SignalsManagerComponents, so two mods' delta overrides of one base `.et` layer per block. Side fact: the engine
    refuses a 2nd SignalsManagerComponent on a Vehicle ("cannot be combined") - a dummy of that type was a bad pick.
    Dummy removed again. Checks 2 (proxy tuning a vehicle-owned radio) and 3 (seat events on non-owner clients)
    need MP with 2 clients; the design no longer depends on either: tuning/power go through a server RPC, the
    reconciler only reads the local player's own seat.
  - Built: `LC_VehicleCommsComponent` (Vehicle/; attributes: radio on/off, radio prefab, antenna offset `0 2.5 0`,
    radio seats = LC_ERadioLeaseSeat flags default pilot+commander+gunner, military-only) in delta overrides of
    `Wheeled_Car_Base` + `Wheeled_Truck_Base` (military-only 1), `Wheeled_APC_Base`, `TrackedVehicle_Base`,
    `Helicopter_Base`. Server spawns `Prefabs/LC/Vehicles/LC_VehicleRadio.et` ({6A5C69DD20000080}: RplComponent,
    BaseRadioComponent 1 RadioTransceiver 30-76 MHz 5 km, SCR_RadioComponent, SignalsManager; no mesh, physics,
    inventory or actions so nobody can pick it up) one frame after init, AddChild at the antenna (BaseRplComponent
    replicates hierarchy changes by default), key + platoon frequency from the vehicle's default faction,
    RplProp RplId for clients. `LC_VehicleSeat` (client reconciler in LC_Client.Tick): seat reaches radio ->
    VEHICLE lease request (retry 3 s) -> on grant `LC_VONEntryVehicleRadio` per transceiver via AddEntry; seat
    change/death/leave -> RemoveEntry + release; re-adds entries vanilla dropped (`LC_HasEntry`). Entry = ordinary
    SCR_VONEntryRadio that also sends frequency/power to the server (`LC_TuneLeasedRadio`/`LC_PowerLeasedRadio`,
    lease-holder check). Lease seat rule takes the comms component's seat flags. Plugin: reception distance per
    receiving radio from its own antenna (`lc_radio_receiver_distance`, radios[].pos), best quality wins.
  - Evidence: plugin build "All checks passed", no warnings (new test_receiver_antenna); enforce_check 43 files clean;
    Workbench compile logs_2026-11-02_14-53-42 `Module: Game; loaded 5703x files; 11177x classes`, no LC errors.
  - NOT verified: radio spawn in play mode. The WB play session froze while the window was unfocused (known
    gotcha) and the only test vehicle was the BTR broken by the dummy; the UAZ placed later never made it into a
    session, and the MCP could not stop play (reports edit mode). No `SpawnEntityPrefab ... LC_VehicleRadio` line
    seen. First thing for the next session: place a UAZ469/LAV-25, play with the window focused, grep the log for
    `LC_VehicleRadio.et`; if absent, print-debug EOnInit/SpawnRadio (InPlayMode / Replication.IsServer at init).
  - Terrain links still measure from the listener's eye, not the receiving radio (fine seated; touch-up).
  - Left for the next session: part 3 intercom, part 4 infantry phone (see hand-over note in the side-session notes
    folder and the plan B section).
- 2026-11-02 (B part 2 follow-up, Duke's asks): **key fill + per-player mute on vehicle radios.**
  - Key fill: VON-menu input `LC_RadioKeyFill` (default `Y`, "Load Radio Key"; control hint only while the local
    player's seat reaches a vehicle radio, `LC_VehicleRadioSeatCondition`) on a hovered `LC_VONEntryVehicleRadio`
    -> `LC_LoadKeyLeasedRadio` RPC -> server checks the lease and sets the player's faction key. The key is an
    `[RplProp] m_sRadioKey` on `LC_VehicleCommsComponent`, applied to each machine's own copy of the radio (on
    change and when the radio resolves), because nothing documents that `SetEncryptionKey` replicates and both
    vanilla's pre-transmit key check and the key sent to the plugin read the local copy. A radio with no key
    (vehicle with no default faction) takes the first crew member's key when their lease is granted.
  - Mute recheck: vanilla `SCR_VONEntryRadio.ToggleMuteEntry` calls `BaseTransceiver.SetMuteState` on the
    transceiver, and `GetIsMuted` reads it back from there (`SCR_VONEntryRadio.c:234-260`). The engine API documents
    neither replication nor ownership for it (unlike `SetFrequency` "supports proxies and server" and `SetRange`
    "owner only"). On a shared, server-owned vehicle radio that is either one player's mute silencing the crew,
    or a change that does not hold on a proxy. `LC_VONEntryVehicleRadio` now overrides both to keep mute on the
    entry only (m_bIsMuted; the menu's mute icon already reads that). `LC_Radio.CanReceive` honours it via
    `GetIsMuted`. Mute resets when the entry is rebuilt (seat change). Carried radios unchanged (owner = holder).
  - Evidence: enforce_check 44 files clean; Workbench compile in logs_2026-11-02_17-08-47 at 17:09:51
    `5704 files / 11178 classes` (the +1 is the new condition class), no LC errors. That instance belongs to the
    intercom session; it crashed afterwards on "Resources are leaking" while reloading scripts mid-play, which is
    not this change. NOT verified in game: key fill, mute, the hint.
  - In-game tests for Duke: capture an enemy vehicle (or GM-place one), sit in a crew seat, try to transmit on its
    radio (blocked hint), hover it, press Y, transmit again (works; the other crew member can too). Two crew: one
    mutes the vehicle radio with M; the other still hears it.
- 2026-11-02 (B parts 3-4 session, started from the 3.0 lead session):
  - **Step 0, vehicle radio spawn in play mode: NOT verified.** My Workbench (ClaudeDevTools, testWorld auto-opened
    on launch) crashed when my `wb_open_resource worlds/testWorld.ent` reloaded the world while it was already
    loading ("Resources are leaking!" assert, then `ENGINE (F): Crashed`, logs_2026-11-02_17-08-47). Lesson: a
    ClaudeDevTools launch reopens the last world by itself; wait for it rather than opening it again. Duke then
    had the League client up, so I did not take window focus for a play session. Left a `LogLevel.DEBUG` line
    `[LC] Vehicle radio mounted on <vehicle> (<rplId>)` in `LC_VehicleCommsComponent.SpawnRadio` for Duke's test.
  - **Part 3, intercom, landed.** Bridge: `self.ic` + `players[].ic` (crew 1 / cargo 2 / misc 4), both optional,
    so **protocol stays 10** (older plugin ignores them, older game sends none). Plugin: `lc_intercom_gain`
    (lc_direct_voice.c) + a third audio path in `lc_audio.c` (`lc_voice_target.intercom`): same voice again,
    centred, no distance/muffle/room, band-pass 250 Hz-5 kHz, radio-path headset, soft limit; heard even out of
    voice range. Game: `LC_EIntercomChannel` + `LC_VONEntryIntercom` (Vehicle/LC_Intercom.c); comms component
    gains `m_bIntercom`, `m_iIntercomCrewSeats` (default pilot+commander+gunner = crew, rest cargo),
    `[RplProp] m_aIntercomChoices` (player, channel) pairs of picks that differ from the seat's start, written
    only by the server (`ServerSetIntercomChoice` via `LC_SetIntercomChannel` RPC, refuses non-members, prunes
    leavers). Membership is computed on every machine from seat occupancy (phone: lease, part 4 hook
    `GetPhoneChannel` / `FindPhoneIntercom`). `LC_VehicleSeat.Update` split into UpdateRadio + UpdateIntercom:
    lists the entry last (moves it back behind radio entries added later), holds the local pick. Patches:
    `SetEntryActive` refuses the entry, `RadioToggle` null-checked (Radio/LC_RadioToggle.c), `OnAdjustEntry`
    cycles it without the frequency table. Docs: developer-reference "Vehicle intercom (3.0)", player-guide
    "Intercom", CHANGELOG.
  - Evidence: plugin build "All checks passed", no warnings (new `test_intercom`: gain bits, `ic` parsing incl.
    absent = 0, audio heard with zero direct gain under full muffle, band-pass, headset); mutation (intercom path
    off) caught by 5 checks, rebuilt clean. enforce_check 46 files clean. Workbench compile logs_2026-11-02_17-24-28
    `17:24:36.503 Module: Game; loaded 5706x files; 11182x classes`, 0 `SCRIPT (E)`.
  - NOT verified in game: anything. Open questions for Duke's MP test: is `GetCompartment()` set on proxies
    (other players' seats; membership depends on it), does the radial centre label follow the new name on
    scroll (only `SetName` + `UpdateEntries`), does moving the entry back to last close an open menu (vanilla
    `RemoveEntry` does; should only happen once, just after a lease grant).
  - **Part 4 (infantry phone) not started**: stopped at the context limit. Hooks are in place
    (`LC_VehicleCommsComponent.GetPhoneChannel`, `LC_VehicleSeat.FindPhoneIntercom`); a step-by-step hand-over
    (PHONE lease kind, phone-point distance rule, component, action, BTR-70/LAV-25 overrides reusing vanilla block
    IDs, docs) is in the side-session notes folder, `workstream-B-part4-handover.md`.
- 2026-11-02 (B part 4 session): **infantry phone landed.** B is complete in code.
  - `LC_ERadioLeaseKind.PHONE`: the leased entity is the vehicle (needs `LC_InfantryPhoneComponent` instead of a
    radio). Rules: `LC_RadioLeasePhoneRule` (distance from the phone's action context, `GetAnchor` override point
    added to `LC_RadioLeaseDistanceRule`) + `LC_RadioLeaseOnFootRule` (no seat at all; watches seats, so getting
    into any vehicle hangs up). Granted only if the vehicle has an intercom. New `GetHolderKind(player, radio)`.
    The crew-key fill on grant now only runs for `VEHICLE` leases (a phone pick-up no longer keys the radio).
  - `Vehicle/LC_InfantryPhone.c`: component (context name `push_rear`, cable 5 m, channel Misc; phone point =
    `GetContext(name).GetTransformationWorld`, else vehicle origin) and `LC_InfantryPhoneAction` (shown on foot
    with an intercom, local effect only, Pick Up / Hang Up by lease, picking up hangs up any other phone). Hooks
    filled: `GetPhoneChannel` (holder kind PHONE -> phone channel), `FindPhoneIntercom` (own PHONE lease ->
    vehicle intercom). No bridge or plugin change.
  - Prefabs: delta overrides `Prefabs/Vehicles/Wheeled/BTR70/BTR70_Base.et` ({1C5FE7B7FF49BB8D}) and
    `LAV25/LAV25_base.et` ({1B8EB269EECA2DF9}): the component plus the action in vanilla's
    `ActionsManagerComponent "{C97BE5489221AE18}"` additionalActions. Workbench `getInfo` on both resolves to one
    actions manager with the phone action beside vanilla's Push / Open Trunk / handbrake, plus LC comms from the
    APC base override.
  - Docs: developer-reference "Infantry phone (3.0)" (one-override recipe for modded vehicles, LAV-25 example),
    lease kinds/rules, script map; player-guide; CHANGELOG. Wiki (workspace `a518e46`): subclass constructors
    must match the parent's argument types (first compile failed on it), action delta override appends.
  - Evidence: enforce_check 47 files clean; Workbench compile logs_2026-11-02_17-37-38 `17:37:44.520 Module: Game;
    loaded 5707 files / 11187 classes` (+1 file, +5 classes), 0 `SCRIPT (E)`. Plugin untouched (no rebuild).
  - NOT verified in game: anything of B. No play-mode session this time (the step-0 radio spawn is still open).
  - **Duke's in-game test list for B** (MP, 2 clients, plugin 3.0 test build; grep the logs for `[LC]`):
    1. Radio spawn: place/spawn a LAV-25 (or UAZ) and play: log has `[LC] Vehicle radio mounted on ...` (DEBUG level)
       and no `Vehicle radio prefab not found`.
    2. Seat lease: sit as driver/commander/gunner: `[LC] Radio lease granted: ... VEHICLE`, the vehicle radio is
       in the VON menu; move to cargo: lease ended (`WRONG_SEAT`), entry gone; get out: same.
    3. Tuning: one crew member changes the vehicle radio's frequency; the other sees it; a cargo player cannot.
       Transmit on it and be heard on a hand radio on the same frequency, from the vehicle's position.
    4. Key fill / mute: enemy vehicle (or GM-placed without faction key): transmit blocked, hover + Y, works for
       both crew; one crew member mutes it with M, the other still hears it.
    5. Intercom channels: driver + cargo player: driver on Crew, cargo on Cargo; they hear each other only
       directly. Cargo scrolls the Intercom entry to Crew: now heard clear, whisper too, engine running. No
       Intercom: off. Check the radial centre label follows the name on scroll, and whether the menu closes when
       a radio lease is granted while it is open.
    6. Phone (BTR-70 and LAV-25 rear): on foot, Pick Up Infantry Phone: `[LC] Radio lease granted: ... PHONE`,
       Intercom entry appears on Crew; the driver/commander/gunner hear you with no switching; walk > ~5.5 m:
       `OUT_OF_RANGE` end, entry gone; pick up again then Hang Up; pick up then get into any vehicle seat
       (`WRONG_SEAT` end); pick up then die. The action must not show on a civilian vehicle or from inside.
- 2026-11-02 (Duke): **phone default channel Crew, not Misc.** `LC_InfantryPhoneComponent.m_eChannel` default 1
  (Crew); a misconfigured NONE now falls back to Crew. Docs, CHANGELOG, plan (decision), test list updated
  (item 6 above already reads Crew). The prefab overrides set no channel, so they follow the default.
- Wrote docs/design/3.0-test-plan.md: test list for the whole branch (start-up/versions, 2.0.5 regression,
  leases, vehicle radios, intercom, phone, mods alongside, expected behaviour). CHANGELOG gained the missing
  vehicle radios line.
- Duke's fourth round (plan): handset and phone picked up by an interaction; phone action on the passenger door
  for now (BTR-70, LAV-25) instead of `push_rear`; a visible black cable with slack from holder to vehicle/radio,
  generic for the manpack handset too; check vanilla for rope/cable first. Handed to a fresh session.
- 2026-11-02 (fourth-round session): **phone moved to the passenger door; handset in hand and a cable.**
  - Door: BTR-70 cargo seats use `door_l01` (DoorInfoList 0) and `door_r01` (1), listing `{1 0}` (right first);
    LAV-25 cargo seats use `door_back_left` (2) / `door_back_right` (3); `door_side_left` (1) is not cargo.
    Picked the right-hand one on both: `door_r01`, `door_back_right` (Duke: right-hand if two). Overrides now set
    `m_sContextName` and the action's `ParentContextList` to it; the component's context default is now empty
    (= vehicle origin), so a modded vehicle names its context explicitly. Distance rule unchanged (it already
    measures from `GetPhonePosition`, now the door).
  - Research (docs/research/2026-11-02-handset-cable.md): vanilla has no rope/cable simulation; it builds meshes
    from script (`MeshObject.Create` + `UpdateVerts`/`UpdateIndices` + `SetObject`, rebuilt on move in
    `SCR_BaseAreaMeshComponent`); RHS's tow cable draws its visual the same way (approach only). Loiter
    animations attach local props to hand bones (`SpawnEntityPrefabLocal` + `AddChild(bone)`).
  - Built `Radio/LC_HandsetCable.c`: `LC_HandsetCable` (handset prop on `LeftHandProp` + 4-sided 16-segment tube,
    parabolic sag from slack, terrain clamp, rebuilt only when an end moves > 1 cm, local entity at the hand end)
    and `LC_HandsetCables` (per client, per frame from `LC_Client.Tick`: one cable per lease of a cable kind,
    PHONE now; hidden beyond 100 m of the camera or when streamed out). `LC_RadioLeases` gained
    `GetLeaseCount` / `GetLeaseAt` (all leases, any player). Material: vanilla gantry crane cable emat.
  - Duke (in this session): the vanilla `PhoneWall_01_dst_02_blue` handset in the holder's hand. Built
    `Prefabs/LC/Props/LC_InfantryPhoneHandset.et` ({2F7C1A6D93B4E058}) from its model `PhoneWall_01_dst_02.xob`
    with the blue materials and no RigidBody (the vanilla debris prefab has a static model-geometry body that
    must not ride on a character); `LC_InfantryPhoneComponent.m_sHandsetPrefab` points at it. A local visual
    prop, not an inventory item: an item would need replication and would take the hands from the weapon.
  - Evidence: enforce_check 48 files clean. Workbench (ClaudeDevTools, restarted with Duke's OK after closing
    his 20:37 instance) logs_2026-11-02_20-50-48 `20:50:55.419 Module: Game; loaded 5708x files; 11191x classes`,
    0 `SCRIPT (E)`. Play mode: entered on testWorld; the player was being created when Workbench stopped on a
    "Script Authorization Required" dialog (vanilla `SCR_ArsenalManagerComponent.c:1101` wants
    `BaseContainerTools.LoadContainer`). Left unanswered (security prompt; Duke was in another game, so no focus
    taken). **NOT verified: anything at runtime** (cable, handset placement, door action position).
  - Open for Duke's look (test plan §6): tube winding, handset offset/rotation on `LeftHandProp` (all zero:
    `HANDSET_OFFSET` / `HANDSET_ANGLES` / `CORD_EXIT` constants in LC_HandsetCable.c), colour, no hold
    animation (the hand keeps the weapon pose). That Workbench instance is still open in play mode behind the
    dialog.
  - Later: Duke answered the dialog (rule from now on: answer it with Yes to All myself; wiki + memory). Play mode
    then ran: `21:31:00.386 [LC] Client started`, the per-frame tick (cable manager included) ran with no
    `SCRIPT (E)` / NULL pointer. No phone picked up (no input from this session), so cable and handset still unseen.
- 2026-11-02 (Duke's first look, Workbench play, LAV-25): phone action, lease and handset spawn work at
  `door_back_right`. Wrong: the cable starts at the hand, not the handset's bottom (debris model origin off the
  handset; offsets all zero); the cable renders white and ribbon-like, not black; the handset hangs in the weapon
  pose. Fixed: the handset prefab's MLOD material remap (`RESOURCES (E) Can't remap ... PhoneWall_01_MLOD`), `abd8e63`.
  Duke asks: **force the radio inspect animation** while holding the phone, and place the handset and cable to
  match it. Handed to a fresh session (context budget); brief in the side-session notes,
  `handset-pose-cable-handover.md`.
- 2026-11-02 (same session, Duke chose to do it here): **radio inspect pose + cable fixes.**
  - Pose: vanilla's radio inspect = a gadget item in the left hand (`TakeGadgetInLeftHand` via
    `SCR_GadgetManagerComponent.SetGadgetMode IN_HAND`) plus `CharacterControllerComponent.SetInspect(item)`, with
    the item's `ItemAnimationAttributes.AnimationInstance` (PRC-68: `Radio_ANPRC68_player.asi`) and the gadget's
    `m_eAnimVariable` (ANPRC68). No way to force it without an item, so (Duke's earlier OK to make it an item)
    `LC_HandsetItem.c`: the server gives `Prefabs/LC/Items/LC_InfantryPhoneHandsetItem.et` ({6E1D2B7C4A905F38},
    `LC_HandsetGadgetComponent`, no mesh) into the holder's inventory on a PHONE grant (vanilla precedent:
    `SCR_IdentityManagerComponent` spawn + `TryInsertItem`) and deletes it on any lease end (`TryDeleteItem`, else
    `RplComponent.DeleteRplEntity`); not in the holder's hierarchy any more -> lease ends "handset dropped". The
    holder's machine (`LC_HandsetCables` -> `LC_HandsetItem.Update`) takes it into the hand and inspects it, retry
    every 500 ms.
  - Handset prop now rides on the held item (bone until then), centred on its own bounds (`GetBounds`), so the
    debris model's off-centre origin no longer matters; the cable starts at the model's bottom (lowest Y, centred).
  - Cable: own material `Assets/LC/LC_HandsetCable.emat` ({9C3E5A1F7B2D4068}, MatPBRBasic black, no specular,
    `Cull none`). Suspected cause of the white ribbon: the script mesh has no normals and the gantry material is
    glossy + textured (not proven).
  - Evidence: enforce_check 49 files clean; Workbench restarted (Duke's OK) logs_2026-11-02_21-42-55
    `21:43:02.780 Module: Game; loaded 5709x files; 11194x classes`, 0 `SCRIPT (E)`; testWorld open in edit mode.
    NOT verified in play: pose, handset placement, cable colour, hang-up while inspecting.
- 2026-11-02 (Duke's correction): **hold, don't inspect; don't force.** The phone gives the radio *hold* pose
  only: no `SetInspect` (inspection hid the world actions, e.g. putting the item away, and forced free look),
  `AllowInspection` removed from the item. The holder's machine takes a newly given handset item into the left
  hand once (retry every 500 ms, at most 6 tries) and then leaves it alone: pulling out something else ends the
  pose and the phone stays held (cable falls back to the hand bone); dropping the item hangs up (unchanged).
  Evidence: enforce_check 49 files clean. Workbench: `wb_play` at 21:51 logged `Workbench Reload Game` and no
  `SCRIPT (E)`, but no new `Module: Game` line, so this revision's compile is not confirmed by the log. Play
  mode left running for Duke.

## 2026-11-03
- Dev builds renamed `3.0-dev-NN` (Duke): version `3.0-dev-01` on both halves; plugin rebuilt
  (`LimaCharlie_3.0-dev-01.ts3_plugin`, "All checks passed"). Addon files copied to Duke's new Workbench project
  `addons\Lima Charlie DEV` (ID LimaCharlieDEV, GUID 6AC643FAF3F385E3), all but its own addon.gproj / .rdb;
  diff -rq shows only those two differ. Not yet opened/compiled in Workbench as the DEV project.

## 2026-11-04 (feature/3.0-comms: 2.0.6 merged in)
- Merged `bugfix-2.0.6` (c56799c, all of 2.0.6: fixes, performance, diagnostics, picker, VON display, colours)
  into feature/3.0-comms at 17bf712 (merge 0603fc5), in a separate worktree so the 2.0.6 checkout was untouched.
- Conflicts: writer (3.0 builds radios inside BuildInGameJson for antenna positions; kept, string kept in
  `m_sWrittenRadios` for the regular-diagnostics radios line; per-player JSON built alone with 3.0's `ic`),
  plugin direct voice (2.0.6 muffle/room glide feeds 3.0's intercom-aware compute; both helpers kept), VON menu
  (OnAdjustEntry + LC_Reopen), versions (stay `3.0-dev-01`, protocol 10), CHANGELOG (3.0 above 2.0.6), PROJECT, this log.
- Found beyond the textual conflicts: (1) leased radios were appended after 2.0.6's "editor channels last" list;
  now own radios, leased, then GM editor channels (`SCR_VONController.LC_AppendRadioEntries`). (2) Both branches
  had taken config object IDs `{6A5C69DD1000004E..51}` (3.0 LC_RadioKeyFill input + keybind entry; 2.0.6 picker
  keys 1-2); 3.0's moved to `{6A5C69DD1000005A..5D}`. No duplicate `{6A5C69DD...}` ids left in .conf/.et/.layout/.emat.
- DEV addon: only the 19 changed addon files copied (not addon.gproj/.rdb); `diff -r --strip-trailing-cr` against
  the branch shows only DEV's own gproj/rdb and the two known `.edds.meta` newlines.
- Evidence: enforce_check 49 files clean; plugin build /W4 clean, "All checks passed",
  `LimaCharlie_3.0-dev-01.ts3_plugin`; second Workbench on the DEV gproj logs_2026-11-04_13-46-12
  `13:46:17 Module: Game; loaded 5709x files; 11201x classes`, no SCRIPT (E) (closed again; Duke's instance untouched).
- NOT tested in play or MP: everything 2.0.6 lists as untested, plus the combined order (editor + vehicle +
  leased radios), the keybind for Load Radio Key under its new entry id, and the glide on intercom speech.
- Duke's call: whether this build is `3.0-dev-02` (left at `3.0-dev-01`).
- Duke: bump to `3.0-dev-02`. Both halves (`LC_Version.c`, `lc_version.h`) and the test plan; plugin rebuilt,
  "All checks passed", `LimaCharlie_3.0-dev-02.ts3_plugin` (package.ini and DLL carry 3.0-dev-02); DEV addon's
  LC_Version.c copied; enforce_check (DEV) 49 files clean; second Workbench on the DEV gproj logs_2026-11-04_22-27-11
  `22:27:16 Module: Game; loaded 5709x files; 11201x classes`, no SCRIPT (E) (closed again). Not run in play.
- Merged `main` 18d6afe (2.0.6 release: release notes, radio bleed research, worklog; no addon or plugin change).

## 2026-11-03 (bugfix-2.0.6)
- Unit session logs (2.0.5, 4 clients) triaged before reading the external handover:
  docs/investigations/2026-11-03-session-logs.md. Confirmed: GM channel scramble = positional TX-key default
  over vanilla `m_aEntries`, reordered by vanilla `RemoveEntry`'s unordered `RemoveItem` (Tim's log, editor close);
  radio stutter contributor = OTAC `OT_VonDisplay` `m_wRole` VM exceptions on the "+N" VON slot, re-raised by our
  20 Hz feed (748 in the logs). Code hot spots (rooms Bellman-Ford per write, 117-area apartments; traces) not
  measured. Reverb fade in big spaces is by design. Terrain not captured. Nothing committed in the project.
- External FPS handover read and compared (investigation doc, "Comparison" section). Verified their room-scanner
  no-Finish bug, per-trace collider volume under room diagnostics, outdoor use of indoor graphs, LC reset at the
  20:47 reload; Linstone's plugin is 2.0.1; Parko's clock is UTC+10:30. Did their open diagnostic-flag audit: flags off
  = no optional work. Revised: mapped layouts add area-wide solver work over a session (fits "runway fixes it").
- Layout change (Duke): `addon\` (Workbench "Lima Charlie") now holds the **2.0.x line**; branch `bugfix-2.0.6` from
  `main` (= v2.0.5 + release docs). The 3.0 work lives in Workbench "Lima Charlie DEV", junctioned as `addon-dev\`
  (local only, in `.git/info/exclude`). DEV matched feature/3.0-comms 17bf712 at the switch (only two .edds.meta
  trailing newlines differ). feature/3.0-comms is 9 commits ahead of origin (unpushed).
- **2.0.6 fixes** on `bugfix-2.0.6` (both halves `2.0.6`, protocol 9):
  - scramble: modded `SCR_VONController.RemoveEntry` puts back the entry vanilla's `RemoveItem` swapped in;
    `LC_GetRadioEntries` lists GM editor entries last.
  - VON overflow: `LC_VonDisplay.LC_IsStuckOverflow` (additional and never active); the feed skips those speakers.
  - rooms: `Build` finishes on grid exhaustion, cap inside the loop; per-area adjacency at `Finish`; `LC_RoomPaths`
    cache (8 slots, keyed subscene + layout + source area, LRU), portals re-read every 250 ms, search
    (queue-based) only on a cost change.
  - diag: collider volume only when the room line is due.
  - plugin: per-player muffle and room glide (`lc_direct_voice_ease`, tau 200 ms) in `publish_voice`.
  - Evidence: plugin build /W4 clean, "All checks passed", `LimaCharlie_2.0.6.ts3_plugin`; enforce_check 37 files
    clean; Workbench (ClaudeDevTools → Lima Charlie) logs_2026-11-03_22-54-22 `22:54:28.683 Module: Game; loaded
    5697x files; 11155x classes`, 0 `SCRIPT (E)`.
  - NOT tested in play or MP: editor close and radio drop order, overflow with OTAC, apartment frame times, muffle
    glide by ear.

## 2026-11-04 (bugfix-2.0.6)
- Room occlusion / building paths performance pass: docs/investigations/2026-11-03-room-occlusion-performance.md
  (code read first, 2.0.6 investigation compared after). 2.0.6 already removes the big recurring cost (per-write
  solve, ~50x fewer script calls in the apartment). Still open, unmeasured: 8 path slots thrash at 9+ live
  buildings; `Locate` sphere query every write for any moving player (0.5 m OR 500 ms) and per-child callbacks;
  `GetPrefabName` per speaker per write in `GetLayout`; trace filter does casts + prefab name before the width
  test; portal costs read per slot not per building; unguarded portal index on a variant building. No code changed.
- Rooms fixes 1-5 + 7 committed (f9480cc): 32 path slots, per-building shared portal costs with global revision,
  Locate 150 ms minimum + per-query visited list, layout cached on LC_RoomLocation, layout ignored (and never
  mapped) when the instance's area/portal counts differ from the prefab's map, FilterCover width first. Evidence:
  enforce_check clean; Workbench logs_2026-11-03_22-54-22 `00:08:53 Module: Game; loaded 5697x files; 11158x
  classes`, no LC errors. Not run in MP.
- Whole-mod performance pass with Workbench timings: docs/investigations/2026-11-04-whole-mod-performance.md
  (bench sources + raw output in docs/investigations/bench/). Top items: mapping 3.6 ms/write x 313 writes in
  the apartment and never completes (cap 40,000 < ~71,000 cells, 87 portals unfound); game_state.json write
  ~0.4 ms/write (proxy); writer JSON 190-485 us/write (one append per player saves 13-30%); plugin_state poll
  108 us/50 ms; GetId rebuilt ~4x per radio per write. Steady state ~0.8-1 ms/write. No code changed by the pass.
- Waiting on Duke: mapping budget by portal count and/or layouts saved to $profile; writer JSON one-append;
  GetId cached on the entry.
- Duke: "Start on the fixes" (mapping budget + saved layouts, writer JSON, radio id cache). Done on bugfix-2.0.6:
  - mapping batch `clamp(12000/portals, 8, 128)`, cap 120,000; layouts saved/resumed in
    `$profile:LimaCharlie/rooms/<GUID>.txt` (every 4,000 cells, on finish, on walking out; strict validation).
  - writer: one append per player. Radio id + duplex cached on modded `SCR_VONEntryRadio` (keyed on transceiver).
  - Evidence: enforce_check clean; WB logs_2026-11-04_07-56-21 Game module 5697 files / 11158 classes, no LC
    errors; LCBench_Map in play mode: apartment 1.13 ms/write (was 3.6), 1,433 writes, all 310 portals found
    (270 usable, was 185), save/load/resume identical; house unchanged 0.45 ms. Radio ids correct in play-mode
    game_state.json. Docs: how-it-works, troubleshooting, CHANGELOG, whole-mod investigation follow-up.
  - Pushed to origin/bugfix-2.0.6 (cd86857) on Duke's go. NOT tested in MP. Bench left out of ClaudeDevTools (EMCP_WB_Ping.c restored). Bench room files remain in the
    Workbench profile (`profile/LimaCharlie/rooms/`), written by the mod itself.


## 2026-11-04 (bugfix-2.0.6, afternoon)
- Duke's list: picker reopens radio menu; status notice lifetime + mic line; diagnostics rework.
  - Picker: `LC_KeyAssignMenu` sets a flag on Choose/Back; `OnMenuClose` calls (100 ms later) modded
    `SCR_VONMenu.LC_Reopen` → `m_RadialController.OnInputOpen()` (the VON menu is a toggle,
    `m_bCloseOnReleaseOpen 0`; any ChimeraMenu open closes the radial via `SCR_RadialMenu.OnMenuOpened`).
    Not reopened when something else closed the picker.
  - Notice: root cause of "keeps displaying" = `SCR_HintEditorComponent` sets hint duration override -1
    (endless) while the GM editor is active. Notice now: once per connection, 1.5 s after first spawn (or
    editor open), hidden by us after 12 s (`HideHint(info)` only hides our own hint); no re-show on change;
    "Microphone: on/muted" line.
  - Diagnostics: `LC_EDiagnostics` OFF / REGULAR (default) / COMPREHENSIVE (= old diagnosticLog). Flags bit 3
    = regular. server.json key `diagnostics`; legacy `diagnosticLog: true` → COMPREHENSIVE, `false` ignored
    (every generated file has it). Regular = tx start/end, lockouts, rx start/end with worst quality, plugin
    stale/back, radios on change (3 s check), listener building/room on change, 60 s summary of counters.
    Room log: per-player part built only on the printing write.
  - Evidence: enforce_check 37 files clean; Workbench (ClaudeDevTools → Lima Charlie) logs_2026-11-04_11-49-04
    `11:49:11 OK 5697 files / 11159 classes`, no LC errors. NOT run in play: picker reopen, notice timing in
    GM editor, regular log lines/volume all unverified in game. Workbench left running (launched by me).
- Play-mode check (testWorld, Workbench via ClaudeDevTools, no TeamSpeak running):
  - logs_2026-11-04_11-49-04: settings line `diagnostics regular` with the old server.json
    (`diagnosticLog: false` ignored as intended); notice 1.6 s after `Client started`; `[LC] radios` line;
    `Plugin stopped answering` warning; `[LC] summary 60 s: writes 138, plugin states 1, ...` (2.3 Hz = the
    no-plugin rate). Found: the leftover plugin_state.json (2.0.1, connected) read once counted as a running
    plugin, so the notice said "connected".
  - Fix: `LC_PluginStateReader.IsPluginRunning` needs two different seqs read. Workbench restarted (my
    instance ignored a polite close; force-closed, no edits in it). logs_2026-11-04_11-59-29: Game module OK
    5697 / 11159, notice at 12:04:31.85 = 6 s grace after `Client started` and says "plugin: not running";
    no LC script errors.
  - Still NOT verified: picker reopen (needs real input), the hint actually disappearing after 12 s in the GM
    editor, tx/rx/building lines (need TeamSpeak + another player / a building).
- Duke: picker keys 1-4 / Esc = None; fold OTAC Core's VON display tweaks into LC (approved by OTAC; Duke picked
  background-coloured labels for all, two server settings on by default, no built-in nets).
  - Picker: new `LC_KeyAssignContext` (priority 50, actions LC_KeyAssign1-4 on KC_1-4, click filter) activated
    each frame in `OnMenuUpdate`; MenuBack now = Choose(-1). Not in the keybind menu (fixed keys).
  - VON display: label background = label colour, text white on RED/BLUE/PURPLE else black; unnamed labels
    reset to layout colours (white bg, black text). `hideRadioRoles` / `hideDirectSpeechDisplay` (flags bits
    4/5, server.json keys), applied in modded `SCR_VonDisplay` (OnReceive + UpdateTransmission set
    `m_bIsVONDirectDisabled`; role/SL hidden on receive, null-checked for the overflow line).
  - Evidence: enforce_check clean; WB restart logs_2026-11-04_12-48-53 Game module OK 5697/11159; play mode
    settings line `hide radio roles true, hide direct speech display true`, no input/script errors.
    NOT verified: keys 1-4/Esc in the picker, label look, role/direct hiding (needs input + a second player).
  - OTAC side still has its own `SCR_VonDisplay.UpdateTransmission` override; it should drop it (both would
    colour the label, last loaded wins) — Duke to pass on.
- Custom label colours (Duke: "Make the change"): `LC_EChannelColour.CUSTOM` (appended, numbers unchanged) +
  `ref Color m_CustomColour` (UIWidgets.ColorPicker); packed colour field = palette number or `#RRGGBB`
  (picker colour LinearToSRGB'd to hex on the server; hex parsed by hand with `ToAscii` on both sides); labels
  carry resolved sRGB; text white below Rec.601 brightness 150 (same split as before for the palette).
  - Evidence: enforce_check clean; WB logs_2026-11-04_12-58-41 Game OK 5697/11159; NET API test
    `LCTest_Colours` (bench/LCTest_Colours.c, run from ClaudeDevTools then removed) 8/8 ok: hex either case,
    bad hex -> white, names/numbers, UnpackToText round trip, palette split 01000011, picker
    FromSRGBA(73,179,255) -> "#49B3FF".
  - Assumed, not seen: a picker value in a real .conf is linear like FromSRGBA (vanilla feeds picker colours
    straight to widgets). The example `m_CustomColour 0.0666 0.4508 1 1` in server-settings.md is computed
    from that assumption. Label look in game not seen.
- Research: radio background bleed (battle sound heard behind a radio transmission):
  docs/research/2026-11-04-radio-background-bleed.md. No Arma 3 TFAR/ACRE add-on found (TFAR #1440: devs
  say real capture is impossible, one TS voice stream); Reforger precedent SH Battle Radio is gone (404, no
  archive). Recommended: sender's game collects shots/explosions/vehicle state while transmitting, plugins
  carry them by sendPluginCommand, receiver's plugin mixes samples into that speaker's radio path before
  `radio_tick`. Vanilla has script-readable mixer loudness (`Submix_L4`/`L3`, untested live). No code changed.
  Waiting on Duke: name of the Arma 3 mod; go/no-go and where it sits (after 3.0 suggested).
- Radio background bleed **shelved** by Duke: not a focus, no good way to source the samples. Write-up kept.
- Release notes written: docs/release-notes-2.0.6.md (bug fixes / performance / changes). Plugin
  build/LimaCharlie_2.0.6.ts3_plugin (2026-11-03 22:54) is current with plugin/src. release_check 2.0.6:
  0 errors, 3 warnings (VoiceLevel source PNGs without .meta, already in 2.0.5), enforce clean.
- Duke: commit 2.0.6 and merge bugfix-2.0.6 into main on GitHub. Cover pictures left uncommitted.
