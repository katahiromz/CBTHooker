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

#define WATCH_START  (WM_APP + 100)
#define WATCH_END (WM_APP + 101)
#define WATCH_ACTION (WM_APP + 102)
#define WATCHER_BRINGTOTOP (WM_APP + 103)
#define WATCHER_SINKTOBOTTOM (WM_APP + 104)

#define WM_MYNOTIFY  (WM_APP + 200)

typedef enum ACTION_TYPE
{
    AT_NOTHING,
    AT_SUSPEND,
    AT_RESUME,
    AT_MAXIMIZE,
    AT_MINIMIZE,
    AT_RESTORE,
    AT_SHOW,
    AT_SHOWNA,
    AT_HIDE,
    AT_BRINGTOTOP,
    AT_SINKTOBOTTOM,
    AT_MAKETOPMOST,
    AT_MAKENONTOPMOST,
    AT_CLOSE,
    AT_DESTROY,
} ACTION_TYPE;

typedef struct CBTDATA
{
    HWND hwndNotify;
    HWND hwndWatcher;
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
CBTHOOKAPI VOID APIENTRY DoAction(HWND hwnd, ACTION_TYPE iAction, CBTDATA *pData OPTIONAL);
CBTHOOKAPI HWND APIENTRY DoGetTargetWindow(VOID);

#ifndef MSGFLT_ADD
    #define MSGFLT_ADD 1
    #define MSGFLT_REMOVE 2
    #define MSGFLT_RESET (0)
    #define MSGFLT_ALLOW (1)
    #define MSGFLT_DISALLOW (2)
    typedef struct tagCHANGEFILTERSTRUCT {
        DWORD cbSize;
        DWORD ExtStatus;
    } CHANGEFILTERSTRUCT,*PCHANGEFILTERSTRUCT;
#endif

inline BOOL DoChangeMessageFilter(HWND hwnd, UINT message, BOOL bEnable)
{
    typedef BOOL (WINAPI *FN_ChangeWindowMessageFilterEx)(HWND, UINT, DWORD, PCHANGEFILTERSTRUCT);
    typedef BOOL (WINAPI *FN_ChangeWindowMessageFilter)(UINT, DWORD);

    HMODULE hUser32 = GetModuleHandle(TEXT("user32"));
    auto fn1 = (FN_ChangeWindowMessageFilterEx)GetProcAddress(hUser32, "ChangeWindowMessageFilterEx");
    if (fn1)
    {
        return (*fn1)(hwnd, message, (bEnable ? MSGFLT_ALLOW : MSGFLT_DISALLOW), NULL);
    }
    auto fn2 = (FN_ChangeWindowMessageFilter)GetProcAddress(hUser32, "ChangeWindowMessageFilter");
    if (fn2)
        return (*fn2)(message, (bEnable ? MSGFLT_ADD : MSGFLT_REMOVE));
    return FALSE;
}

inline BOOL EnableProcessPriviledge(LPCTSTR pszSE_)
{
    BOOL f;
    HANDLE hProcess;
    HANDLE hToken;
    LUID luid;
    TOKEN_PRIVILEGES tp;
    
    f = FALSE;
    hProcess = GetCurrentProcess();
    if (OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES, &hToken))
    {
        if (LookupPrivilegeValue(NULL, pszSE_, &luid))
        {
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            tp.Privileges[0].Luid = luid;
            f = AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL);
        }
        CloseHandle(hToken);
    }
    return f;
}
