#pragma once

/* Radio transmissions other plugins have announced with RTX commands. Written from TeamSpeak callbacks,
   read by the worker. A client transmits on at most one radio at a time. */

#include "lc_radio.h"
#include "lc_ts.h"

#define LC_MAX_TRANSMISSIONS 64

typedef struct {
    anyID                  client;
    lc_radio_announcement info;
    unsigned long long     startMs;  /* when this client started transmitting on the current frequency */
    unsigned long long     updateMs; /* last announcement */
} lc_transmission;

void lc_transmissions_init(void);
void lc_transmissions_shutdown(void);
void lc_transmissions_reset(void);

/* Returns 1 if command was a valid RTX announcement (and applied it). */
int lc_transmissions_handle_command(anyID sender, const char* command, unsigned long long nowMs);

/* Copies up to max transmissions of the given session into out; returns how many. */
int lc_transmissions_list(const char* token, lc_transmission* out, int max);

void lc_transmissions_remove_client(anyID client);

/* Drops transmissions whose sender stopped refreshing them (lost stop announcement, crash). */
void lc_transmissions_expire(unsigned long long nowMs, unsigned long long maxAgeMs);
