#pragma once

#ifndef _INC_WINDOWS
    #include <windows.h>
#endif

#ifdef CBTHOOK_DLL
    #define CBTHOOKAPI  EXTERN_C __declspec(dllexport)
#else
    #define CBTHOOKAPI  EXTERN_C __declspec(dllimport)
#endif

#define WC_WATCHER32  TEXT("CBT Hooker Watcher 32")
#define WC_WATCHER64  TEXT("CBT Hooker Watcher 64")

#define WATCH_START  (WM_USER + 100)
#define WATCH_END (WM_USER + 101)
#define WATCH_ACTION (WM_USER + 102)

typedef enum ACTION_TYPE
{
    AT_NOTHING,
    AT_SUSPEND,
    AT_RESUME,
    AT_MAXIMIZE,
    AT_MINIMIZE,
    AT_RESTORE,
    AT_SHOW,
    AT_HIDE,
    AT_CLOSE,
    AT_DESTROY,
} ACTION_TYPE;

typedef struct CBTDATA
{
    HWND hwndNotify;
    INT nCode;
    ACTION_TYPE iAction;
    HHOOK hHook;
    BOOL has_cls;
    BOOL has_txt;
    BOOL has_pid;
    BOOL has_tid;
    BOOL has_self;
    TCHAR cls[MAX_PATH];
    TCHAR txt[MAX_PATH];
    DWORD pid;
    DWORD tid;
    DWORD self_pid;
    DWORD dwMyPID;
    BOOL is_64bit;
    HWND hwndFound;
} CBTDATA;

typedef struct CBTMAP
{
    HANDLE hMapping;
    CBTDATA *pData;
} CBTMAP;

CBTHOOKAPI BOOL APIENTRY DoStartWatch(const CBTDATA *pData, DWORD dwMyPID);
CBTHOOKAPI BOOL APIENTRY DoEndWatch(VOID);
CBTHOOKAPI VOID APIENTRY DoAction(HWND hwnd, ACTION_TYPE iAction);
CBTHOOKAPI HWND APIENTRY DoGetTargetWindow(VOID);
