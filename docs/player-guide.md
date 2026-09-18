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
facing, and is attenuated when you are behind them. Walls and vehicles muffle it — see
[how-it-works.md](how-it-works.md).

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
