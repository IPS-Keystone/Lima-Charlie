# Player guide

## Controls

| Action | Default | What it does |
| --- | --- | --- |
| Cycle voice level | `Ctrl` + `Tab` | Whisper → Normal → Shout → Whisper |
| Transmit on channel 1–4 | *unbound* | Hold to talk on the radio assigned to that key |
| Assign transmit key | `J` | Cycles the selected radio through keys 1–4 |
| Type frequency | `F` | Opens a box to type a frequency in MHz |
| Cycle ear | `T` | Both → left → right, per radio |
| Cycle beep set | `K` | Changes the transmission tones, per radio |
| Cycle volume | `L` | Steps down 10%, wrapping from 0% back to 100% |
| Adjust volume | `Ctrl` + scroll | On the hovered channel in the radio menu |

**The four transmit keys ship unbound on purpose.** Bind them in Options → Keybinds → Lima Charlie
before you can use a radio at all. Everything else has a default you can change there too.

`Ctrl` + scroll and `Ctrl` + `Tab` are hidden from the keybind menu; the rest are listed.

## The status notice

Shortly after you join, a panel tells you whether the whole chain is working, because none of these
failures make a sound on their own:

```
Lima Charlie
Mod loaded: 1.0.6
TeamSpeak plugin: 1.0.6
TeamSpeak: connected
Channel: LimaCharlie
Others with the plugin here: 3
```

If something is missing it says so instead, with what to do about it — the plugin not running, TeamSpeak
not connected to a server, or the channel move not having happened. It appears again if any of that changes
later, so starting TeamSpeak after the game tells you when it has been picked up.

`Others with the plugin here` counting 0 is normal if you are the first one in.

The notice is silent when everything is in place and makes a sound when it is not. It uses the game's hint
panel, falling back to a popup if you have hints turned off; with both turned off it only reaches
`console.log`.

## Voice levels

Direct speech carries as far as your voice level says, and nothing else changes with it:

| Level | Range |
| --- | --- |
| Whisper | 5 m |
| Normal | 20 m |
| Shout | 60 m |

The icon in the bottom right shows the current level and fades after six seconds. It sits just above where
Cupcake's Stance Indicator draws, and works with or without that mod.

Your voice falls off to −30 dB at the edge of its range, pans left and right with where the listener is
facing, and is attenuated when you are behind them. Walls, hulls and closed doors muffle it — see
**Indoors** below and [how-it-works.md](how-it-works.md).

## Indoors

Buildings are understood room by room rather than as one lump.

- **Same room** — you hear each other clearly, whatever furniture is in the way.
- **Within a few metres** — what you can see decides it, so standing in a doorway together is clear.
- **If you can see each other, you are not muffled** — through an open door, a hangar opening, or across
  a room the game happens to treat as two.
- **Trees never muffle you**, however thick the wood. Neither do other players, lamp posts, bollards or
  signs. Walls, fences, vehicles and terrain still do.
- **Next room** — muffled by whatever is between you. An **open door** carries voices far better than a
  closed one, and it does not need line of sight: shouting round a corner through an open doorway works.
  A closed door is worth about as much as the wall beside it.
- **Broken windows** let sound through. Intact ones do not.
- **Inside to outside** — the same rules, through whichever doors and windows are open.

Speech heard indoors also picks up the room's **reverb**, sized by the room: a small office barely rings,
a hall rings most. Very large open spaces — hangars, warehouses — do not ring at all, because their doors
are most of their walls. Voices from elsewhere in the building ring a room far less, and a voice
from outside barely at all, since it reaches you through a doorway rather than filling the room. Outdoors
there is none, and radio never reverberates — only speech in the air does.

## Vehicles

Sitting in a vehicle is not treated specially any more. What matters is whether something is actually
between the two of you:

- **Same vehicle** — clear, always.
- **Enclosed hull between you** — muffled, like a wall.
- **Open mount, open bed, open-top vehicle, turned out of a hatch** — not muffled at all. Aiming a mortar
  no longer muffles your voice, which it used to.

A car with its windows up will muffle, because the glass really is in the way.

## Radios

Each radio you carry appears in the vanilla VON radial menu, one entry per transceiver. The extra line on
each entry shows its transmit key and beep set.

Per radio, kept for the session only — nothing is saved between sessions:

- **Ear** — both, left only, right only, so you can put two nets in different ears.
- **Beep set** — TFAR SW, TFAR LR, TFAR AB, TFAR Classic, ACRE, or none. Defaults to TFAR SW for short
  range radios and TFAR LR for long range ones.
- **Volume** — ten steps.
- **Transmit key** — which of the four keys sends on it. Unassigned radios default to their position in
  the list.

### What a transmission sounds like

Perfectly clear out to a share of the radio's range (35% by default), then garbling progressively worse
out to the edge. The range itself never changes; only where the degradation starts.

Degradation is one dial, and everything moves together with it: a 300–3400 Hz radio band, rising
distortion, sample-and-hold artefacts, bit-crushing, occasional dropouts past the halfway mark, and hiss
that rises from a clean 30 dB signal-to-noise ratio down to −3 dB at the edge. The voice itself is held
level as the noise rises, so a bad signal sounds noisier, not louder.

Terrain between you and the transmitter counts as extra distance, so a ridge can garble or silence a
signal that distance alone would carry. This is TFAR's model.

### Transmission beeps

You hear a start and end beep on someone else's transmission only within 90% of the radio's range by
default. Past that the voice arrives with no beeps — so a beep means the signal is worth listening to.
Your own beeps always play when you key up and release.

### Half duplex

A radio can be set to half duplex on its prefab, in which case it goes deaf for as long as you hold its
transmit key. Every radio ships **full duplex**, so this does nothing until someone edits a prefab.

It is per radio, not per frequency: a second set on the same net keeps receiving while the first transmits.

## Channel names

Frequencies can carry a name and a colour, shown beside them on the VON display whenever anyone transmits
or receives. The server owns the list, so a name means the same thing to everybody.

Three modes, chosen by the server:

- **Lima Charlie only** — the server's named frequencies, nothing else. Unnamed frequencies show no name.
- **Hybrid** (default) — the server's name where there is one, otherwise the game's own labels for the
  platoon net, playable group callsigns and Conflict task nets.
- **Vanilla only** — the game's labels only; the server's list is ignored.

## Anti-spam

Keying a radio more than four times in four seconds locks radio transmission for two seconds and plays a
deny tone. Releasing and re-keying within 300 ms reads as one continuous transmission to everyone else, so
normal stutter on the key does not spam beeps.

## Being overheard

Enemy AI turn to look at players speaking out loud nearby. Radio traffic is never audible to AI, and
friendly AI ignore you either way. Walls shorten how far your voice carries for this — a shout still
reaches the street outside a building, a normal voice does not. The server can turn this off.

## What you hear when dead or unconscious

Unconscious, you still hear everything. Dead, you hear nothing. A Game Master with no character of their
own hears through the camera.
