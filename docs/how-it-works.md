# How it works

## The shape of it

```
Reforger client                    TeamSpeak client
+-------------------+              +--------------------------+
| LC_Client         |              | limacharlie_win64.dll    |
|   writes 20 Hz -> | game_state   |                          |
|                   | plugin_state | <- writes 4+ Hz          |
|   reads      <-   |              |                          |
+-------------------+              +--------------------------+
         ^                                      |
         | RPC: token + settings                | plugin commands
         |                                      v
   Reforger server                      other players' plugins
```

Enfusion cannot load a DLL, so there is no in-process bridge. Two JSON files in the Reforger profile
directory carry everything. An HTTP route through `RestContext` was measured first and rejected: the engine
batches request dispatch on a roughly 100 ms cadence, giving a flat ~104 ms round trip regardless of
payload. The file route runs at about 10 ms average, 13 ms at the 95th percentile.

The game never touches voice audio. TeamSpeak carries it, and the plugin edits each incoming stream before
the mix.

## Who decides what

The server decides almost nothing. It issues a session token and a settings object, and that is the whole
of its involvement in voice.

Everything audible is decided **on the listening client**, from data that speakers volunteer about
themselves. This is TFAR's trust model, and it has TFAR's consequence: a modified client can lie about
where it is or what it carries. Accepted deliberately — the alternative is routing voice through the game
server.

The session token is the one guard. Plugins ignore any peer reporting a different token, so two groups on
different Reforger servers can share a TeamSpeak channel.

## Direct speech

The speaker's plugin announces its voice range to the channel. Each listener's plugin computes gain per
speaker from its own copy of the positions:

- Falls off to **−30 dB** at the edge of range, with a fade over the last 15%.
- **Equal-power pan** from the listener's facing, width 0.85.
- **Behind attenuation** of 0.25, so someone at your back is quieter than someone in front.
- **Muffle** from occlusion: up to −9 dB plus a low-pass sliding from 8 kHz down to 700 Hz.
- **Room reverb** when the listener is indoors, sized by the room (see below).

Positions come from the listener's camera, not their body, so a Game Master or spectator hears from where
they are looking. The character's head is used only when the camera is close to it — within 20 m, which
covers first person and any third-person boom but not a free camera.

### Room reverb

Speech heard indoors gets the room's tail. The game sends the volume of the room the **listener** is
standing in, and the plugin sizes the reverb from it: nothing outdoors or in a space under 25 m³, then
growing with the logarithm of the volume, so a garage and a hangar differ clearly while a cupboard and a
bedroom do not.

| Room | Wet | Tail |
| --- | --- | --- |
| Outdoors, or under 25 m³ | none | — |
| About 150 m³ | 0.04 | short |
| About 600 m³ | 0.10 | longer |
| 3000 m³ | 0.05 | fading out |
| 6000 m³ and up | none | — |

The tail is fullest in an ordinary enclosed room and fades away towards both ends. A cupboard has nothing
to ring; a hangar or warehouse is an open structure whose doors are most of its wall, so it does not ring
like a sealed box either — and one tail stretched over a space that size sounds wrong even when it is shut.

Each voice's contribution to that tail is scaled by how much of the room it actually fills: all of it from
inside the same room, 0.35 from elsewhere in the building, and 0.1 from outdoors, where the voice arrives
through a doorway rather than ringing the room. Without that, someone shouting from the street sounded as
though they were stood in the room with you.

A room's tail belongs to the room rather than to one voice, so every direct voice adds into one shared
send as it is processed and the tail is rendered once over the mixed playback buffer. It is a Schroeder
reverb: four parallel combs, two allpasses, damping in the feedback so the tail loses its top end first,
and slightly longer delays in the right ear so it is not identical in both. Muffling has already shaped
each voice before it reaches the send, so someone in the next room reverberates as dully as they arrive.
Radio never goes to the room, and stepping outdoors drops the tail rather than letting it ring on.

### Occlusion

Every nearby player is asked about twice: first the engine's room model, and only if that cannot answer,
traces.

**Rooms first.** A building with interior audio carries the engine's own room model, in which area 0 is
the outside world and the areas above it are that building's rooms, separated by doorways and windows.
Where that model covers both people, it decides the muffle outright and no traces are needed:

