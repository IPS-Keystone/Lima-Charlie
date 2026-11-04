# Changelog

## 2.0.6 — mod 2.0.6, plugin 2.0.6 (testing, unreleased)

Since 2.0.5. Bug fixes from the 3 November unit session. Bridge protocol unchanged (9).

- **Radio channels no longer swap places.** Closing the Game Master editor or dropping a radio used to
  reshuffle the radio menu and move the transmit keys onto other channels: the game removes a
  radio by moving the last one into its slot. Lima Charlie now puts the moved one back at the end, so the
  order stays as it was. A Game Master's editor channels now come after their own radios, so opening the
  editor no longer takes over TX 1-4.
- **Smoother when several people talk on the radio at once.** With OTAC Core loaded, an incoming radio
  speaker beyond the VON display's slots made OTAC's display code throw an error, and Lima Charlie raised it
  again up to 20 times a second until a slot freed. Each error wrote a full stack trace to three log files.
  It now happens once per overflow. The error itself is OTAC Core's to fix.
- **Less work in big buildings.** The path through a building's rooms was worked out again on every update,
  up to 20 times a second, across every doorway of the building. A Soviet apartment block has 117 rooms and
  over 300 doorways and windows. Paths are now kept per building and only worked out again when a door or
  window changes (checked four times a second), and the search only follows each room's own doorways.
  Someone outdoors near several mapped buildings no longer throws one building's answer away for the next.
  Up to 32 paths are kept (a town fight spread over more than 8 buildings used to work every one out again on
  every update), and each building's doors and windows are read once per check however many rooms paths
  start from.
- **Less work for people on the move.** Finding which building someone is in used to run on every update for
  anyone running or in a vehicle nearby. It now runs at most about 7 times a second per person, and asks each
  building once rather than once for every door and window of it.
- **Cheaper occlusion traces.** Each object a trace passes is measured first, and only something wide enough
  to be cover is checked further (tree, fence, person); posts and props are passed straight away. Same result.
- **A building with a different room model under the same name is left to tracing.** A copy of a building with
  its model swapped, or a destroyed building, could be read with another model's doorway numbers. It now
  falls back to traces, and is never used to map that building type.
- **Room mapping stops when it has covered the building.** It used to carry on probing above the roof until
  it hit its cap, whenever it could not find every doorway. That was a few extra seconds of mapping per
  building type.
- **No more hitching the first time you walk into a big building.** Mapping a building type's rooms cost a
  fixed amount of probing per update, and each probe costs more the more doorways the building has: 3.6 ms
  on every update for 15-30 seconds in a Soviet apartment block. It is now sized to the building, about 1 ms
  per update, so a big building takes longer to map (a few minutes of standing in it) without hitching.
  It also no longer gives up halfway along the apartment block with 87 of its doorways unfound.
- **Mapped buildings are remembered.** Room maps are saved to `profile/LimaCharlie/rooms/`, one small file
  per building type, including how far an unfinished one got. Each building type is mapped once per
  player and game version instead of every session. Deleting the folder just means mapping again.
- **Cheaper game state updates.** Each nearby player's entry is built on its own and added once, which made
  building the update up to 30% faster with 60 players nearby. Each radio's id and duplex mode are read once
  instead of several times per update and on every frame a transmit key is held.
- **Room diagnostic log is cheaper.** With `roomDiagnosticLog` on, measuring how solid a blocker is (the
  `s0.42` figure) walked every collider of the blocking object, often a whole building, on every occlusion
  check. It is now measured only for the line about to be printed.
- **The transmit key picker hands you back to the radio menu, and takes keys.** With the picker open, `1` to
  `4` choose that transmit key and `Esc` chooses None, the same as the buttons. Either way the radio menu
  reopens instead of leaving you in the game, so you can set the next channel straight away.
- **VON display, folded in from OTAC Core (with their approval).** Named channels are drawn on a background
  of their colour with white or black text to suit, instead of coloured text. A widget reused for an unnamed
  channel gets the game's own colours back (it used to keep the last name's colour). Two new server settings,
  both on by default: `hideRadioRoles` hides the group role and squad leader icon on incoming radio, and
  `hideDirectSpeechDisplay` keeps people talking out loud off the VON display. OTAC's PLATOON / AIR /
  COMMAND nets are not built in; set them in `channelLabels`, e.g. `"50,#FF0000,PLATOON;52,#49B3FF,AIR;60,#FFE02F,COMMAND"`.
