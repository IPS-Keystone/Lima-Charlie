# Lima Charlie 2.0.5 — changes since 2.0.1

Mod 2.0.5 and TeamSpeak plugin 2.0.5. Update **both**: the bridge protocol is still 9, so a 2.0.1–2.0.4
plugin keeps working with the 2.0.5 mod, but it plays radios at the old level, judges terrain with the old
formula and ignores ear protection mods. (2.0.2 was a test build and was never released.)

## For players

**Radios**
- **Radios are louder.** At full channel volume a radio voice is now about 18% louder than someone
  talking right beside you. Before, it was about half their level, because the radio band strips out the
  low end of a voice. *(2.0.5)*
- **Channels start at 80% volume** instead of 100%, a little under the person beside you, so there is room
  to turn a radio up. *(2.0.5)*
- **A shout on the net no longer clips.** Loud speech on a radio is rounded off by a limiter. *(2.0.5)*
- **Radio keys open your TeamSpeak microphone.** Holding any radio transmit key turns TeamSpeak's input on
  and voice activation off for the length of the transmission, then puts both back exactly as they were.
  Push-to-talk users need one key, not two, and voice activation can no longer clip a quiet word. The
  last word still goes out: the release waits for the transmission's short tail. A mute set in TeamSpeak
  is left alone. *(2.0.4)*
- **A transmit key picker.** Press `J` over a radio in the VON menu to open a small menu with **None** and
  **1** to **4**. The key the radio is on now is lit. Picking a key moves it from whichever radio had it;
  **None** takes the radio off every key, which was not possible before. `Esc` closes it. *(2.0.4)*
- **Someone close enough to hear in person is heard both ways while they transmit**: in the room and on
  the radio, as TFAR does. *(2.0.3)*

**Terrain** *(2.0.5)*
- **Hills are much kinder to long range radios.** Each hill that rises above the line between two radios
  now takes a share of the radio's range, growing with its height. A second hill takes its share of what
  the first left, and all of them together never take more than 90%:

  | Hills between you | Range lost | 20 km radio: clear to / heard to |
  | --- | --- | --- |
  | none | 0% | 7.0 / 20.0 km |
  | 10 m | 8% | 6.4 / 18.4 km |
  | 30 m | 22% | 5.5 / 15.7 km |
  | 60 m | 37% | 4.4 / 12.5 km |
  | 120 m | 57% | 3.0 / 8.5 km |
  | 250 m | 74% | 1.8 / 5.2 km |
  | 30 m + 60 m | 51% | 3.4 / 9.8 km |

  Under the old model, TFAR's, a 20 km radio behind a 10 m ridge broke up from 3.5 km and was gone at
  9.7 km. A hill near either end of the path counted as many times its real height, and the penalty grew
  with distance.
- The same hill now costs a short range set the same share of its range as a long range one, so a long
  range set behind a ridge still reaches far.

## For server operators
- **The terrain effect setting** (`terrainEffectPercent` in `server.json`, `m_fTerrainEffectPercent` in
  Workbench) still runs 0–300%.
  It now scales every hill's height: at 200% a 30 m hill counts as 60 m. Expect noticeably longer radio
  range on hilly maps at the same setting. *(2.0.5)*
- **`server.json` always says what it did.** Each session logs one line: the file was generated, read and
  overriding (naming the keys), read and overriding nothing, or found and unreadable. The last two used to
  be silent. *(2.0.3)*

## For modders
- **A hearing hook for ear protection mods.** `LC_Hearing.SetLocalProfile(source, profile)` lets another
  mod change how the local player hears Lima Charlie voices. A profile has gain, high-pass, low-pass and
  drive for nearby voices, and the same four for radio voices. It is called from a small compat addon
  that depends on both mods, so neither mod needs the other.
  - Nearby voices are shaped after walls and distance and before the room's echo, so the echo comes
    through the headset too. Radio beeps and UI sounds are never touched.
  - Each mod reports under its own key, and a key not refreshed within a second is dropped.
  - Contract, limits and logging: `docs/developer-reference.md`, "Hearing hook for other mods". *(2.0.5)*

## Troubleshooting additions
- **The TeamSpeak log shows radio levels** once a second per talker while they come in on a radio:
  `Radio rx player 3: in -24.1 dB, direct gain ... radio gain ... out ...`. *(2.0.3)*
- **The game and the plugin each log hearing changes** whenever another mod changes your hearing:
  `[LC] Hearing {...}` in the game log, `Hearing: direct ...; radio ...` in the TeamSpeak log. *(2.0.5)*
