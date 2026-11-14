# Lima Charlie 2.0.8 — changes since 2.0.6

TeamSpeak plugin 2.0.8, with the mod changes that go with it. There was no 2.0.7. The bridge protocol is
still 9: a 2.0.8 plugin works with the 2.0.6 mod and beside 2.0.6 plugins, and the new mod works with a
2.0.6 plugin. The performance work needs both halves to take full effect. Full detail is in `CHANGELOG.md`.

Not yet tested in game or in TeamSpeak.

## New
- **Lima Charlie status on TeamSpeak profiles.** Click anyone in TeamSpeak and their info panel now has a
  Lima Charlie section, like Task Force Radio and ACRE2 have:

  ```
  Connected to game: Yes
  Plugin version: 2.0.8
  Addon version: 2.0.6
  ```

  - It shows for people in any channel, in game or not, so you can check who has the plugin and which
    version before an operation starts.
  - "Addon version" is the mod version the person's game is running. It reads "unknown" until they are in
    a game.
  - Both of you need plugin 2.0.8 or newer. Someone still on 2.0.6 only shows while they are in game in
    your channel. Someone without the plugin shows nothing.

## Bug fixes
- **The TeamSpeak info panel now appears at all.** It was written for 1.0.0 but TeamSpeak never displayed it.
- **Carried weapons never muffle.** A rifle or launcher on someone standing between two people could count
  as a wall. Anything worn or carried by a character is now ignored.
- **Camouflage nets no longer muffle voices.**

## Performance (many players)
- **Silent players cost almost nothing.** Someone nearby who is not talking is worked out four times a
  second instead of up to twenty; someone out of range is not looked at until they could be back in it.
- **The fast update rate is for voices you can hear.** It used to run whenever anyone on the server was
  talking, radio included. Now only while someone within direct speech range is talking, or you are.
- **Only voices in earshot are checked for walls.** The plugin now tells the game how far each player's
  voice carries, and a talker beyond that plus 5 m is skipped.
- **Fewer rechecks at a distance, and half the wall checks for a blocked voice.** "One wall" against
  "several" can lag by up to a second; blocked against clear does not lag.
- **Muffled voices cost less in silence** (plugin).
- **Server: AI hearing is capped** at 60 cover checks a second.

## Changes
- The info panel no longer has a "Playing" line; it is the same three lines for everyone.
- The plugin keeps a small `<lc>...</lc>` entry in your TeamSpeak client meta data, next to the ones Task
  Force Radio and ACRE2 keep there. It is taken out again when the plugin is unloaded.

## Installing
Close TeamSpeak, double-click `LimaCharlie_2.0.8.ts3_plugin`, start TeamSpeak. Update the mod when it is
published.