- **Channel labels can be any colour.** Alongside the named colours, `server.json` takes `#RRGGBB`
  (`"52,#49B3FF,AIR"`), and the mod config has a `CUSTOM` colour with a colour picker. Text on a label is
  now white or black by the colour's brightness, so a custom colour stays readable; the named colours keep
  the split they had.
- **The Lima Charlie status notice shows once, after you spawn in.** It used to appear when the server's
  session arrived (often still in the deploy menu) and again every time something changed, including each
  time you muted or unmuted in TeamSpeak. It now shows once per connection, shortly after you first spawn (or
  open the Game Master editor), and says whether your microphone is on or muted. It also takes itself down
  after 12 seconds: in the Game Master editor the game makes every hint stay until closed, which is why it
  sometimes would not go away.
- **Diagnostics are now three levels, with the cheap one on by default.** `diagnostics` (server.json) or
  `m_eDiagnostics` (mod config) is `OFF`, `REGULAR` or `COMPREHENSIVE`. **REGULAR** (default) logs a line when
  something happens - a transmission starting and ending, a radio transmission heard and how bad it got,
  the radio list changing, entering or leaving a building (with how much of its room model is mapped), the
  plugin stopping or coming back, a transmit key lockout - and a one line summary a minute (updates written,
  plugin states read, nearby players, muffling by rooms or trace, traces, room probes, transmissions,
  receptions). It reads only what the update already worked out, so a player's log after a problem is
  useful without asking them to play it again. **COMPREHENSIVE** is the old `diagnosticLog`: both sides of
  the bridge once a second, for deliberate tests. `roomDiagnosticLog` stays its own switch; its per-player
  part is now built only for the line it prints. An old `"diagnosticLog": true` still works (as
  COMPREHENSIVE); `false`, which every generated server.json has, no longer turns anything off. The plugin
  state line now also says when TeamSpeak mutes your microphone.
- **A leftover plugin file no longer counts as the plugin running.** With TeamSpeak closed, the
  `plugin_state.json` from an earlier session was read once and counted as a live plugin for three seconds,
  so the status notice could say "connected". The plugin now has to be seen writing twice.
- **Voices behind walls no longer pump.** When people shuffle in a room, a voice can flip between clear and
  one wall behind every second or so; that was heard as a 5-8 dB step and the top end cutting in and out.
  The muffle and room echo now glide to the new value over about 200 ms.

## 2.0.5 — mod 2.0.5, plugin 2.0.5 (testing, unreleased)

Since 2.0.4.

- **A hearing hook for other mods.** An ear protection headset or earplugs mod can now change how you hear
  Lima Charlie voices, through a small compat addon that depends on both: `LC_Hearing.SetLocalProfile`
  takes a gain, a high-pass, a low-pass and a drive for nearby voices and the same for radio voices. Nearby
  voices are shaped after walls and distance and before the room's echo, so the echo comes through the
  headset too; radio beeps are never touched. Neither mod needs the other. An older plugin ignores it, so
  the bridge protocol is unchanged (9). See docs/developer-reference.md, "Hearing hook for other mods".
- **A new terrain model, much kinder to long range radios.** Each hill rising above the line between two
  radios now takes a share of the radio's range that grows with its height (a 10 m hill 8%, 30 m 22%,
  120 m 57%, 250 m 74%), and a second hill takes its share of what the first left, up to 90% in all. The
  same hill costs a 20 km set the same share as a 2 km one. TFAR's model, used until now, counted a hill
  near either end of the path as many times its real height and grew the penalty with distance, so a
  20 km radio behind a 10 m ridge garbled from 3.5 km and was dead at 9.7 km; now it is clean to 6.4 km and
  reaches 18.4 km. The server's terrain effect setting scales every hill's height as before.
