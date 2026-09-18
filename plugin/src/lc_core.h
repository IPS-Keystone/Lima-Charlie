#pragma once

/* Plugin logic, run on a worker thread: reads the game state, controls the microphone, moves the client
   into the game's channel, announces this client to other plugins and reports back to the game.
   The event functions are called from TeamSpeak callbacks. */

#include "lc_ts.h"

/* A snapshot for the TeamSpeak info panel. Safe to call from a TeamSpeak callback thread. */
typedef struct {
    int  inGame;      /* the game is running and writing a fresh state file */
    int  playing;     /* in game and able to hear, i.e. not dead and not at a menu */
    int  playerId;
    char playerName[128];
    char modVersion[32];
    char token[64];
} lc_status;

void lc_core_status(lc_status* out);

int lc_core_start(void);
void lc_core_stop(void);

void lc_core_on_talk_status(uint64 serverConnectionHandlerID, int status, anyID client);
void lc_core_on_plugin_command(uint64 serverConnectionHandlerID, anyID sender, const char* command);
void lc_core_on_client_moved(uint64 serverConnectionHandlerID, anyID client, uint64 newChannelID, int visibility);
void lc_core_on_connection_changed(uint64 serverConnectionHandlerID);
