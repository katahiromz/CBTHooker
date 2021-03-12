#include "CBTHooker.h"
#include <tlhelp32.h>
#include <cassert>

#define SHARE_NAME TEXT("CBT Hooker Share")

static HINSTANCE s_hinstDLL = NULL;
static HANDLE s_hMapping = NULL;

static CBTMAP *DoMap(BOOL bWrite = TRUE)
{
    CBTMAP *pMap = new CBTMAP();
    if (pMap == NULL)
        return NULL;

    DWORD dwAccess = bWrite ? FILE_MAP_WRITE : FILE_MAP_READ;
    pMap->hMapping = OpenFileMapping(dwAccess, TRUE, SHARE_NAME);
    if (pMap->hMapping == NULL)
    {
        delete pMap;
        return NULL;
    }

    pMap->pData = reinterpret_cast<CBTDATA*>(
        MapViewOfFile(pMap->hMapping, dwAccess, 0, 0, sizeof(CBTDATA)));
    if (!pMap->pData)
    {
        CloseHandle(pMap->hMapping);
        delete pMap;
        return NULL;
    }

    return pMap;
}

static void DoUnMap(CBTMAP *pMap)
{
    if (pMap)
    {
        UnmapViewOfFile(pMap->pData);
        CloseHandle(pMap->hMapping);
        delete pMap;
    }
}

CBTHOOKAPI BOOL APIENTRY DoSuspendProcess(DWORD pid, BOOL bSuspend)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return FALSE;

    BOOL bSuccess = FALSE;
    THREADENTRY32 entry = { sizeof(entry) };
    if (Thread32First(hSnapshot, &entry))
    {
        do
        {
            if (entry.th32OwnerProcessID == pid)
            {
                if (HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID))
                {
                    if (bSuspend)
                        SuspendThread(hThread);
                    else
                        ResumeThread(hThread);
                    CloseHandle(hThread);
                    bSuccess = TRUE;
                }
            }
        } while (Thread32Next(hSnapshot, &entry));
    }

    CloseHandle(hSnapshot);
    assert(bSuccess);
    return bSuccess;
}

CBTHOOKAPI BOOL APIENTRY DoSuspendWindow(HWND hwnd, BOOL bSuspend)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (bSuspend)
        return DoSuspendProcess(pid, TRUE);
    else
        return DoSuspendProcess(pid, FALSE);
}

CBTHOOKAPI VOID APIENTRY DoAction(HWND hwnd, ACTION_TYPE iAction)
{
    switch (iAction)
    {
    case AT_NOTHING: // Do nothing
        break;
    case AT_SUSPEND: // Suspend
        DoSuspendWindow(hwnd, TRUE);
        break;
    case AT_RESUME: // Resume
        DoSuspendWindow(hwnd, FALSE);
        break;
    case AT_MAXIMIZE: // Maximize
        ShowWindow(hwnd, SW_MAXIMIZE);
        break;
    case AT_MINIMIZE: // Minimize
        ShowWindow(hwnd, SW_MINIMIZE);
        break;
    case AT_RESTORE: // Restore
        ShowWindow(hwnd, SW_RESTORE);
        break;
    case AT_SHOW: // Show
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        break;
    case AT_HIDE: // Hide
        ShowWindow(hwnd, SW_HIDE);
        break;
    case AT_CLOSE: // Close
        PostMessage(hwnd, WM_CLOSE, 0, 0);
        break;
    case AT_DESTROY: // Destroy
        DestroyWindow(hwnd);
        break;
    }
}

