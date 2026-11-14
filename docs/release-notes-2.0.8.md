# Lima Charlie 2.0.8 — changes since 2.0.6

TeamSpeak plugin 2.0.8. There was no 2.0.7. This is a plugin-only update: the mod stays at 2.0.6 and the
bridge protocol is still 9, so nothing changes on the server and a 2.0.6 plugin keeps working beside 2.0.8
ones. Full detail is in `CHANGELOG.md`.

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
- **The info panel now appears at all.** It was written for 1.0.0 but TeamSpeak never displayed it.

## Changes
- The panel no longer has a "Playing" line; it is the same three lines for everyone.
- The plugin keeps a small `<lc>...</lc>` entry in your TeamSpeak client meta data, next to the ones Task
  Force Radio and ACRE2 keep there. It is taken out again when the plugin is unloaded.

## Installing
Close TeamSpeak, double-click `LimaCharlie_2.0.8.ts3_plugin`, start TeamSpeak. No mod or server update.
