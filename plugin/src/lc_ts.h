#pragma once

/* TeamSpeak 3 SDK access shared by all plugin modules. */

#include "plugin_definitions.h"
#include "teamspeak/public_definitions.h"
#include "teamspeak/public_errors.h"
#include "teamspeak/public_rare_definitions.h"
#include "ts3_functions.h"

extern struct TS3Functions g_ts3;

/* ID assigned by TeamSpeak in ts3plugin_registerPluginID, needed for sendPluginCommand. NULL until registered. */
const char* lc_plugin_id(void);
