#include "../CBTHooker.h"
#include <tlhelp32.h>
#include <tchar.h>
#include <strsafe.h>
#include <cassert>
#include <cstring>

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
        FlushViewOfFile(pMap->pData, sizeof(*pMap->pData));
        UnmapViewOfFile(pMap->pData);
        CloseHandle(pMap->hMapping);
        delete pMap;
    }
}

static BOOL EnableProcessPriviledge(LPCTSTR pszSE_)
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

static BOOL DoSuspendProcess(CBTDATA *pData, DWORD pid, BOOL bSuspend)
{
    if (pData->self_pid == pid || pData->dwMyPID == pid)
        return FALSE;

#ifdef _WIN64
    if (!pData->is_64bit)
        return FALSE;
#else
    if (pData->is_64bit)
        return FALSE;
#endif

    EnableProcessPriviledge(SE_DEBUG_NAME);

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

static BOOL DoSuspendWindow(CBTDATA *pData, HWND hwnd, BOOL bSuspend)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);

    if (bSuspend)
        return DoSuspendProcess(pData, pid, TRUE);
    else
        return DoSuspendProcess(pData, pid, FALSE);
}

CBTHOOKAPI VOID APIENTRY
DoAction(HWND hwnd, ACTION_TYPE iAction, CBTDATA *pData OPTIONAL)
{
    CBTMAP *pMap = NULL;
    if (pData == NULL)
    {
        pMap = DoMap();
        pData = pMap->pData;
    }

    switch (iAction)
    {
    case AT_NOTHING: // Do nothing
        break;
    case AT_SUSPEND: // Suspend
        DoSuspendWindow(pData, hwnd, TRUE);
        break;
    case AT_RESUME: // Resume
        DoSuspendWindow(pData, hwnd, FALSE);
        break;
    case AT_MAXIMIZE: // Maximize
        ShowWindowAsync(hwnd, SW_MAXIMIZE);
        break;
    case AT_MINIMIZE: // Minimize
        ShowWindowAsync(hwnd, SW_MINIMIZE);
        break;
    case AT_RESTORE: // Restore
        ShowWindowAsync(hwnd, SW_RESTORE);
        break;
    case AT_SHOW: // Show
        ShowWindowAsync(hwnd, SW_SHOWNORMAL);
        break;
    case AT_HIDE: // Hide
        ShowWindowAsync(hwnd, SW_HIDE);
        break;
    case AT_CLOSE: // Close
        PostMessage(hwnd, WM_CLOSE, 0, 0);
        break;
    case AT_DESTROY: // Destroy
        DestroyWindow(hwnd);
        break;
    }

    if (pMap)
        DoUnMap(pMap);
}

static BOOL DoesMatch(HWND hwnd, CBTDATA *pData)
{
    if (!IsWindow(hwnd))
        return FALSE;

    TCHAR szText[MAX_PATH];
    BOOL bMatched = TRUE;
    if (bMatched && pData->has_cls)
    {
        szText[0] = 0;
        GetClassName(hwnd, szText, _countof(szText));
        if (lstrcmpi(szText, pData->cls) != 0)
            bMatched = FALSE;
    }
    if (bMatched && pData->has_txt)
    {
        szText[0] = 0;
        GetWindowText(hwnd, szText, _countof(szText));
        CharUpper(szText);

        WCHAR szText2[MAX_PATH];
        szText2[0] = 0;
        StringCbCopy(szText2, sizeof(szText2), pData->txt);
        CharUpper(szText2);

        if (_tcsstr(szText, szText2) == NULL)
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

    return bMatched;
}

static VOID
DoTarget(HWND hwnd, CBTDATA *pData, INT nCode)
{
    if (DoesMatch(hwnd, pData))
    {
        pData->hwndFound = hwnd;
        DoAction(hwnd, pData->iAction, pData);
        PostMessage(pData->hwndNotify, WM_MYNOTIFY,
                    reinterpret_cast<WPARAM>(hwnd),
                    MAKELPARAM(nCode, TRUE));
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
                DoTarget(hwnd, pData, nCode);
            break;
        }
        DoUnMap(pMap);
    }

    return ::CallNextHookEx(hHook, nCode, wParam, lParam);
}

CBTHOOKAPI BOOL APIENTRY DoStartWatch(const CBTDATA *pData, DWORD dwMyPID)
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
        return FALSE;
    }

    LPVOID pView = MapViewOfFile(s_hMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(CBTDATA));
    if (!pView)
    {
        CloseHandle(s_hMapping);
        s_hMapping = NULL;
        return FALSE;
    }

    CBTDATA *pNew = reinterpret_cast<CBTDATA*>(pView);
    *pNew = *pData;
    pNew->hwndFound = NULL;
    pNew->hHook = SetWindowsHookEx(WH_CBT, CBTProc, s_hinstDLL, 0);
    pNew->dwMyPID = dwMyPID;
    if (!pNew->hHook)
    {
        UnmapViewOfFile(pView);
        CloseHandle(s_hMapping);
        s_hMapping = NULL;
        return FALSE;
    }

    FlushViewOfFile(pNew, sizeof(*pNew));
    UnmapViewOfFile(pNew);

    return TRUE;
}

CBTHOOKAPI BOOL APIENTRY DoEndWatch(VOID)
{
    if (CBTMAP *pMap = DoMap())
    {
        auto pData = pMap->pData;
        HHOOK hHook = pData->hHook;
        pData->hHook = NULL;
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
        break;
    }
    return TRUE;
}