- **Radios are louder.** At full channel volume a radio voice is now about 18% louder than someone talking
  right beside you; it was half their level, because the radio band throws away the low end of a voice.
  Channels now start at 80% rather than 100%, a little under the person beside you. A shout on the net is
  rounded off by a limiter instead of clipping.

## 2.0.4 — mod 2.0.4, plugin 2.0.4

Since 2.0.3.

- **Radio keys open your microphone in TeamSpeak.** Holding any radio transmit key forces TeamSpeak's input
  on and switches voice activation detection off for the length of the transmission, so a push-to-talk user
  does not need to hold TeamSpeak's key as well and voice activation never clips a quiet word on the net.
  This is what TFAR does, with one difference: the settings are saved before the transmission and restored
  exactly afterwards, rather than inferred. TFAR deactivates input on release whenever voice activation was
  off, which switches off the microphone of anyone on continuous transmission — the bug 1.0.6 shipped. The
  release follows the transmission's own third-of-a-second tail, so the last word goes out. A mute set in
  TeamSpeak is a separate setting and is left alone.
- **A transmit key picker.** `J` over a radio in the VON menu now opens a small menu beside it with five
  buttons — **None** and **1** to **4** — modelled on the Coalition squad interface's colour team picker.
  The key the radio is on now is lit. Choosing a key moves it from whatever radio had it; **None** takes the
  radio off every key, including the one its position in the list would give it, which was not possible
  before. Esc closes it.

## 2.0.3 — mod 2.0.3, plugin 2.0.3

Since 2.0.1. Plugin change; the mod is bumped only so both halves report the same number.

- **Radio reception now logs its own levels**, once a second per talker and only while they are coming in
  on a radio, to the TeamSpeak log:

  ```
  Radio rx player 3: in -24.1 dB, direct gain 0.03/0.02 out -55.0 dB, radio gain 0.80/0.00 quality 1.00 out -21.7 dB
  ```

  `in` is the voice as TeamSpeak handed it over, before anything of ours; the direct and radio figures are
  what each path made of it. This is here to settle a report that transmissions from someone at the edge of
  earshot came through quieter than transmissions from further away, which the mix as written cannot
  produce: the radio gain has no distance term, and every per-talker difference between those two cases is
  on the direct path. Two readings from a test — one speaker at the edge, one out of earshot — say whether the
  difference is in what arrives, in the radio path, or in neither.
- **`server.json` always says what it did**: one log line a session, whether the file was generated, read and
  overriding (naming the keys), read and overriding nothing, or found and unreadable. Two of those four
  used to be silent. (Added to this entry afterwards; it shipped in 2.0.3 without a line here.)
- Local voice is unchanged under the radio. Someone close enough to hear in person is heard both in the room
  and on the radio, as TFAR does. (A development build that removed the room voice during transmissions was
  never released.)

## 2.0.1 — mod 2.0.1, plugin 2.0.1

Since 2.0.0. The change is in the mod; the plugin is rebuilt only so both halves report the same number.

- **Every session now logs one line about `server.json`**, saying which of four things happened: it was
  generated, it was read and overrides something, it was read and overrides nothing, or it was found and
  could not be parsed. Two of those used to be silent — a file whose keys all matched the mod's config
  logged nothing at all, which looked exactly like a file being ignored.

```
[LC] No $profile:LimaCharlie/server.json found, so it has been generated from this session's settings; edit it to override the mod's config
[LC] Read $profile:LimaCharlie/server.json; it overrides: cleanRangePercent channelLabels
[LC] Read $profile:LimaCharlie/server.json; none of its keys override the mod's config
[LC] Found $profile:LimaCharlie/server.json but could not parse it, so the mod's config stands this session
```

## 2.0.0 — mod 2.0.0, plugin 2.0.0

**The first stable release since 1.0.1**, and the one to install. It gathers everything tested through the
1.0.2–1.0.14 development builds; those sections are kept below as the detail of how it got here.

**Update both halves.** The bridge protocol went from 6 to 9, and the plugin ignores state it does not
recognise, so an older plugin leaves radio and positional audio dead while TeamSpeak still looks connected.
The join notice prints both versions side by side; if either line does not read 2.0.0, that is why.

Two things every player has to do that they did not before:

