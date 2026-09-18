/* Lima Charlie - TeamSpeak 3 client plugin entry point. */

#include "lc_audio.h"
#include "lc_core.h"
#include "lc_peers.h"
#include "lc_log.h"
#include "lc_sounds.h"
#include "lc_ts.h"
#include "lc_version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLUGINS_EXPORTDLL __declspec(dllexport)
#define LC_PLUGIN_API_VERSION 26

struct TS3Functions g_ts3;
static char*        g_pluginID = NULL;

const char* lc_plugin_id(void)
{
    return g_pluginID;
}

static void log_to_teamspeak(enum lc_log_level level, const char* message)
{
    static const enum LogLevel levels[] = {LogLevel_DEBUG, LogLevel_INFO, LogLevel_WARNING, LogLevel_ERROR};
    g_ts3.logMessage(message, levels[level], "Lima Charlie", 0);
}

/* ---- Metadata ---- */

PLUGINS_EXPORTDLL const char* ts3plugin_name(void)
{
    return "Lima Charlie";
}

PLUGINS_EXPORTDLL const char* ts3plugin_version(void)
{
    return LC_PLUGIN_VERSION;
}

PLUGINS_EXPORTDLL int ts3plugin_apiVersion(void)
{
    return LC_PLUGIN_API_VERSION;
}

PLUGINS_EXPORTDLL const char* ts3plugin_author(void)
{
    return "Lima Charlie";
}

PLUGINS_EXPORTDLL const char* ts3plugin_description(void)
{
    return "Arma Reforger radio and proximity voice integration.";
}

PLUGINS_EXPORTDLL void ts3plugin_setFunctionPointers(const struct TS3Functions funcs)
{
    g_ts3 = funcs;
}

/* ---- Lifecycle ---- */

PLUGINS_EXPORTDLL int ts3plugin_init(void)
{
    lc_log_set_sink(log_to_teamspeak);
    /* A worker that fails to start is logged, not fatal, so the plugin still loads for diagnosis. */
    lc_core_start();
    return 0;
}

PLUGINS_EXPORTDLL void ts3plugin_shutdown(void)
{
    lc_core_stop();
    lc_log_set_sink(NULL);
    free(g_pluginID);
    g_pluginID = NULL;
}

PLUGINS_EXPORTDLL int ts3plugin_offersConfigure(void)
{
    return PLUGIN_OFFERS_NO_CONFIGURE;
}

PLUGINS_EXPORTDLL void ts3plugin_registerPluginID(const char* id)
{
    const size_t len = strlen(id) + 1;
    g_pluginID       = (char*)malloc(len);
    if (g_pluginID)
        memcpy(g_pluginID, id, len);
}

PLUGINS_EXPORTDLL void ts3plugin_freeMemory(void* data)
{
    free(data);
}

PLUGINS_EXPORTDLL int ts3plugin_requestAutoload(void)
{
    return 0;
}

/* ---- Info panel ---- */

/* Shown under a client in the TeamSpeak info frame. For ourselves this comes from the game state we are
   reading; for anyone else it comes from the HELLO they broadcast, so a player whose game is closed, or who
   has no plugin at all, simply shows nothing rather than a stale entry. */
PLUGINS_EXPORTDLL void ts3plugin_infoData(uint64 serverConnectionHandlerID, uint64 id, enum PluginItemType type, char** data)
{
    if (type != PLUGIN_CLIENT)
        return;

    char        text[512];
    const char* yes = "Yes";
    const char* no  = "No";

    anyID me = 0;
    if (g_ts3.getClientID(serverConnectionHandlerID, &me) != ERROR_ok)
        me = 0;

    if (me != 0 && (anyID)id == me) {
        lc_status status;
        lc_core_status(&status);
        _snprintf(text, sizeof(text) - 1,
                  "[b]Lima Charlie[/b]\n"
                  "Connected to game: %s\n"
                  "Playing: %s\n"
                  "Plugin version: %s\n"
                  "Addon version: %s",
                  status.inGame ? yes : no,
                  status.playing ? yes : no,
                  LC_PLUGIN_VERSION,
                  status.modVersion[0] ? status.modVersion : "unknown");
    } else {
        lc_peer_info peer;
        if (!lc_peers_find((anyID)id, &peer))
            return;

        _snprintf(text, sizeof(text) - 1,
                  "[b]Lima Charlie[/b]\n"
                  "Connected to game: %s\n"
                  "Plugin version: %s\n"
                  "Addon version: %s",
                  yes,
                  peer.pluginVersion[0] ? peer.pluginVersion : "unknown",
                  peer.modVersion[0] ? peer.modVersion : "unknown");
    }

    text[sizeof(text) - 1] = '\0';
    const size_t len       = strlen(text) + 1;
    /* TeamSpeak frees this through ts3plugin_freeMemory. */
    *data = (char*)malloc(len);
    if (*data)
        memcpy(*data, text, len);
}

