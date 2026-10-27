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