- **TeamSpeak's sound pack must be on, with its volume up** (Options → Notifications). Radio beeps are
  handed to the same player that makes TeamSpeak's join and leave noises, so *Sounds deactivated* means no
  beeps and no interface tones. Voices are unaffected.
- **No in-game key is needed to speak.** TeamSpeak's own voice activation or push-to-talk governs your
  microphone; the mod no longer touches it. The transmit keys are for radios only, and still ship unbound.

The **Vanilla** beep set is selectable but not in the download: it is built from Bohemia's own samples, which
carry no licence covering redistribution, so the package ships without them and `tools/make_vanilla_beeps.py`
rebuilds the set from your own game install if you want it.

For server operators: `$profile:LimaCharlie/server.json` now holds every setting and has the last word over
the mod's config and the mission header. Delete an existing one to pick up the new keys. If your group also
runs Coalition VON, keep the Lima Charlie channel name distinct from any channel CVON has used.

The headline changes, all detailed below: occlusion decided from the engine's room model before any ray,
with open doorways carrying sound the way vanilla does; room reverb sized from the room; vanilla's roger
beep as a beep set, plus one beep volume for every channel; unconscious players silent unless the server
says otherwise; a join notice that tells a player whether the whole chain is working; and the TeamSpeak
channel held while you are alt-tabbed.

## 1.0.14 — mod 1.0.14, plugin 1.0.14

Since 1.0.13. The change is in the mod; the plugin is rebuilt only so both halves report the same number.

- **One open doorway between you no longer muffles anything.** Standing to the side of an open door, with
  someone out in front of it, was muffled because the room model charged a fifth of a muffle for passing
  through the doorway and the trace found the wall beside you. An open portal within 15 m is now treated as
  no obstruction at all, and no trace is taken, so it comes out clear — which is how vanilla sounds, and for
  the same reason: its occlusion is gated on how enclosed the listener is, and a room standing open is not
  enclosed.
- A part-open door, a closed one, or anything further than 15 m is unchanged: the path is an upper bound and
  the trace still decides, which is what keeps building corners and alleyways slightly muffled.

### Keybinds and the radio menu

- **The beep volume key now appears in the keybind menu, and binds.** Two faults: its default was `;`,
  which the input system would not take, and an action whose default it cannot resolve is an action that
  does not exist, so the entry had nothing to show. The default is now `O`. The entry also needs
  `m_sPreset`, which is what the menu attaches a new binding to — without it the row draws but refuses
  every key.
- Every keybind name is shortened to the length the menu is built for — vanilla's own field documents 15
  characters and ours ran to 48 — and the four transmit keys are marked as continuous, as vanilla marks its
  own hold actions.
- **The radio menu keys now appear in the game's control hint panel**, the one that prompts you to open a
  door, whenever the menu is open. Ear, beeps, channel volume, beep volume, transmit key and type frequency.
- **The beep volume is shown on every radio entry**, not only once it has been turned down. A setting that
  only appears after you change it is a setting nobody finds.
- **The type-frequency box has moved off the middle of the radial menu** to the right of it, so it no longer
  covers the channels you are picking between.

## 1.0.13 — mod 1.0.13, plugin 1.0.13

Since 1.0.12. Protocol is unchanged at 9, so this is a plugin-only change.

- **Beeps are played by TeamSpeak now, not mixed into its playback buffer.** That buffer is shared with every
  plugin the client has loaded and handed to each in turn, so a plugin that overwrites it rather than adding
  to it destroys whatever we put there — which is what Coalition VON was doing to a tester's beeps. TeamSpeak's
  own player is out of reach of all of them.
- **The ear and the volume survive the move.** The API that plays a file takes a path and nothing else, so a
  copy of the clip with the panning and volume already applied is rendered to `sounds/beepcache/` the first
  time each combination is needed, and reused after. Volume is quantised to twenty steps to keep the number of
  files down, and the cache is emptied on load so a changed beep cannot leave a stale rendering behind.
- TeamSpeak mixes these itself, so its own sound volume in Options → Notifications now applies to our beeps
  as well as ours.
- The old path is kept as a fallback for installs where the cache cannot be written; the log says when it is
  used.

