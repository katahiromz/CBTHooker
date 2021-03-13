#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <tchar.h>
#include <cstring>
#include <string>
#include <shlwapi.h>
#include <strsafe.h>
#include "../CBTHooker.h"
#include "resource.h"

#define TIMER_ID 0xFEEDBEEF

static HINSTANCE s_hInst = NULL;
static BOOL s_bWatching = FALSE;

static const UINT s_action_ids[] =
{
    IDS_AT_NOTHING, IDS_AT_SUSPEND, IDS_AT_RESUME, IDS_AT_MAXIMIZE,
    IDS_AT_MINIMIZE, IDS_AT_RESTORE, IDS_AT_SHOW, IDS_AT_HIDE,
    IDS_AT_CLOSE, IDS_AT_DESTROY
};

LPTSTR LoadStringDx(INT nID)
{
    static UINT s_index = 0;
    const UINT cchBuffMax = 1024;
    static TCHAR s_sz[4][cchBuffMax];

    TCHAR *pszBuff = s_sz[s_index];
    s_index = (s_index + 1) % _countof(s_sz);
    pszBuff[0] = 0;
    ::LoadString(NULL, nID, pszBuff, cchBuffMax);
    return pszBuff;
}

void DoAddText(HWND hwnd, LPCTSTR pszText)
{
    HWND hwndEdit = GetDlgItem(hwnd, edt1);
    INT cch = GetWindowTextLength(hwndEdit);
    SendMessage(hwndEdit, EM_SETSEL, cch, cch);
    SendMessage(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)pszText);
    SendMessage(hwndEdit, EM_SCROLLCARET, 0, 0);
}

BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    HICON hIcon = LoadIcon(s_hInst, MAKEINTRESOURCE(IDI_MAINICON));

    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

    static const UINT s_cbt_sids[] =
    {
        IDS_HCBT_ACTIVATE, IDS_HCBT_CREATEWND, IDS_HCBT_DESTROYWND,
        IDS_HCBT_MINMAX, IDS_HCBT_SETFOCUS
    };
    for (auto id : s_cbt_sids)
    {
        SendDlgItemMessage(hwnd, cmb1, CB_ADDSTRING, 0, (LPARAM)LoadStringDx(id));
    }
    SendDlgItemMessage(hwnd, cmb1, CB_SETCURSEL, 0, 0);

    static const LPCTSTR s_cls[] =
    {
        TEXT("BUTTON"), TEXT("COMBOBOX"), TEXT("EDIT"), TEXT("LISTBOX"),
        TEXT("SCROLLBAR"), TEXT("STATIC"), TEXT("ComboBoxEx32"),
        TEXT("NativeFontCtl"), TEXT("ReBarWindow32"), TEXT("SysAnimate32"),
        TEXT("SysDateTimePick32"), TEXT("SysHeader32"), TEXT("SysIPAddress32"),
        TEXT("SysLink"), TEXT("SysListView32"), TEXT("SysMonthCal32"),
        TEXT("SysPager"), TEXT("SysTabControl32"), TEXT("SysTreeView32"),
        TEXT("ToolbarWindow32"), TEXT("msctls_hotkey32"), TEXT("msctls_progress32"),
        TEXT("msctls_statusbar32"), TEXT("msctls_trackbar32"),
        TEXT("msctls_updown32"), TEXT("tooltips_class32"),
        TEXT("AtlAxWin"), TEXT("AtlAxWin71"), TEXT("AtlAxWin80"),
        TEXT("AtlAxWin90"), TEXT("AtlAxWin100"), TEXT("AtlAxWin110"),
        TEXT("AtlAxWin140"),
    };
    for (auto cls : s_cls)
    {
        SendDlgItemMessage(hwnd, cmb2, CB_ADDSTRING, 0, (LPARAM)cls);
    }

    for (auto id : s_action_ids)
    {
        SendDlgItemMessage(hwnd, cmb6, CB_ADDSTRING, 0, (LPARAM)LoadStringDx(id));
    }
    SendDlgItemMessage(hwnd, cmb6, CB_SETCURSEL, 0, 0);

    return TRUE;
}