| Situation | Muffle |
| --- | --- |
| Same room | 0 |
| Different rooms of one building | cheapest path between them across the doorways |
| One inside a building, the other outside it | cheapest path from that room to the outside |

A path adds up what each doorway on it costs: 0.2 wide open, 0.6 shut, and the same 0.6 for an intact
window, which is not a path at all until it breaks. So an open door two rooms away is worth hearing
through and a closed one is worth about as much as the wall beside it. Nothing has to be in line of sight
for this, which is the part traces cannot do: shouting round a corner through an open door now carries.

**Both have a say, and the clearer wins.** A path through the rooms cannot tell whether the two can simply
see each other, through an open hangar door or across a room the engine has split in two; a trace cannot
tell that an open door two rooms away carries a voice. So anything other than same-room is traced as well
and the lower muffle is used. Same room needs neither — it is clear already, and a pillar or a crate between
them must not make it worse. Two people standing in
a doorway are in different areas, and the graph can only charge them for the doorway even though they can
see each other; a trace gets that right, and at that range the direct path dominates anyway.

Each person's room is looked up again after they move 0.5 m or after half a second, and the building they
were last in is asked first, so anyone who has not left a building costs a single call.

The layout of doorways is not something the engine will hand over, so it is worked out once per building
type, by probing the building with small boxes to find its doorways and then asking what lies either side
of each one. That runs 128 probes per write while a player stands in a building type nobody has mapped
yet, and stops as soon as every doorway the engine reports has been found. The result is shared by every
copy of that building on the map.

**Traces otherwise** — both outdoors, two different buildings, or a building with no room model. Two
traces, one from each end: if each end hits a different first obstacle, there are at least two things in
the way.

| Situation | Muffle |
| --- | --- |
| Clear line | 0 |
| One obstacle | 0.6 |
| Two or more | 0.9 |
| Both in the same vehicle | 0 |

**Trees and people are never cover**, and nor is anything narrow. A lamp post, a bollard, a sign or
somebody standing in the way used to block the trace and read as a whole wall, which made open ground sound
like a building; a wood between two people sounded like a wall between them.

The engine's trace filter is given the verdict for each entity along the path, so one trace handles a path
with any number of them on it:

- **Vegetation** — anything deriving from `BaseTree`, which covers trees, the destructible ones and the
  parts a felled one breaks into, plus plain `TreeEntity` static trees.
- **Powerlines** — `PowerlineEntity`, whose bounding box spans the whole distance between its poles.
- **Power and telegraph poles** — `PowerPoleEntity`, thin but with crossarms wide enough to pass the width
  test.
- **Small debris** — `SCR_BaseDebrisSmallEntity`: rubble, splinters and what a felled tree leaves behind.
- **Characters** — whatever their bounding box says.
- **See-through fences and railings**, by prefab name: `NetFence`, `MetalFence`, `PoleFence`, `GraveFence`,
  `GameProofFence`, `PipeRailing`, `RailingMetal`, `BarbedTape`, `BarbedCoil`, `BarbedWire`. Names are a
  poor handle, but a flat chain-link fence has the same shape as a solid one and nothing in its class or
  its material distinguishes them. Plank fences and concrete bridge railings are deliberately absent, being
  solid. One decision per prefab, cached.
- **Anything under 0.8 m across in plan**, measured on the wider of its two horizontal sides, so a fence
  panel or a wall section still counts while a post does not.

The first four are class checks rather than size ones, because their bounding boxes are far larger than
the thing that would actually be in the way.

Terrain and other geometry with no entity behind it always counts as cover.

Vehicles are not a special case. A hull that is really between two people blocks the trace like any other
wall; an open mount does not. Sitting in one used to add a flat muffle, which was wrong for everything you
sit *on* rather than *in* — a mortar, a technical's bed, an open jeep, a hatch you are turned out of.

Traces are cached per player and repeated only as often as they can matter: every 50 ms while that person
is talking, 250 ms while silent, and only when one of you has moved more than 15 cm — otherwise every
second, to catch doors and vehicles moving around a still pair. At most twelve players are re-traced per
write.

## Radio reception

A transmitting plugin announces itself to the TeamSpeak channel roughly once a second, plus whenever it
starts, stops, changes radio, or moves more than 25 m:

