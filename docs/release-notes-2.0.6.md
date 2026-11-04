# Lima Charlie 2.0.6 — changes since 2.0.5

Mod 2.0.6 and TeamSpeak plugin 2.0.6. Bug fixes and performance work from the 3 November unit session.
The bridge protocol is still 9, so a 2.0.5 plugin keeps working with the 2.0.6 mod. Update **both** anyway:
without the 2.0.6 plugin, voices behind walls still pump (see below). Full detail is in `CHANGELOG.md`.

## Bug fixes
- **Radio channels stay in order.** Closing the Game Master editor or dropping a radio used to shuffle the
  radio menu and move your transmit keys onto other channels. The order now stays as it was. A Game
  Master's editor channels come after their own radios, so opening the editor no longer takes TX 1-4.
- **Voices behind walls no longer pump.** As people moved around a room, a voice could flip between clear
  and one wall behind every second or so. That was heard as a 5-8 dB step with the top end cutting in and
  out. The muffle and room echo now glide to the new value over about 200 ms. *(plugin)*
- **No error spam with several people on the net at once (with OTAC Core).** When more radio speakers came
  in than the VON display has slots, an error from OTAC's display code was raised up to 20 times a second,
  each one writing a stack trace to three logs. It now happens once per overflow. The error itself is
  OTAC Core's to fix.
- **Unnamed channels get the game's own colours back** on the VON display. A reused label used to keep the
  last named channel's colour.
- **The status notice takes itself down after 12 seconds.** In the Game Master editor it used to stay until
  closed.
- **No false "connected" in the status notice.** With TeamSpeak closed, a `plugin_state.json` left over
  from an earlier session counted as a running plugin. The plugin now has to be seen writing twice.
- **Big buildings are mapped in full.** Mapping the Soviet apartment block used to stop halfway along it,
  with 87 of its doorways unfound.
- **A building with a swapped room model is left to traces.** A copy of a building with another model, or a
  destroyed one, could be read with the wrong doorway numbers. It now falls back to traces and is never
  used to map that building type.

## Performance improvements
All with the same results as before.
- **No more hitching the first time you walk into a big building.** Mapping a Soviet apartment block's
  rooms cost 3.6 ms on every update for 15-30 seconds. Mapping now takes about 1 ms per update, so a big
  building takes longer to map (a few minutes of standing in it) without hitching.
- **Mapped buildings are remembered.** Room maps are saved to `profile/LimaCharlie/rooms/`, one small file
  per building type, including how far an unfinished one got. Each building type is mapped once per player
  and game version, not every session. Deleting the folder just means mapping again.
- **Room mapping stops when the building is covered.** It used to keep probing above the roof until it hit
  its cap, a few extra seconds of mapping per building type.
- **Paths through a building's rooms are kept.** They used to be worked out up to 20 times a second across
  every doorway of the building (the apartment block has 117 rooms and over 300 doorways and windows).
  They are now worked out again only when a door or window changes, checked four times a second, and the
  search follows only each room's own doorways. Up to 32 paths are kept, so a fight spread over many
  buildings no longer throws them away.
- **Finding which building someone is in** runs at most about 7 times a second per person (it ran on every
  update for anyone running or in a vehicle), and asks each building once rather than once per doorway.
- **Cheaper occlusion traces.** Only objects wide enough to be cover (trees, fences, people) are checked
  further; posts and props are passed straight away.
- **Cheaper updates to the plugin.** Each update is built up to 30% faster with 60 players nearby. Radio id
  and duplex mode are read once per update, not several times and on every frame a transmit key is held.
- **Cheaper room diagnostic log.** With `roomDiagnosticLog` on, how solid a blocker is (the `s0.42` figure)
  is now measured only for the line about to be printed, not on every occlusion check.

## Changes
**Radios and the VON display**
- **The transmit key picker takes keys and hands you back.** With the picker open, `1` to `4` choose that
  key and `Esc` chooses None. Either way the radio menu reopens, so you can set the next channel straight
  away.
- **VON display tweaks folded in from OTAC Core** (with their approval). Named channels are drawn on a
  background of their colour, with white or black text to suit.
- **New server setting `hideRadioRoles`** (on by default). Incoming radio on the VON display shows the
  speaker's name and channel only, no group role or squad leader icon.
- **New server setting `hideDirectSpeechDisplay`** (on by default). The VON display lists radio
  transmissions only, not people talking out loud nearby. The game's own server setting for this still
  applies when this is off.
- **Channel labels can be any colour.** `channelLabels` takes `#RRGGBB` alongside the named colours, e.g.
  `"52,#49B3FF,AIR"`. The mod config has a `CUSTOM` colour with a colour picker. Label text is white or
  black by the colour's brightness; the named colours keep the split they had.
- **OTAC's nets are not built in.** To get OTAC Core's PLATOON / AIR / COMMAND labels, set
  `"channelLabels": "50,#FF0000,PLATOON;52,#49B3FF,AIR;60,#FFE02F,COMMAND"`.

**Status notice**
- **Shows once, after you spawn.** It used to appear while you were still in the deploy menu, and again on
  every change, including each TeamSpeak mute or unmute. It now shows once per connection, shortly after
  you first spawn (or open the Game Master editor), and says whether your microphone is on or muted.

**Diagnostics**
- **`diagnostics` replaces `diagnosticLog`, with three levels:** `OFF`, `REGULAR` (default) and
  `COMPREHENSIVE`. In the mod config it is `m_eDiagnostics`.
  - **REGULAR** is cheap enough to leave on, so a player's log after a problem is useful without asking
    them to play it again. It logs a line when something happens: a transmission starting and ending, a
    radio transmission heard and how bad it got, the radio list changing, entering or leaving a building
    (with how much of its room model is mapped), the plugin stopping or coming back, a transmit key
    lockout. Once a minute it logs a summary, `[LC] summary 60 s: writes ..., plugin states ..., ...`:
    updates written, plugin states read, nearby players, muffling by rooms or by trace, traces, room
    probes, transmissions and receptions.
  - **COMPREHENSIVE** is the old `diagnosticLog`: both sides of the bridge once a second, for deliberate
    tests.
  - An old `"diagnosticLog": true` still works, as COMPREHENSIVE. `false`, which every generated
    `server.json` has, no longer turns anything off.
- **The plugin state line says when TeamSpeak mutes your microphone**:
  `[LC] Plugin ...: TeamSpeak connected=..., ..., mic muted=...`.

## Known open items
- OTAC Core still has its own `SCR_VonDisplay.UpdateTransmission` override. With both mods loaded, both
  colour the label and the last one loaded wins, so OTAC should drop theirs.
- Not yet checked in a live session: picker keys and reopen, the notice taking itself down in the GM
  editor, label look, role and direct speech hiding, and the regular log lines with TeamSpeak and a second
  player.