static BOOL DoesMatch(HWND hwnd, CBTDATA *pData)
{
    TCHAR szText[MAX_PATH];
    BOOL bMatched = TRUE;

    if (bMatched && pData->has_cls)
    {
        GetClassName(hwnd, szText, MAX_PATH);
        if (lstrcmpi(szText, pData->cls) != 0)
            bMatched = FALSE;
    }
    if (bMatched && pData->has_txt)
    {
        GetWindowText(hwnd, szText, MAX_PATH);
        if (lstrcmpi(szText, pData->txt) != 0)
            bMatched = FALSE;
    }
    if (bMatched && pData->has_pid)
    {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pData->pid != pid)
            bMatched = FALSE;
    }
    if (bMatched && pData->has_tid)
    {
        DWORD tid = GetWindowThreadProcessId(hwnd, NULL);
        if (pData->tid != tid)
            bMatched = FALSE;
    }

    if (bMatched && !pData->hwndFound)
        pData->hwndFound = hwnd;

    return bMatched;
}

static VOID
DoTarget(HWND hwnd, CBTDATA *pData)
{
    if (pData->iAction == 0)
        return;

    if (DoesMatch(hwnd, pData))
    {
        DoAction(hwnd, pData->iAction);
    }
}

static LRESULT CALLBACK
CBTProc(INT nCode, WPARAM wParam, LPARAM lParam)
{
    HHOOK hHook = NULL;

    if (CBTMAP *pMap = DoMap())
    {
        CBTDATA *pData = pMap->pData;
        hHook = pData->hHook;
        HWND hwnd = reinterpret_cast<HWND>(wParam);
        switch (nCode)
        {
        case HCBT_ACTIVATE:
        case HCBT_CREATEWND:
        case HCBT_DESTROYWND:
        case HCBT_MINMAX:
        case HCBT_MOVESIZE:
        case HCBT_SETFOCUS:
            if (pData->nCode == nCode)
                DoTarget(hwnd, pData);
            break;
        }
        DoUnMap(pMap);
    }

    return ::CallNextHookEx(hHook, nCode, wParam, lParam);
}

CBTHOOKAPI BOOL APIENTRY DoStartWatch(const CBTDATA *pData)
{
    if (s_hMapping)
    {
        CloseHandle(s_hMapping);
        s_hMapping = NULL;
    }
    s_hMapping = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                   0, sizeof(CBTDATA), SHARE_NAME);
    if (s_hMapping == NULL)
    {
        MessageBox(NULL, TEXT("ERROR #1"), NULL, MB_ICONERROR);
        return FALSE;
    }

    LPVOID pView = MapViewOfFile(s_hMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(CBTDATA));
    if (!pView)
    {
        MessageBox(NULL, TEXT("ERROR #2"), NULL, MB_ICONERROR);
        CloseHandle(s_hMapping);
        s_hMapping = NULL;
        return FALSE;
    }

    CBTDATA *pNew = reinterpret_cast<CBTDATA*>(pView);
    *pNew = *pData;
    pNew->hHook = SetWindowsHookEx(WH_CBT, CBTProc, s_hinstDLL, 0);
    if (!pNew->hHook)
    {
        MessageBox(NULL, TEXT("ERROR #3"), NULL, MB_ICONERROR);
        UnmapViewOfFile(pView);
        CloseHandle(s_hMapping);
        s_hMapping = NULL;
        return FALSE;
    }

    return TRUE;
}

CBTHOOKAPI BOOL APIENTRY DoEndWatch(VOID)
{
    if (CBTMAP *pMap = DoMap())
    {
        HHOOK hHook = pMap->pData->hHook;
        pMap->pData->hHook = NULL;
        UnhookWindowsHookEx(hHook);
        DoUnMap(pMap);
    }
    if (s_hMapping)
    {
        CloseHandle(s_hMapping);
        s_hMapping = NULL;
    }
    return TRUE;
}

CBTHOOKAPI HWND APIENTRY DoGetTargetWindow(VOID)
{
    HWND ret = NULL;
    if (CBTMAP *pMap = DoMap())
    {
        ret = pMap->pData->hwndFound;
        DoUnMap(pMap);
    }
    return ret;
}

EXTERN_C
BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        s_hinstDLL = hinstDLL;
        s_hMapping = NULL;
        DisableThreadLibraryCalls(hinstDLL);
        break;
    case DLL_PROCESS_DETACH:
        DoEndWatch();
        break;
    }
    return TRUE;
}