void DoEnableControls(HWND hwnd, BOOL bEnable)
{
    if (bEnable)
    {
        EnableWindow(GetDlgItem(hwnd, chx1), TRUE);
        EnableWindow(GetDlgItem(hwnd, chx2), TRUE);
        EnableWindow(GetDlgItem(hwnd, chx3), TRUE);
        EnableWindow(GetDlgItem(hwnd, chx4), TRUE);
        EnableWindow(GetDlgItem(hwnd, chx5), TRUE);
        EnableWindow(GetDlgItem(hwnd, cmb1), TRUE);
        EnableWindow(GetDlgItem(hwnd, cmb2), TRUE);
        EnableWindow(GetDlgItem(hwnd, cmb3), TRUE);
        EnableWindow(GetDlgItem(hwnd, cmb4), TRUE);
        EnableWindow(GetDlgItem(hwnd, cmb5), TRUE);
        //EnableWindow(GetDlgItem(hwnd, cmb6), TRUE);
        SetDlgItemText(hwnd, IDOK, LoadStringDx(IDS_STARTWATCH));
    }
    else
    {
        EnableWindow(GetDlgItem(hwnd, chx1), FALSE);
        EnableWindow(GetDlgItem(hwnd, chx2), FALSE);
        EnableWindow(GetDlgItem(hwnd, chx3), FALSE);
        EnableWindow(GetDlgItem(hwnd, chx4), FALSE);
        EnableWindow(GetDlgItem(hwnd, chx5), FALSE);
        EnableWindow(GetDlgItem(hwnd, cmb1), FALSE);
        EnableWindow(GetDlgItem(hwnd, cmb2), FALSE);
        EnableWindow(GetDlgItem(hwnd, cmb3), FALSE);
        EnableWindow(GetDlgItem(hwnd, cmb4), FALSE);
        EnableWindow(GetDlgItem(hwnd, cmb5), FALSE);
        //EnableWindow(GetDlgItem(hwnd, cmb6), FALSE);
        SetDlgItemText(hwnd, IDOK, LoadStringDx(IDS_ENDWATCH));
    }
}

static LPCTSTR DoGetParams(HWND hwnd, CBTDATA *pData)
{
    TCHAR sz1[MAX_PATH], sz2[MAX_PATH];
    TCHAR sz3[32], sz4[32], sz5[32];
    static TCHAR s_szText[MAX_PATH * 3];

    if (pData->has_cls)
        StringCbPrintf(sz1, sizeof(sz1), TEXT("/cls \"%s\""), pData->cls);
    else
        sz1[0] = 0;

    if (pData->has_txt)
        StringCbPrintf(sz2, sizeof(sz2), TEXT("/txt \"%s\""), pData->txt);
    else
        sz2[0] = 0;

    if (pData->has_pid)
        StringCbPrintf(sz3, sizeof(sz3), TEXT("/pid %u"), pData->pid);
    else
        sz3[0] = 0;

    if (pData->has_tid)
        StringCbPrintf(sz4, sizeof(sz4), TEXT("/tid %u"), pData->tid);
    else
        sz4[0] = 0;

    if (pData->has_self)
        StringCbPrintf(sz5, sizeof(sz5), TEXT("/self %u"), pData->self_pid);
    else
        sz5[0] = 0;

    StringCbPrintf(s_szText, sizeof(s_szText),
        TEXT("/notity %u /code %d /action %u /self %u ")
        TEXT("%s %s %s %s %s"),
        (ULONG)(ULONG_PTR)pData->hwndNotify,
        pData->nCode,
        pData->iAction,
        sz1, sz2, sz3, sz4, sz5);
    return s_szText;
}

BOOL IsWow64(HANDLE hProcess)
{
    typedef BOOL (WINAPI *FN_IsWow64Process)(HANDLE, LPBOOL);
    HMODULE hKernel32 = GetModuleHandleA("kernel32");
    FN_IsWow64Process pIsWow64Process =
        (FN_IsWow64Process)GetProcAddress(hKernel32, "IsWow64Process");
    if (!pIsWow64Process)
        return FALSE;

    BOOL bWow64;
    if ((*pIsWow64Process)(hProcess, &bWow64))
        return bWow64;
    return FALSE;
}