/* ---- Events ---- */

PLUGINS_EXPORTDLL void ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID)
{
    lc_core_on_connection_changed(serverConnectionHandlerID);
}

PLUGINS_EXPORTDLL void ts3plugin_onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber)
{
    (void)newStatus;
    (void)errorNumber;
    lc_core_on_connection_changed(serverConnectionHandlerID);
}

PLUGINS_EXPORTDLL void ts3plugin_onEditPostProcessVoiceDataEvent(uint64 serverConnectionHandlerID, anyID clientID, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask)
{
    (void)serverConnectionHandlerID;
    lc_audio_process(clientID, samples, sampleCount, channels, channelSpeakerArray, channelFillMask);
}

PLUGINS_EXPORTDLL void ts3plugin_onEditMixedPlaybackVoiceDataEvent(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask)
{
    (void)serverConnectionHandlerID;
    lc_sounds_mix(samples, sampleCount, channels, channelSpeakerArray, channelFillMask);
}

PLUGINS_EXPORTDLL void ts3plugin_onTalkStatusChangeEvent(uint64 serverConnectionHandlerID, int status, int isReceivedWhisper, anyID clientID)
{
    (void)isReceivedWhisper;
    lc_core_on_talk_status(serverConnectionHandlerID, status, clientID);
}

PLUGINS_EXPORTDLL void ts3plugin_onPluginCommandEvent(uint64 serverConnectionHandlerID, const char* pluginName, const char* pluginCommand, anyID invokerClientID, const char* invokerName, const char* invokerUniqueIdentity)
{
    (void)pluginName;
    (void)invokerName;
    (void)invokerUniqueIdentity;
    lc_core_on_plugin_command(serverConnectionHandlerID, invokerClientID, pluginCommand);
}

PLUGINS_EXPORTDLL void ts3plugin_onClientMoveEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* moveMessage)
{
    (void)oldChannelID;
    (void)moveMessage;
    lc_core_on_client_moved(serverConnectionHandlerID, clientID, newChannelID, visibility);
}

PLUGINS_EXPORTDLL void ts3plugin_onClientMoveTimeoutEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* timeoutMessage)
{
    (void)oldChannelID;
    (void)timeoutMessage;
    lc_core_on_client_moved(serverConnectionHandlerID, clientID, newChannelID, visibility);
}

PLUGINS_EXPORTDLL void ts3plugin_onClientMoveMovedEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID moverID, const char* moverName, const char* moverUniqueIdentifier, const char* moveMessage)
{
    (void)oldChannelID;
    (void)moverID;
    (void)moverName;
    (void)moverUniqueIdentifier;
    (void)moveMessage;
    lc_core_on_client_moved(serverConnectionHandlerID, clientID, newChannelID, visibility);
}

PLUGINS_EXPORTDLL void ts3plugin_onClientKickFromChannelEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, const char* kickMessage)
{
    (void)oldChannelID;
    (void)kickerID;
    (void)kickerName;
    (void)kickerUniqueIdentifier;
    (void)kickMessage;
    lc_core_on_client_moved(serverConnectionHandlerID, clientID, newChannelID, visibility);
}

PLUGINS_EXPORTDLL void ts3plugin_onClientKickFromServerEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, const char* kickMessage)
{
    (void)oldChannelID;
    (void)newChannelID;
    (void)kickerID;
    (void)kickerName;
    (void)kickerUniqueIdentifier;
    (void)kickMessage;
    lc_core_on_client_moved(serverConnectionHandlerID, clientID, 0, visibility);
}
