#pragma once

/* What this plugin tells every other TeamSpeak client about itself, for the info panel.

   It is kept in the client's TeamSpeak meta data, which the server hands to everyone who can see the client,
   so it reaches people in other channels and people who are not in a game, neither of which a HELLO does.
   The meta data string is shared with other plugins (TFAR and ACRE2 each keep a tagged section in it), so we
   own exactly one section, "<lc>connected|plugin version|addon version</lc>", and leave the rest alone. */

#include <stddef.h>

/* TeamSpeak's limit on the whole meta data string, terminator included. */
#define LC_PRESENCE_META_CAP 4096

typedef struct {
    int  connected; /* in a game with the addon running */
    char pluginVersion[32];
    char modVersion[32];
} lc_presence;

/* Writes our section. Returns its length, or 0 if it did not fit. */
size_t lc_presence_format(char* out, size_t cap, int connected, const char* pluginVersion, const char* modVersion);

/* Writes meta with our section replaced by section, or appended if there was none. An empty or NULL section
   removes ours. Returns 1 on success, 0 if the result did not fit (out is then empty). */
int lc_presence_merge(const char* meta, const char* section, char* out, size_t cap);

/* Reads our section out of a client's meta data. Returns 1 if it had one. Versions are reduced to plain
   version characters, since they come from another client and end up in our info panel. */
int lc_presence_parse(const char* meta, lc_presence* out);