BOOL DoStartWatcher(HWND hwnd, CBTDATA *pData)
{
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, _countof(szPath));
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, TEXT("watcher32.exe"));

    LPCTSTR pszParams = DoGetParams(hwnd, pData);
    INT ret1 = (INT)(ULONG_PTR)ShellExecute(hwnd, NULL, szPath, pszParams, NULL, SW_HIDE);
    if (ret1 > 32)
    {
        HWND hwndWatcher;
        for (;;)
        {
            hwndWatcher = FindWindow(WC_WATCHER32, NULL);
            if (hwndWatcher)
                break;
            Sleep(100);
        }
        PostMessage(hwndWatcher, WATCH_START, 0, 0);
    }
    else
    {
        MessageBox(hwnd, TEXT("ERROR #1"), NULL, MB_ICONERROR);
        return FALSE;
    }

    BOOL b64BitSupport = IsWow64(GetCurrentProcess());
    if (!b64BitSupport)
        return TRUE;

    PathRemoveFileSpec(szPath);
    PathAppend(szPath, TEXT("watcher64.exe"));
    INT ret2 = (INT)(ULONG_PTR)ShellExecute(hwnd, NULL, szPath, pszParams, NULL, SW_HIDE);
    if (ret2 > 32)
    {
        HWND hwndWatcher;
        for (;;)
        {
            hwndWatcher = FindWindow(WC_WATCHER64, NULL);
            if (hwndWatcher)
                break;
            Sleep(100);
        }
        PostMessage(hwndWatcher, WATCH_START, 0, 0);
    }
    else
    {
        MessageBox(hwnd, TEXT("ERROR #2"), NULL, MB_ICONERROR);
        return FALSE;
    }

    return TRUE;
}

BOOL DoWatcherAction(HWND hwnd, INT iAction)
{
    if (HWND hwndWatcher = FindWindow(WC_WATCHER32, NULL))
    {
        PostMessage(hwndWatcher, WATCH_ACTION, iAction, 0);
    }
    if (HWND hwndWatcher = FindWindow(WC_WATCHER64, NULL))
    {
        PostMessage(hwndWatcher, WATCH_ACTION, iAction, 0);
    }
    return TRUE;
}

BOOL DoEndWatcher(VOID)
{
    if (HWND hwndWatcher = FindWindow(WC_WATCHER32, NULL))
    {
        PostMessage(hwndWatcher, WATCH_END, 0, 0);
    }
    if (HWND hwndWatcher = FindWindow(WC_WATCHER64, NULL))
    {
        PostMessage(hwndWatcher, WATCH_END, 0, 0);
    }
    return TRUE;
}

void OnOK(HWND hwnd)
{
    if (!s_bWatching)
    {
        CBTDATA data;
        ZeroMemory(&data, sizeof(data));

        INT i = ::SendDlgItemMessage(hwnd, cmb1, CB_GETCURSEL, 0, 0);
        switch (i)
        {
        case 0: data.nCode = HCBT_ACTIVATE; break;
        case 1: data.nCode = HCBT_CREATEWND; break;
        case 2: data.nCode = HCBT_DESTROYWND; break;
        case 3: data.nCode = HCBT_MINMAX; break;
        case 4: data.nCode = HCBT_SETFOCUS; break;
        default:
            MessageBox(hwnd, TEXT("Please choose CBT type."), NULL, MB_ICONERROR);
            return;
        }

        data.has_cls = (IsDlgButtonChecked(hwnd, chx1) == BST_CHECKED);
        data.has_txt = (IsDlgButtonChecked(hwnd, chx2) == BST_CHECKED);
        data.has_pid = (IsDlgButtonChecked(hwnd, chx3) == BST_CHECKED);
        data.has_tid = (IsDlgButtonChecked(hwnd, chx4) == BST_CHECKED);

        TCHAR szText[MAX_PATH];

        i = (INT)SendDlgItemMessage(hwnd, cmb2, CB_GETCURSEL, 0, 0);
        if (i == CB_ERR)
            GetDlgItemText(hwnd, cmb2, data.cls, _countof(data.cls));
        else
            SendDlgItemMessage(hwnd, cmb2, CB_GETLBTEXT, i, (LPARAM)data.cls);

        i = (INT)SendDlgItemMessage(hwnd, cmb3, CB_GETCURSEL, 0, 0);
        if (i == CB_ERR)
            GetDlgItemText(hwnd, cmb3, data.txt, _countof(data.txt));
        else
            SendDlgItemMessage(hwnd, cmb3, CB_GETLBTEXT, i, (LPARAM)data.txt);

        GetDlgItemText(hwnd, cmb4, szText, _countof(szText));
        if (szText[0])
        {
            data.pid = _tcstoul(szText, NULL, 0);
        }
        else
        {
            data.has_pid = FALSE;
            data.pid = 0;
        }

        GetDlgItemText(hwnd, cmb5, szText, _countof(szText));
        if (szText[0])
        {
            data.tid = _tcstoul(szText, NULL, 0);
        }
        else
        {
            data.has_tid = FALSE;
            data.tid = 0;
        }

        i = SendDlgItemMessage(hwnd, cmb6, CB_GETCURSEL, 0, 0);
        data.iAction = static_cast<ACTION_TYPE>(i);
        if (IsDlgButtonChecked(hwnd, chx5) != BST_CHECKED)
            data.iAction = AT_NOTHING;

        data.has_self = TRUE;
        data.self_pid = GetCurrentProcessId();
        data.hwndNotify = hwnd;
        data.hwndFound = NULL;

        DoStartWatcher(hwnd, &data);
    }
    else
    {
        DoAddText(hwnd, TEXT("DoEndWatcher()\r\n"));
        if (DoEndWatcher())
        {
            s_bWatching = FALSE;
            DoEnableControls(hwnd, TRUE);
        }
    }
}

