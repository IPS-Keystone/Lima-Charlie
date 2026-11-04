# Work log — Lima Charlie

The running journal (`_Rules/PROJECT-DEFAULTS.md` → Working loop). Read the top at the start of a
session; update after each meaningful step and before ending one. Never rewrite or delete old entries.

## Done
- 2.0.4 released (mod + plugin), protocol 9 (was mis-noted as 6); radio keys open the TeamSpeak mic, transmit-key picker (CHANGELOG.md).
- Direct speech with voice levels, occlusion (room model + traces), room reverb, radios with garble and
  terrain, AI hearing, Game Master support (docs/how-it-works.md).
- Verified in multiplayer earlier: attenuation, muffling, range switching (2-player test, 2026-09-14).

## To do
- Branch `hearing-hook` (2.0.5, testing): `LC_Hearing` hook for ear-pro mods (OTAC Ear Pro compat first).
  Built + offline tests pass; Workbench Game module compiles. Not yet run in game: needs a compat (or a
  test caller) and a 2-client TeamSpeak listening test. Release number (2.0.5 vs 2.1.0) is Duke's call.
- Next planned major: electronic warfare (own branch, merged with the major release).
- Not yet tested: retail game client with BattlEye.
- Decide whether `WorkshopCoverPicture.*` and the new `PROJECT.md` / `docs/research/` / this file get committed.

## Touch-ups
- Terrain answers can be missed: plugin assumes line of sight after 300 ms without reporting it (docs/troubleshooting.md, Known fragilities).
- Wishlist not started: simplex/duplex, vehicle intercom component, babble languages, relays/satcom, EMP.

---

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
