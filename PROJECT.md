# Lima Charlie — PROJECT.md

TeamSpeak 3 voice/radio integration for Arma Reforger (TFAR-style). Two halves shipped together: the
Enfusion addon + a TS3 plugin (plain C), bridged by JSON files in `$profile:LimaCharlie`.

## Key facts
| | |
|---|---|
| Addon | `Documents\My Games\ArmaReforgerWorkbench\addons\Lima Charlie` (junction: `addon\`) |
| Addon GUID / gproj ID | `6A5C69DDFCAAC5B3` / `LimaCharlie` (published folder `LimaCharlie_6A5C69DDFCAAC5B3`) |
| Script prefix / log tag | `LC_` / `[LC]` |
| Plugin | `plugin\` (src `lc_*.c`, DLL `limacharlie_win64.dll`, TS cmd prefix `LC1|`, build `plugin\build.bat`, packager `plugin\package.py`) |
| Toolchain | MSVC 2022 Build Tools |
| Repo | github.com/IPS-Keystone/Lima-Charlie (`main`), repo root = this folder |
| Status (2026-10-11) | mod + plugin 2.0.4 released; protocol `LC_PROTOCOL_VERSION` 9; 2.0.5 hearing hook on branch `hearing-hook` |
| Licence | as published (check repo LICENSE before changing); third-party: TFAR/ACRE sounds with licences in `plugin\sounds` |
| Target | TeamSpeak **3** (not 6), PC only, MP dedicated servers |

## Project-specific rules (override defaults)
- **Versioning `MAJOR.STABLE.DEV`** in `plugin/src/lc_version.h`: Major = first public release (1.0.0) and
  each major feature (electronic warfare is the next planned major); Stable = bug fixes/small changes;
  Dev = testing. Mod and plugin report the same number. `LC_PROTOCOL_VERSION` moves only on incompatible
  bridge changes and must be bumped with `LC_GameStateWriter.PROTOCOL_VERSION`.
- **New features on their own branch, merged to `main` with the release that introduces them.**
- Version control matters for the plugin (Workshop keeps addon history, not plugin history).
- Layout deviation: non-Enfusion code is in `plugin\` (not `src\`); research in `docs\research\`.

## Design decisions (settled)
- Trust model like TFAR: client-trusted, plugins decide audibility; transmissions announced plugin-to-plugin
  via TS `sendPluginCommand`.
- Bridge = files in `$profile:` (~10 ms); RestApi rejected (~100 ms batching). See Wiki/external/teamspeak-bridge.md.
- Vanilla VON capture disabled (modded `SCR_VONController.ActivateVON` minus `SetCapture(true)`).
- Prefer vanilla radio items (`BaseRadioComponent`/`RadioTransceiver`). No 506IRRU compatibility. No persistent client settings.
- User dislikes `ShowCustomHint`; uses HUD (`LC_Hud`). Soft-compatible with Cupcake's Stance Indicator.
- Wishlist not started: EW, simplex/duplex, vehicle intercom, babble languages, relays/satcom, EMP.

## Pointers
- Docs: `docs\` (developer-reference, how-it-works, troubleshooting, ...). Research: `docs\research\RESEARCH_NOTES.md`.
- Third-party refs: `References\ARMods\{CoalitionVON-2.1.30,506IRRU-EnhancedRadio-1.0.38,CupcakesStanceIndicator}`,
  `References\Arma3\from-LimaCharlie\{ACRE2,TFAR}`, `References\GithubRepos\{acre2,TFAR,Coalition-VON,ts3client-pluginsdk}`.
- Untracked: `WorkshopCoverPicture.*` (decide whether to commit).