void OnCancel(HWND hwnd)
{
    DoAddText(hwnd, TEXT("DoEndWatcher()\r\n"));
    if (DoEndWatcher())
    {
        s_bWatching = FALSE;
        DoEnableControls(hwnd, TRUE);
    }
    EndDialog(hwnd, IDCANCEL);
}

void OnPsh1(HWND hwnd)
{
    INT iAction = SendDlgItemMessage(hwnd, cmb6, CB_GETCURSEL, 0, 0);
    DoWatcherAction(hwnd, iAction);
}

void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case IDOK:
        OnOK(hwnd);
        break;
    case IDCANCEL:
        OnCancel(hwnd);
        break;
    case psh1:
        OnPsh1(hwnd);
        break;
    case cmb2:
        if (codeNotify == CBN_EDITCHANGE || codeNotify == CBN_SELENDOK)
        {
            CheckDlgButton(hwnd, chx1, BST_CHECKED);
        }
        break;
    case cmb3:
        if (codeNotify == CBN_EDITCHANGE || codeNotify == CBN_SELENDOK)
        {
            CheckDlgButton(hwnd, chx2, BST_CHECKED);
        }
        break;
    case cmb4:
        if (codeNotify == CBN_EDITCHANGE || codeNotify == CBN_SELENDOK)
        {
            CheckDlgButton(hwnd, chx3, BST_CHECKED);
        }
        break;
    case cmb5:
        if (codeNotify == CBN_EDITCHANGE || codeNotify == CBN_SELENDOK)
        {
            CheckDlgButton(hwnd, chx4, BST_CHECKED);
        }
        break;
    case cmb6:
        if (codeNotify == CBN_SELENDOK)
        {
            CheckDlgButton(hwnd, chx5, BST_CHECKED);
        }
        break;
    case chx1:
        if (codeNotify == BN_CLICKED)
        {
            if (IsDlgButtonChecked(hwnd, chx1) == BST_CHECKED)
            {
                SendDlgItemMessage(hwnd, cmb2, EM_SETSEL, 0, -1);
                SetFocus(GetDlgItem(hwnd, cmb2));
            }
            else
            {
                SetDlgItemText(hwnd, cmb2, NULL);
            }
        }
        break;
    case chx2:
        if (codeNotify == BN_CLICKED)
        {
            if (IsDlgButtonChecked(hwnd, chx2) == BST_CHECKED)
            {
                SendDlgItemMessage(hwnd, cmb3, EM_SETSEL, 0, -1);
                SetFocus(GetDlgItem(hwnd, cmb3));
            }
            else
            {
                SetDlgItemText(hwnd, cmb3, NULL);
            }
        }
        break;
    case chx3:
        if (codeNotify == BN_CLICKED)
        {
            if (IsDlgButtonChecked(hwnd, chx3) == BST_CHECKED)
            {
                SendDlgItemMessage(hwnd, cmb4, EM_SETSEL, 0, -1);
                SetFocus(GetDlgItem(hwnd, cmb4));
            }
            else
            {
                SetDlgItemText(hwnd, cmb4, NULL);
            }
        }
        break;
    case chx4:
        if (codeNotify == BN_CLICKED)
        {
            if (IsDlgButtonChecked(hwnd, chx4) == BST_CHECKED)
            {
                SendDlgItemMessage(hwnd, cmb5, EM_SETSEL, 0, -1);
                SetFocus(GetDlgItem(hwnd, cmb5));
            }
            else
            {
                SetDlgItemText(hwnd, cmb5, NULL);
            }
        }
        break;
    case chx5:
        if (codeNotify == BN_CLICKED)
        {
            if (IsDlgButtonChecked(hwnd, chx5) != BST_CHECKED)
            {
                SendDlgItemMessage(hwnd, cmb6, CB_SETCURSEL, 0, 0);
            }
            SetFocus(GetDlgItem(hwnd, cmb6));
        }
        break;
    }
}

