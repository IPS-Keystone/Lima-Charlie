# Changelog

## 2.0.2 — mod 2.0.2, plugin 2.0.2

Since 2.0.1. Plugin change; the mod is bumped only so both halves report the same number.

- **Radio transmissions play at the same loudness whatever the speaker's distance.** The radio voice itself
  never varied, but a speaker close enough to hear in person also had their direct voice playing on top at
  full gain, while one at the edge of earshot added almost nothing. Someone beside you keying up came
  through several dB louder than someone further off, which read as distant transmissions being quiet. Their
  direct voice is now ducked to a fifth while you are receiving them on a radio: still present in the room,
  never what sets the level.

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
