#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <cstring>
#include <string>
#include <strsafe.h>
#include "../CBTHooker.h"

#ifdef UNICODE
    typedef std::wstring tstring;
#else
    typedef std::string tstring;
#endif

BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
{
    LPVOID pvData = lpCreateStruct->lpCreateParams;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pvData));
    return TRUE;
}

LRESULT OnStartWatch(HWND hwnd, CBTDATA *pData)
{
    return DoStartWatch(pData, GetCurrentProcessId());
}

LRESULT OnEndWatch(HWND hwnd, CBTDATA *pData)
{
    BOOL ret = DoEndWatch();
    DestroyWindow(hwnd);
    return ret;
}

LRESULT OnAction(HWND hwnd, CBTDATA *pData, INT iAction)
{
    if (HWND hwndTarget = DoGetTargetWindow())
    {
        switch (iAction)
        {
        case AT_NOTHING: case AT_SUSPEND: case AT_RESUME: case AT_MAXIMIZE:
        case AT_MINIMIZE: case AT_RESTORE: case AT_SHOW: case AT_HIDE:
        case AT_CLOSE: case AT_DESTROY:
            DoAction(hwndTarget, static_cast<ACTION_TYPE>(iAction));
            break;
        default:
            break;
        }
    }
    return 0;
}

void OnDestroy(HWND hwnd)
{
    PostQuitMessage(0);
}

LRESULT CALLBACK
WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CBTDATA* pData = reinterpret_cast<CBTDATA*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_CREATE, OnCreate);
        HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
    default:
        if (uMsg == WATCH_START)
            return OnStartWatch(hwnd, pData);
        else if (uMsg == WATCH_END)
            return OnEndWatch(hwnd, pData);
        else if (uMsg == WATCH_ACTION)
            return OnAction(hwnd, pData, (INT)wParam);
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

EXTERN_C int wmain(int argc, LPWSTR *argv)
{
    HWND hwndNotify = NULL;
    INT nCode = HCBT_ACTIVATE;
    INT iAction = AT_NOTHING;
    BOOL has_cls = FALSE, has_txt = FALSE, has_pid = FALSE;
    BOOL has_tid = FALSE, has_self = FALSE;
    tstring cls, txt;
    DWORD pid = 0, tid = 0, self_pid = 0;
    for (int iarg = 1; iarg < argc; ++iarg)
    {
        LPTSTR arg = argv[iarg];

        if (lstrcmpi(arg, TEXT("/notify")) == 0)
        {
            ++iarg;
            hwndNotify = (HWND)(ULONG_PTR)_tcstoul(argv[iarg], NULL, 0);
            continue;
        }

        if (lstrcmpi(arg, TEXT("/code")) == 0)
        {
            ++iarg;
            nCode = _ttoi(argv[iarg]);
            continue;
        }

        if (lstrcmpi(arg, TEXT("/action")) == 0)
        {
            ++iarg;
            iAction = _ttoi(argv[iarg]);
            continue;
        }

        if (lstrcmpi(arg, TEXT("/cls")) == 0)
        {
            ++iarg;
            cls = argv[iarg];
            has_cls = TRUE;
            continue;
        }

        if (lstrcmpi(arg, TEXT("/txt")) == 0)
        {
            ++iarg;
            txt = argv[iarg];
            has_txt = TRUE;
            continue;
        }

        if (lstrcmpi(arg, TEXT("/pid")) == 0)
        {
            ++iarg;
            pid = _tcstoul(argv[iarg], NULL, 0);
            has_pid = TRUE;
            continue;
        }

        if (lstrcmpi(arg, TEXT("/tid")) == 0)
        {
            ++iarg;
            tid = _tcstoul(argv[iarg], NULL, 0);
            has_tid = TRUE;
            continue;
        }

        if (lstrcmpi(arg, TEXT("/self")) == 0)
        {
            ++iarg;
            self_pid = _tcstoul(argv[iarg], NULL, 0);
            has_self = TRUE;
            continue;
        }
    }

    switch (nCode)
    {
    case HCBT_ACTIVATE: case HCBT_CREATEWND: case HCBT_DESTROYWND:
    case HCBT_MINMAX: case HCBT_SETFOCUS:
        break;
    default:
        fprintf(stderr, "ERROR: invalid code\n");
        return EXIT_FAILURE;
    }

    switch (iAction)
    {
    case AT_NOTHING: case AT_SUSPEND: case AT_RESUME:
    case AT_MAXIMIZE: case AT_MINIMIZE: case AT_RESTORE:
    case AT_SHOW: case AT_HIDE: case AT_CLOSE: case AT_DESTROY:
        break;
    default:
        fprintf(stderr, "ERROR: invalid action\n");
        return EXIT_FAILURE;
    }

    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
#ifdef _WIN64
    wc.lpszClassName = WC_WATCHER64;
#else
    wc.lpszClassName = WC_WATCHER32;
#endif
    if (!RegisterClass(&wc))
    {
        MessageBox(NULL, TEXT("ERROR: RegisterClass failed"), NULL, MB_ICONERROR);
        return EXIT_FAILURE;
    }

    CBTDATA *pData = new CBTDATA;
    pData->hwndNotify = hwndNotify;
    pData->nCode = nCode;
    pData->iAction = static_cast<ACTION_TYPE>(iAction);
    pData->has_cls = has_cls;
    pData->has_txt = has_txt;
    pData->has_pid = has_pid;
    pData->has_tid = has_tid;
    pData->has_self = has_self;
    StringCbCopy(pData->cls, sizeof(pData->cls), cls.c_str());
    StringCbCopy(pData->txt, sizeof(pData->txt), txt.c_str());
    pData->pid = pid;
    pData->tid = tid;
    pData->self_pid = self_pid;
#ifdef _WIN64
    pData->is_64bit = true;
#else
    pData->is_64bit = false;
#endif
    pData->hwndFound = NULL;

    HWND hwnd = CreateWindow(wc.lpszClassName, NULL, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, GetModuleHandle(NULL), pData);
    if (hwnd == NULL)
    {
        MessageBox(NULL, TEXT("ERROR: CreateWindow failed"), NULL, MB_ICONERROR);
        delete pData;
        return EXIT_FAILURE;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    INT my_argc;
    LPWSTR *my_wargv = CommandLineToArgvW(GetCommandLineW(), &my_argc);
    INT ret = wmain(my_argc, my_wargv);
    LocalFree(my_wargv);
    return ret;
}