static void OnUser100(HWND hwndNotify, HWND hwnd, INT nCode)
{
    TCHAR szText[MAX_PATH * 3];
    DWORD tid, pid;
    TCHAR cls[MAX_PATH], txt[MAX_PATH];

    tid = GetWindowThreadProcessId(hwnd, &pid);
    GetClassName(hwnd, cls, MAX_PATH);
    GetWindowText(hwnd, txt, MAX_PATH);

    switch (nCode)
    {
    case HCBT_ACTIVATE:
        StringCbPrintf(szText, sizeof(szText),
            TEXT("HCBT_ACTIVATE(hwnd:%p, pid:%lu (0x%lX), tid:%lu (0x%lX), cls:'%s', txt:'%s')\r\n"),
            hwnd, pid, pid, tid, tid, cls, txt);
        break;
    case HCBT_CREATEWND:
        StringCbPrintf(szText, sizeof(szText),
            TEXT("HCBT_CREATEWND(hwnd:%p, pid:%lu (0x%lX), tid:%lu (0x%lX), cls:'%s', txt:'%s')\r\n"),
            hwnd, pid, pid, tid, tid, cls, txt);
        break;
    case HCBT_DESTROYWND:
        StringCbPrintf(szText, sizeof(szText),
            TEXT("HCBT_DESTROYWND(hwnd:%p, pid:%lu (0x%lX), tid:%lu (0x%lX), cls:'%s', txt:'%s')\r\n"),
            hwnd, pid, pid, tid, tid, cls, txt);
        break;
    case HCBT_MINMAX:
        StringCbPrintf(szText, sizeof(szText),
            TEXT("HCBT_MINMAX(hwnd:%p, pid:%lu (0x%lX), tid:%lu (0x%lX), cls:'%s', txt:'%s')\r\n"),
            hwnd, pid, pid, tid, tid, cls, txt);
        break;
    case HCBT_MOVESIZE:
        StringCbPrintf(szText, sizeof(szText),
            TEXT("HCBT_MOVESIZE(hwnd:%p, pid:%lu (0x%lX), tid:%lu (0x%lX), cls:'%s', txt:'%s')\r\n"),
            hwnd, pid, pid, tid, tid, cls, txt);
        break;
    case HCBT_SETFOCUS:
        StringCbPrintf(szText, sizeof(szText),
            TEXT("HCBT_SETFOCUS(hwnd:%p, pid:%lu (0x%lX), tid:%lu (0x%lX), cls:'%s', txt:'%s')\r\n"),
            hwnd, pid, pid, tid, tid, cls, txt);
        break;
    }

    DoAddText(hwnd, szText);
}

INT_PTR CALLBACK
DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
    case WM_USER + 100:
        OnUser100(hwnd, reinterpret_cast<HWND>(wParam), LOWORD(lParam));
        break;
    }
    return 0;
}

INT WINAPI
WinMain(HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       lpCmdLine,
        INT         nCmdShow)
{
    s_hInst = hInstance;
    InitCommonControls();
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, DialogProc);
    return 0;
}