```
LC1|RTX|<token>|<player id>|<on>|<frequency kHz>|<range m>|<x>|<y>|<z>|<encryption key>
```

A receiving plugin plays it if it has a radio switched on, tuned to that frequency, with a matching
encryption key, and the transmitter is in range. Quality then comes from one number:

```
effective distance = d + h*7 + h*7*(d/2000)
ratio              = effective distance / range
```

`h` is the terrain clearance and 7 is TFAR's coefficient. Below the clean fraction, quality is 1. Above it,
quality falls linearly to 0.05 at the edge. Beyond the edge, nothing is heard.

Everything audible is driven off that single degradation figure:

| Stage | Clean | Edge of range |
| --- | --- | --- |
| Band-pass | 300–3400 Hz | unchanged |
| Distortion drive | 1.5 | 9.5 |
| Sample-and-hold | 1 sample | 8 samples |
| Bit depth | 10 bits | 4 bits |
| Dropouts | none | starts past 25% degradation |
| Signal-to-noise | 30 dB | −3 dB |
| Voice level | 1.0 | 0.7 |

The voice is deliberately held *down* as degradation rises while the noise rises past it. Normalising the
distortion on peak amplitude instead made bad signals louder than good ones, which is backwards.

### Terrain

The game computes clearance, not the plugin, because only the game can query terrain height. The plugin
asks for the links it needs via `plugin_state.json`, and the game answers in `game_state.json`.

Clearance is how far the midpoint of the path must rise for both halves to clear the ground, sampled every
25 m up to 128 samples, ignoring the first and last 10 m so standing on a slope does not block your own
radio. The result is 0 for line of sight, otherwise clamped to 10–250 m.

The server's terrain setting is applied when computing clearance rather than in the plugin. Scaling `h`
scales the whole penalty exactly as scaling the coefficient would, and the bridge protocol stays fixed.

**Known fragility:** the plugin waits 300 ms for a terrain answer before assuming line of sight. A missed
answer degrades silently to "no terrain effect at all" for that transmission rather than failing loudly.

## AI hearing

When you speak out loud, your client tells the server once a second. The server broadcasts a danger event
at your position with a radius equal to your voice level, capped at 120 m. Each AI that receives it checks
distance, then whether you are an enemy by faction, then what is in the way:

| Obstacles | Range kept |
| --- | --- |
| None | 100% |
| One | 35% |
| Two or more | 15% |

So a shout carries out of a building to the street at 21 m, a normal voice stops at 7 m, and a whisper
through anything at all is inaudible.

The reaction is a **look only** — no move, no investigate, no combat state. It repeats each second you keep
talking, so the head turn holds rather than flicking. The enemy check is faction-based and does not require
the AI to have spotted you.

Radio traffic is never audible to AI.

## Game Master

The editor camera is the listener, so direct speech and occlusion both work from where you are looking
rather than from the body you left behind.

While the editor is open and the server allows it, a Game Master transmits **and** receives without range
or terrain limits. Both directions use the same lever: the radios are treated as having a range of 1000 km.
Since terrain only ever adds to distance, and distance is divided by range, both collapse to nothing.

Transmitting is done by announcing the limitless range, which needs nothing from the plugin. Receiving
needs the plugin, because the range a transmission is judged against comes from the sender — hence the
`unlimitedRx` flag.

A Game Master with no character at all still hears, through the editor camera. One whose character is dead
does not: a body that is not alive is dead regardless of what the camera is doing. The check is
`SCR_EditorManagerEntity.IsOpened()` rather than "has no body", because "has no body" is also true on the
deploy screen, whose camera can sit over a base full of talking players.

**Static radio stations are unaffected.** `RadioStation_base.et` derives from `Props_Base.et` and carries no
`SCR_RadioComponent`, so it has no duplex attribute and is out of scope.

## Extending radios

The duplex attribute rides `modded class SCR_RadioComponent`, which already sits in
`Prefabs/Items/Core/Radio_base.et`. Every radio deriving from it inherits the attribute with no prefab
override, and several mods can add their own attributes to the same component — which overriding
`Radio_base.et` itself would not allow.

`BaseRadioComponent` is engine-generated and cannot be extended. `SCR_RadioComponent` is scripted, which is
why it is the attachment point.
