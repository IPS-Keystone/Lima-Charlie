#pragma once

/* File bridge with the game: <Documents>\My Games\<profile folder>\profile\LimaCharlie\
   The game writes game_state.json; the plugin answers in plugin_state.json.
   Both the retail game and Workbench profile folders are watched; the most recently written wins. */

#include <stddef.h>

#define LC_PROFILE_DIR_COUNT 2

/* Resolves the profile paths. Returns 0 on success. */
int lc_profile_files_init(void);

/* Folder name ("ArmaReforger" or "ArmaReforgerWorkbench") for logs. */
const char* lc_profile_dir_name(int dirIndex);

/* Reads game_state.json if a profile holds a version newer than the last consumed one.
   On success buf is NUL-terminated and 1 is returned. Call lc_profile_consume_game_state once the
   content parsed, so a half-written file is simply read again on the next poll. */
int lc_profile_poll_game_state(char* buf, size_t cap, size_t* outLen, int* outDirIndex, unsigned long long* outStamp);
void lc_profile_consume_game_state(unsigned long long stamp);

/* Current time in the same units as the stamps (100 ns FILETIME). */
unsigned long long lc_profile_now_stamp(void);

/* Atomically replaces plugin_state.json in the given profile. Returns 1 on success. */
int lc_profile_write_plugin_state(int dirIndex, const char* data, size_t len);
