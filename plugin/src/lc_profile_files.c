#include "lc_profile_files.h"

#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>

#include <stdio.h>
#include <string.h>

typedef struct {
    wchar_t gameStatePath[MAX_PATH];
    wchar_t pluginStatePath[MAX_PATH];
    wchar_t pluginStateTempPath[MAX_PATH];
} lc_profile_dir;

static const char* const    kProfileFolderNames[LC_PROFILE_DIR_COUNT] = {"ArmaReforger", "ArmaReforgerWorkbench"};
static const wchar_t* const kProfileFolders[LC_PROFILE_DIR_COUNT]     = {L"ArmaReforger", L"ArmaReforgerWorkbench"};

static lc_profile_dir    g_dirs[LC_PROFILE_DIR_COUNT];
static unsigned long long g_consumedStamp;

static unsigned long long filetime_to_stamp(FILETIME ft)
{
    return ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

static int get_write_stamp(const wchar_t* path, unsigned long long* stamp)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fad))
        return 0;
    *stamp = filetime_to_stamp(fad.ftLastWriteTime);
    return 1;
}

/* Shares read/write/delete so the game's own writes never fail because we are reading. */
static int read_whole_file(const wchar_t* path, char* buf, size_t cap, size_t* outLen)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    DWORD total = 0;
    DWORD n     = 0;
    while (total < cap && ReadFile(h, buf + total, (DWORD)(cap - total), &n, NULL) && n > 0)
        total += n;
    CloseHandle(h);
    buf[total] = '\0';
    *outLen    = total;
    return total > 0;
}

int lc_profile_files_init(void)
{
    PWSTR docs = NULL;
    if (FAILED(SHGetKnownFolderPath(&FOLDERID_Documents, 0, NULL, &docs)) || !docs)
        return 1;
    for (int i = 0; i < LC_PROFILE_DIR_COUNT; ++i) {
        lc_profile_dir* d = &g_dirs[i];
        memset(d, 0, sizeof(*d));
        _snwprintf(d->gameStatePath, MAX_PATH - 1, L"%ls\\My Games\\%ls\\profile\\LimaCharlie\\game_state.json", docs, kProfileFolders[i]);
        _snwprintf(d->pluginStatePath, MAX_PATH - 1, L"%ls\\My Games\\%ls\\profile\\LimaCharlie\\plugin_state.json", docs, kProfileFolders[i]);
        _snwprintf(d->pluginStateTempPath, MAX_PATH - 1, L"%ls.tmp", d->pluginStatePath);
    }
    CoTaskMemFree(docs);
    g_consumedStamp = 0;
    return 0;
}

const char* lc_profile_dir_name(int dirIndex)
{
    return (dirIndex >= 0 && dirIndex < LC_PROFILE_DIR_COUNT) ? kProfileFolderNames[dirIndex] : "none";
}

int lc_profile_poll_game_state(char* buf, size_t cap, size_t* outLen, int* outDirIndex, unsigned long long* outStamp)
{
    int                best      = -1;
    unsigned long long bestStamp = 0;
    for (int i = 0; i < LC_PROFILE_DIR_COUNT; ++i) {
        unsigned long long stamp;
        if (!get_write_stamp(g_dirs[i].gameStatePath, &stamp) || stamp <= g_consumedStamp)
            continue;
        if (best < 0 || stamp > bestStamp) {
            best      = i;
            bestStamp = stamp;
        }
    }
    if (best < 0 || cap < 2)
        return 0;
    if (!read_whole_file(g_dirs[best].gameStatePath, buf, cap - 1, outLen))
        return 0;
    *outDirIndex = best;
    *outStamp    = bestStamp;
    return 1;
}

void lc_profile_consume_game_state(unsigned long long stamp)
{
    if (stamp > g_consumedStamp)
        g_consumedStamp = stamp;
}

unsigned long long lc_profile_now_stamp(void)
{
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    return filetime_to_stamp(now);
}

int lc_profile_write_plugin_state(int dirIndex, const char* data, size_t len)
{
    if (dirIndex < 0 || dirIndex >= LC_PROFILE_DIR_COUNT)
        return 0;
    const lc_profile_dir* d = &g_dirs[dirIndex];

    HANDLE h = CreateFileW(d->pluginStateTempPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    DWORD      written = 0;
    const BOOL ok      = WriteFile(h, data, (DWORD)len, &written, NULL) && written == (DWORD)len;
    CloseHandle(h);
    if (!ok) {
        DeleteFileW(d->pluginStateTempPath);
        return 0;
    }

    /* The game may briefly hold plugin_state.json open while reading it. */
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (MoveFileExW(d->pluginStateTempPath, d->pluginStatePath, MOVEFILE_REPLACE_EXISTING))
            return 1;
        Sleep(1);
    }
    DeleteFileW(d->pluginStateTempPath);
    return 0;
}
