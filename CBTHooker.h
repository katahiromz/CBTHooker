#pragma once

#ifndef _INC_WINDOWS
    #include <windows.h>
#endif

#ifdef CBTHOOK_DLL
    #define CBTHOOKAPI  EXTERN_C __declspec(dllexport)
#else
    #define CBTHOOKAPI  EXTERN_C __declspec(dllimport)
#endif

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
    INT nCode;
    ACTION_TYPE iAction;
    HHOOK hHook;
    BOOL has_cls;
    BOOL has_txt;
    BOOL has_pid;
    BOOL has_tid;
    TCHAR cls[MAX_PATH];
    TCHAR txt[MAX_PATH];
    DWORD pid;
    DWORD tid;
    HWND hwndFound;
} CBTDATA;

typedef struct CBTMAP
{
    HANDLE hMapping;
    CBTDATA *pData;
} CBTMAP;

CBTHOOKAPI BOOL APIENTRY DoStartWatch(const CBTDATA *pData);
CBTHOOKAPI BOOL APIENTRY DoEndWatch(VOID);
CBTHOOKAPI BOOL APIENTRY DoSuspendProcess(DWORD pid, BOOL bSuspend);
CBTHOOKAPI BOOL APIENTRY DoSuspendWindow(HWND hwnd, BOOL bSuspend);
CBTHOOKAPI VOID APIENTRY DoAction(HWND hwnd, ACTION_TYPE iAction);
CBTHOOKAPI HWND APIENTRY DoGetTargetWindow(VOID);