## 1.0.12 — mod 1.0.12, plugin 1.0.12

Since 1.0.11. Protocol is unchanged at 9, so this is a plugin-only fix.

- **Your own transmission beeps are back.** The beep volume added in 1.0.11 was applied through a function
  defined further down the file than the two places that called it for the local start and end beeps. C
  accepts that, guesses the function returns an int, and passes the arguments wrongly, so both of your own
  beeps played at a nonsense volume — in practice, silence. The beeps for other people's transmissions were
  below the definition and were never affected.
- The build now treats a call to an undeclared function as an error rather than a warning, which is what let
  this through.

## 1.0.11 — mod 1.0.11, plugin 1.0.11

Since 1.0.10.

**Update the plugin.** The bridge protocol went from 8 to 9, and the plugin ignores state it does not
recognise, so an older plugin leaves radio and positional audio dead while TeamSpeak still looks connected.
The join notice prints both versions side by side; if the plugin line does not read 1.0.11, that is why.

### Beeps

- **The game's own roger beep is now a beep set**, alongside the TFAR and ACRE ones. It is what vanilla
  plays at the end of a radio transmission, mixed the way the game mixes it, so it has no start beep —
  vanilla has none.
- **One beep volume for every channel**, on `;` by default, cycling down in the same ten steps as channel
  volume and wrapping from silent back to full. It multiplies each channel's own volume rather than
  replacing it, so a quiet channel stays quiet, and a channel turned off is silent either way. Below full
  volume the figure shows beside the beep set on each radio's entry.
- Changing either volume with a channel under the cursor plays that channel's beep at the new level.
- Interface tones are not beeps and keep their own level.
- Three things in the beep mixer that could tear the audio: a ninth simultaneous beep replaced whichever
  sound happened to be in the first slot, which could cut one off at full amplitude (it now takes the one
  nearest its end); clips were not faded at the tail, so anything truncated or ending abruptly clicked; and
  unloading the sounds waited a fixed 50 ms for the audio thread rather than actually waiting for it, which
  could free a clip out from under the mixer.

### The channel stops flapping

- **Alt-tabbing no longer moves you out of the TeamSpeak channel.** Reforger stops writing its state while it
  is not the window you are using, and three seconds of that was read as having left the game: you were moved
  back to your old channel, then moved in again when you returned, and everyone else in the channel heard a
  join and a leave each time. The plugin now keeps you where you are until the game actually says it is
  leaving, which it does the instant you quit to the menu or disconnect.
- Your radio transmission still ends after three seconds of silence from the game, so a frozen game cannot
  hold your transmit key open on the net.
- A crash cannot say goodbye, so a crashed game keeps you in the game channel for five minutes before the
  plugin gives up on it. The TeamSpeak log distinguishes the two: `Game state paused; holding the channel`
  when the game goes quiet, `Left game` when it is written off.

### Unconscious players

- **`unconsciousCanSpeak`**, off by default, lets a server give unconscious players their voice back, so
  someone bleeding out can still talk to the medic. Radios stay out of reach while unconscious either way,
  which is vanilla's own rule, and the dead are always silent.

## 1.0.10 — mod 1.0.10, plugin 1.0.10

Since the published mod 1.0.1 (plugin 1.0.0/1.0.1).

**Update the plugin.** The bridge protocol went from 6 to 8, and the plugin ignores state it does not
recognise, so an older plugin leaves radio and positional audio dead while TeamSpeak still looks connected.
The new join notice prints both versions side by side; if the plugin line does not read 1.0.10, that is why.

### Muffling now understands rooms

- Occlusion asks the engine's room model before casting a single ray. Two people in the same room hear each
  other clearly; in the same building, lightly muffled; the cost of the path between rooms is worked out
  from the doors and portals between them.
- Open doors muffle far less than closed ones, and the state is read live.
- A clear line of sight always wins. Standing a metre apart in an open doorway is no longer muffled.
- Trees never muffle a voice. Neither do powerlines, power poles, small debris, other players, or
  see-through fences and railings (ten prefab families: net, metal, pole, grave and game-proof fences, pipe
  and metal railings, barbed tape, coil and wire).
