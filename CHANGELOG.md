# Changelog

## 1.0.11 — mod 1.0.11, plugin 1.0.11

Since the published mod 1.0.1 (plugin 1.0.0/1.0.1).

**Update the plugin.** The bridge protocol went from 6 to 8, and the plugin ignores state it does not
recognise, so an older plugin leaves radio and positional audio dead while TeamSpeak still looks connected.
The new join notice prints both versions side by side; if the plugin line does not read 1.0.11, that is why.

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

### TeamSpeak's microphone, not ours

- The mod no longer touches your capture settings. TeamSpeak's own voice activation or push-to-talk governs
  when you transmit; there is no separate in-game toggle to remember.
- A build before 1.0.7 could leave the microphone deactivated in TeamSpeak after a session. Unmute once and
  it will not come back.
- Unconscious players cannot transmit or speak. Dead players cannot hear. The server checks this itself.
- **`unconsciousCanSpeak`**, off by default, lets a server give unconscious players their voice back, so
  someone bleeding out can still talk to the medic. Radios stay out of reach while unconscious either way,
  which is vanilla's own rule, and the dead are always silent.

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
