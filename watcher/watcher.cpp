#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <cstring>
#include <cassert>
#include <string>
#include <strsafe.h>
#include "../hooker.h"

#ifdef UNICODE
    typedef std::wstring tstring;
#else
    typedef std::string tstring;
#endif

BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
{
    DoChangeMessageFilter(hwnd, WATCH_START, TRUE);
    DoChangeMessageFilter(hwnd, WATCH_END, TRUE);
    DoChangeMessageFilter(hwnd, WATCH_ACTION, TRUE);
    DoChangeMessageFilter(hwnd, WATCHER_BRINGTOTOP, TRUE);
    DoChangeMessageFilter(hwnd, WATCHER_SINKTOBOTTOM, TRUE);

    LPVOID pvData = lpCreateStruct->lpCreateParams;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pvData));
    return TRUE;
}

LRESULT OnStartWatch(HWND hwnd, CBTDATA *pData)
{
    pData->hwndWatcher = hwnd;
    return DoStartWatch(pData, GetCurrentProcessId());
}

LRESULT OnEndWatch(HWND hwnd, CBTDATA *pData)
{
    BOOL ret = DoEndWatch();
    DestroyWindow(hwnd);
    return ret;
}

LRESULT OnBringToTop(HWND hwnd, CBTDATA *pData, HWND hwndTarget)
{
    return BringWindowToTop(hwndTarget);
}

LRESULT OnSinkToBottom(HWND hwnd, CBTDATA *pData, HWND hwndTarget)
{
    UINT uSWP_ = SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOACTIVATE;
    return SetWindowPos(hwndTarget, HWND_BOTTOM, 0, 0, 0, 0, uSWP_);
}

LRESULT OnAction(HWND hwnd, CBTDATA *pData, INT iAction)
{
    if (HWND hwndTarget = DoGetTargetWindow())
    {
        switch (iAction)
        {
        case AT_NOTHING:
        case AT_SUSPEND:
        case AT_RESUME:
        case AT_MAXIMIZE:
        case AT_MINIMIZE:
        case AT_RESTORE:
        case AT_SHOW:
        case AT_SHOWNA:
        case AT_HIDE:
        case AT_BRINGTOTOP:
        case AT_SINKTOBOTTOM:
        case AT_MAKETOPMOST:
        case AT_MAKENONTOPMOST:
        case AT_CLOSE:
        case AT_DESTROY:
            DoAction(hwndTarget, static_cast<ACTION_TYPE>(iAction), NULL);
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
        else if (uMsg == WATCHER_BRINGTOTOP)
            return OnBringToTop(hwnd, pData, (HWND)wParam);
        else if (uMsg == WATCHER_SINKTOBOTTOM)
            return OnSinkToBottom(hwnd, pData, (HWND)wParam);
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

EXTERN_C INT wmain(INT argc, LPWSTR *argv)
{
    if (argc <= 1 ||
        lstrcmpiW(argv[1], L"--help") == 0 ||
        lstrcmpiW(argv[1], L"--version") == 0)
    {
        fprintf(stderr, "ERROR: You should not startup this program manually\n");
        return EXIT_FAILURE;
    }

    HWND hwndNotify = NULL;
    INT nCode = HCBT_ACTIVATE;
    INT iAction = AT_NOTHING;
    BOOL has_cls = FALSE, has_txt = FALSE, has_pid = FALSE;
    BOOL has_tid = FALSE, has_self = FALSE;
    tstring cls, txt;
    DWORD pid = 0, tid = 0, self_pid = 0;
    for (INT iarg = 1; iarg < argc; ++iarg)
    {
        LPWSTR arg = argv[iarg];
        if (arg == NULL)
            break;

        if (lstrcmpiW(arg, L"--notify") == 0)
        {
            ++iarg;
            if (argv[iarg] == NULL)
                break;
            hwndNotify = (HWND)(ULONG_PTR)wcstoul(argv[iarg], NULL, 0);
            continue;
        }

        if (lstrcmpiW(arg, L"--code") == 0)
        {
            ++iarg;
            if (argv[iarg] == NULL)
                break;
            nCode = _wtoi(argv[iarg]);
            continue;
        }

        if (lstrcmpiW(arg, L"--action") == 0)
        {
            ++iarg;
            if (argv[iarg] == NULL)
                break;
            iAction = _wtoi(argv[iarg]);
            continue;
        }

        if (lstrcmpiW(arg, L"--cls") == 0)
        {
            ++iarg;
            if (argv[iarg] == NULL)
                break;
            cls = argv[iarg];
            has_cls = TRUE;
            continue;
        }

        if (lstrcmpiW(arg, L"--txt") == 0)
        {
            ++iarg;
            if (argv[iarg] == NULL)
                break;
            txt = argv[iarg];
            has_txt = TRUE;
            continue;
        }

        if (lstrcmpiW(arg, L"--pid") == 0)
        {
            ++iarg;
            if (argv[iarg] == NULL)
                break;
            pid = wcstoul(argv[iarg], NULL, 0);
            has_pid = TRUE;
            continue;
        }

        if (lstrcmpiW(arg, L"--tid") == 0)
        {
            ++iarg;
            if (argv[iarg] == NULL)
                break;
            tid = wcstoul(argv[iarg], NULL, 0);
            has_tid = TRUE;
            continue;
        }

        if (lstrcmpiW(arg, L"--self") == 0)
        {
            ++iarg;
            if (argv[iarg] == NULL)
                break;
            self_pid = wcstoul(argv[iarg], NULL, 0);
            has_self = TRUE;
            continue;
        }
    }

    switch (nCode)
    {
    case HCBT_ACTIVATE: case HCBT_CREATEWND: case HCBT_DESTROYWND:
    case HCBT_MINMAX: case HCBT_SETFOCUS: case HCBT_MOVESIZE:
    case HCBT_CLICKSKIPPED: case HCBT_KEYSKIPPED: case HCBT_QS:
    case HCBT_SYSCOMMAND: case -1:
        break;
    default:
        fprintf(stderr, "ERROR: invalid code\n");
        return EXIT_FAILURE;
    }

    switch (iAction)
    {
    case AT_NOTHING:
    case AT_SUSPEND:
    case AT_RESUME:
    case AT_MAXIMIZE:
    case AT_MINIMIZE:
    case AT_RESTORE:
    case AT_SHOW:
    case AT_SHOWNA:
    case AT_HIDE:
    case AT_BRINGTOTOP:
    case AT_SINKTOBOTTOM:
    case AT_MAKETOPMOST:
    case AT_MAKENONTOPMOST:
    case AT_CLOSE:
    case AT_DESTROY:
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
    INT my_argc = 0;
    LPWSTR *my_wargv = CommandLineToArgvW(GetCommandLineW(), &my_argc);
    INT ret = wmain(my_argc, my_wargv);
    LocalFree(my_wargv);
    return ret;
}