- Anything narrower than 80 cm is filtered out of the trace by the engine itself, so lamp posts, signs and
  posts stop counting as cover rather than being traced through one at a time.
- Open and thin vehicles no longer blanket-muffle their occupants. Aiming a mortar or sitting on a truck bed
  is treated like standing in the open, because that is what the room model and the sight line say it is.
- Sight lines are rechecked more often: every 50 ms while someone is talking, 250 ms while they are silent.

### Room reverb

- Voices heard indoors now carry reverb sized from the room's actual volume. Nothing below 25 m³, peaking
  around 600 m³, fading out by 6000 m³ so hangars and open halls do not ring.
- A voice from outside heard through a door does not pick up the listener's room.
- Overall strength cut by about 60% after the first round of testing.

### TeamSpeak's microphone, not ours

- The mod no longer touches your capture settings. TeamSpeak's own voice activation or push-to-talk governs
  when you transmit; there is no separate in-game toggle to remember.
- A build before 1.0.7 could leave the microphone deactivated in TeamSpeak after a session. Unmute once and
  it will not come back.
- Unconscious players cannot transmit or speak. Dead players cannot hear. The server checks this itself.

### It tells you when it is working

- On spawning in, a hint lists the mod version, the plugin version, whether TeamSpeak is connected, the
  channel, how many people are in it, and whether your microphone is muted in TeamSpeak. It stays silent
  when everything is in order, and reappears if something changes.
- If no session arrives from the server within 15 seconds, it says so rather than leaving you guessing.

### server.json

- Every setting in the mod's config is now also a key in `$profile:LimaCharlie/server.json`, written out in
  full the first time a session runs without one, one setting per line.
- Precedence: built-in defaults, then the mod's config, then the scenario's mission header, then
  `server.json`, which has the last word because it is the only one an operator can edit without
  republishing the mod.
- Only the keys present in the file override anything, and the ones that took effect are named in the log as
  `server.json overrides: ...`.
- The TeamSpeak channel defaults to `LimaCharlie`; the server IP setting is gone.
- Named nets: `channelLabels` takes `"45.5,RED,COMMAND;38,GREEN,MEDEVAC"`, and `channelNaming` takes
  `LC_ONLY`, `HYBRID` or `VANILLA_ONLY`.
- Two troubleshooting switches, `diagnosticLog` and `roomDiagnosticLog`, both off by default.

If you already have a `server.json`, delete it to get the new keys, or add them by hand.

### Fixes

- **Chopped, stuttering speech for everyone in earshot.** The reverb and sound mixers zeroed playback
  channels TeamSpeak had not marked as filled, which in the mixed buffer wiped out every other voice. Both
  now only add.
- **Every talker after the 512th went silent.** Per-talker audio state used a fixed table that was never
  freed, so after 512 distinct TeamSpeak clients in one run, new talkers had no audio until TeamSpeak was
  restarted. It is cleared on leaving a game or changing server.
- A Game Master with the editor open logged the whole game state every second regardless of settings —
  around 11 MB an hour on a full server. It now needs the diagnostic setting like everything else.
- Rooms with no path between them are traced rather than silenced.
- Room distances could leak between buildings with different numbers of areas.
- A radio dropped mid-transmission could leave a null transceiver behind.
- Denormal floats in the reverb's feedback, and a stale reverb send after an early return.
- A null input manager in the frequency input.

### Performance

Prompted by a report of 60 fps dropping to 15 with 30 players in a small space. No unbounded growth turned
up in either half, but plenty of avoidable work did:

- Occlusion used to trace every player within 60 m every 100 ms in a single frame — 58 rays at once with 30
  players. Results are cached per player, retraced only when due and only if either end moved 15 cm, and at
  most 12 players per write.
- The path between rooms is solved once per update for all listeners, instead of once per pair.
- The VON display feed and terrain links ran every frame; they now run when the plugin state changes, capped
  at 20 Hz.
- The plugin polls at 5 ms in game and 50 ms idle, and only raises the system timer resolution while in a
  game.
- The microphone's mute state is read four times a second instead of two hundred.

### Documentation

New server setup guide, and the player guide, settings reference, installation and troubleshooting pages all
brought up to date.
