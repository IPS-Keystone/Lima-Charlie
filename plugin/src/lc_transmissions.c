#include "lc_transmissions.h"

#include <windows.h>

#include <string.h>

static CRITICAL_SECTION g_lock;
static int              g_initialized;
static lc_transmission g_items[LC_MAX_TRANSMISSIONS];
static int              g_count;

static int find_index(anyID client)
{
    for (int i = 0; i < g_count; ++i) {
        if (g_items[i].client == client)
            return i;
    }
    return -1;
}

void lc_transmissions_init(void)
{
    InitializeCriticalSection(&g_lock);
    g_count       = 0;
    g_initialized = 1;
}

void lc_transmissions_shutdown(void)
{
    if (!g_initialized)
        return;
    g_initialized = 0;
    DeleteCriticalSection(&g_lock);
}

void lc_transmissions_reset(void)
{
    EnterCriticalSection(&g_lock);
    g_count = 0;
    LeaveCriticalSection(&g_lock);
}

int lc_transmissions_handle_command(anyID sender, const char* command, unsigned long long nowMs)
{
    lc_radio_announcement info;
    if (!lc_radio_parse_announcement(command, &info))
        return 0;

    EnterCriticalSection(&g_lock);
    int index = find_index(sender);
    if (!info.on) {
        if (index >= 0)
            g_items[index] = g_items[--g_count];
    } else {
        if (index < 0 && g_count < LC_MAX_TRANSMISSIONS) {
            index = g_count++;
            memset(&g_items[index], 0, sizeof(g_items[index]));
            g_items[index].client = sender;
        }
        if (index >= 0) {
            lc_transmission* item = &g_items[index];
            if (item->info.frequency != info.frequency || strcmp(item->info.token, info.token) != 0 || strcmp(item->info.key, info.key) != 0)
                item->startMs = nowMs;
            item->info     = info;
            item->updateMs = nowMs;
        }
    }
    LeaveCriticalSection(&g_lock);
    return 1;
}

int lc_transmissions_list(const char* token, lc_transmission* out, int max)
{
    int n = 0;
    EnterCriticalSection(&g_lock);
    for (int i = 0; i < g_count && n < max; ++i) {
        if (strcmp(g_items[i].info.token, token) == 0)
            out[n++] = g_items[i];
    }
    LeaveCriticalSection(&g_lock);
    return n;
}

void lc_transmissions_remove_client(anyID client)
{
    EnterCriticalSection(&g_lock);
    const int index = find_index(client);
    if (index >= 0)
        g_items[index] = g_items[--g_count];
    LeaveCriticalSection(&g_lock);
}

void lc_transmissions_expire(unsigned long long nowMs, unsigned long long maxAgeMs)
{
    EnterCriticalSection(&g_lock);
    for (int i = g_count - 1; i >= 0; --i) {
        if (nowMs > g_items[i].updateMs + maxAgeMs)
            g_items[i] = g_items[--g_count];
    }
    LeaveCriticalSection(&g_lock);
}
