#include <windows.h>
#include <process.h>

#define ID_EDIT     (1)

#define MAX_DATA_TO_WRITE_LENGTH    (256u)

#define MAX_UART_COM_PORTS      (16u)
#define MAX_UART_THREAD_ERR     (16u)

#define WM_USER_THREAD_MSG      (WM_USER + 1u)
#define WM_UART_RX_CALLBACK     (WM_USER + 2u)

struct tThreadParams
{
    HANDLE event;
    HWND hwnd;
};

enum tIdmItem
{
    IDM_BASE = 40000u,
    IDM_APP_EXIT,
    IDM_PROM_READ_OUT,
    IDM_PROM_WRITE
};

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
static HMENU PromToolMenu(void);

static VOID InitUartDCB(DCB *dcb);
static DWORD WINAPI UartThread(LPVOID pvoid);
static void UartCallback(HWND hwnd, BYTE *pData, DWORD length);
static void UartReport(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
static void EditPrintf(HWND hwndEdit, TCHAR *szFormat, ...);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow)
{
    static const TCHAR szAppName[] = TEXT("PromTool");
    HWND hwnd = NULL;
    MSG msg;
    WNDCLASS wndclass;

    hPrevInstance = hPrevInstance;
    szCmdLine = szCmdLine;

    wndclass.style = (CS_HREDRAW | CS_VREDRAW);
    wndclass.lpfnWndProc = &WndProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = hInstance;
    wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = &szAppName[0];

    if (0 == RegisterClass(&wndclass))
    {
        (void)MessageBox(NULL, TEXT("Error in RegisterClass."), &szAppName[0], MB_ICONERROR);
        return 0;
    }
    else
    {
        /* RegisterClass's return value is ok */
    }

    hwnd = CreateWindow(
            &szAppName[0],
            &szAppName[0],
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            NULL,
            PromToolMenu(),
            hInstance,
            NULL);

    if (NULL == hwnd)
    {
        (void)MessageBox(NULL, TEXT("Error in CreateWindow."), &szAppName[0], MB_ICONERROR);
        return 0;
    }
    else
    {
        /* CreateWindow's return value is ok */
    }

    ShowWindow(hwnd, iCmdShow);

    if (FALSE == UpdateWindow(hwnd))
    {
        (void)MessageBox(NULL, TEXT("Error in UpdateWindow."), &szAppName[0], MB_ICONERROR);
        return 0;
    }
    else
    {
        /* UpdateWindow's return value is ok */
    }

    while (0 != GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}

static HMENU PromToolMenu(void)
{
    HMENU hMenu = CreateMenu();
    HMENU hMenuPopup = CreateMenu();

    (void)AppendMenu(hMenuPopup, MF_SEPARATOR, 0, NULL);
    (void)AppendMenu(hMenuPopup, MF_STRING, IDM_APP_EXIT, TEXT("&Exit"));

    (void)AppendMenu(hMenu, MF_POPUP, (UINT)hMenuPopup, TEXT("&File"));

    hMenuPopup = CreateMenu();

    (void)AppendMenu(hMenuPopup, MF_SEPARATOR, 0, NULL);

    (void)AppendMenu(hMenu, MF_POPUP, (UINT)hMenuPopup, TEXT("&Edit"));

    hMenuPopup = CreateMenu();

    (void)AppendMenu(hMenuPopup, MF_STRING, IDM_PROM_READ_OUT, TEXT("&Read out"));
    (void)AppendMenu(hMenuPopup, MF_STRING, IDM_PROM_WRITE, TEXT("&Write"));

    (void)AppendMenu(hMenu, MF_POPUP, (UINT)hMenuPopup, TEXT("&Command"));

    return hMenu;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static const TCHAR szDataToWriteFileName[] = TEXT("DataToWrite.bin");
    static HANDLE hFileLog = INVALID_HANDLE_VALUE;
    static DWORD ValidPacketCnt = 0u;
    static DWORD UartThreadId = 0;
    static HANDLE hUartThread = NULL;
    static HWND hwndEdit = NULL;

    struct tThreadParams ThreadParams = { 0, 0 };
    DWORD Length = 0;
    LPVOID pDataPacket = NULL;
    DWORD WrittenResult = 0u;
    HMENU hMenu = NULL;

    switch (message)
    {
        case WM_CREATE:
            {
                ValidPacketCnt = 0u;

                ThreadParams.hwnd = hwnd;
                ThreadParams.event = CreateEvent(NULL, FALSE, FALSE, NULL);

                hUartThread = CreateThread(NULL, 0, &UartThread, (LPVOID)&ThreadParams, 0, &UartThreadId);

                if (NULL == hUartThread)
                {
                    /* failed creating thread */
                }
                else
                {
                    /* thread created, wait for thread creating message queue */
                    WaitForSingleObject(ThreadParams.event, INFINITE);
                }

                hwndEdit = CreateWindow(
                        TEXT("edit"),
                        NULL,
                        (WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | WS_BORDER | \
                         ES_READONLY | ES_LEFT | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL),
                        0,
                        0,
                        0,
                        0,
                        hwnd,
                        (HMENU)ID_EDIT,
                        ((LPCREATESTRUCT)lParam)->hInstance,
                        NULL);

                if (NULL == hwndEdit)
                {
                    (void)MessageBox(NULL, TEXT("Error: hwndEdit == NULL."), TEXT("WndProc"), MB_ICONERROR);
                }

                return 0;
            }

        case WM_SETFOCUS:
            {
                SetFocus(hwndEdit);
                return 0;
            }

        case WM_SIZE:
            {
                MoveWindow(hwndEdit, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
                return 0;
            }

        case WM_USER_THREAD_MSG:
            {
                BOOL err = (BOOL)wParam;
                TCHAR *pName = (TCHAR *)lParam;

                if (FALSE == err)
                {
                    /* no error occured */
                    if (NULL == pName)
                    {
                        /* no message from communication thread */
                    }
                    else
                    {
                        EditPrintf(hwndEdit, TEXT("Communication message: %s"), pName);
                    }
                }
                else
                {
                    /* error occured */
                    if (NULL == pName)
                    {
                        EditPrintf(hwndEdit, TEXT("Communication error."));
                    }
                    else
                    {
                        EditPrintf(hwndEdit, TEXT("Communication error: %s"), pName);
                    }
                }

                if (NULL != pName)
                {
                    HeapFree(GetProcessHeap(), 0, pName);
                }

                return 0;
            }

        case WM_UART_RX_CALLBACK:
            {
                Length = (DWORD)wParam;
                pDataPacket = (LPVOID)lParam;

                hMenu = GetMenu(hwnd);
                EnableMenuItem(hMenu, IDM_PROM_READ_OUT, MF_ENABLED);
                EnableMenuItem(hMenu, IDM_PROM_WRITE, MF_ENABLED);

                if (((0 == Length) || (NULL == pDataPacket)))
                {
                    /* no data */
                }
                else
                {
                    if (FALSE == WriteFile(hFileLog, pDataPacket, Length, &WrittenResult, NULL))
                    {
                        EditPrintf(hwndEdit, TEXT("Error writing data to file."));
                    }

                    HeapFree(GetProcessHeap(), 0, pDataPacket);
                    CloseHandle(hFileLog);
                    hFileLog = INVALID_HANDLE_VALUE;
                    EditPrintf(hwndEdit, TEXT("Read out complete."));
                }

                return 0;
            }

        case WM_COMMAND:
            {
                enum tIdmItem IdmItem = (enum tIdmItem)LOWORD(wParam);

                /* check for errors from edit field */
                if (ID_EDIT == LOWORD(wParam))
                {
                    if ((EN_ERRSPACE == HIWORD(wParam)) || (EN_MAXTEXT == HIWORD(wParam)))
                    {
                        /* edit window has no RAM and reports error */
                        /* do nothing here */
                    }
                }

                /* check, what command is selected by user */
                switch (IdmItem)
                {
                    case IDM_BASE:
                        {
                            break;
                        }

                    case IDM_APP_EXIT:
                        {
                            PostMessage(hwnd, WM_CLOSE, 0, 0);
                            break;
                        }

                    case IDM_PROM_READ_OUT:
                        {
                            ValidPacketCnt = 0;

                            EditPrintf(hwndEdit, TEXT("Selected Command: Read out contents of ROM."));

                            if (INVALID_HANDLE_VALUE == hFileLog)
                            {
                                /* handle already invalid, do nothing */
                            }
                            else
                            {
                                CloseHandle(hFileLog);
                            }

                            hFileLog = CreateFile(
                                    TEXT("readout.bin"),
                                    GENERIC_WRITE,
                                    FILE_SHARE_WRITE,
                                    NULL,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL,
                                    NULL);

                            if (INVALID_HANDLE_VALUE == hFileLog)
                            {
                                EditPrintf(hwndEdit, TEXT("Could not create file for writing read out data."));
                            }
                            else if (FALSE == PostThreadMessage(UartThreadId, (UINT)IDM_PROM_READ_OUT, 0, 0))
                            {
                                EditPrintf(hwndEdit, TEXT("Could not send READ OUT command to PROM device."));
                            }
                            else
                            {
                                hMenu = GetMenu(hwnd);
                                EnableMenuItem(hMenu, IDM_PROM_READ_OUT, MF_GRAYED);
                                EnableMenuItem(hMenu, IDM_PROM_WRITE, MF_GRAYED);
                            }
                            break;
                        }

                    case IDM_PROM_WRITE:
                        {
                            HANDLE hDataToWrite = INVALID_HANDLE_VALUE;
                            const DWORD BytesToRead = 131072u;
                            DWORD BytesRead = 0u;
                            PVOID pBuffer = NULL;

                            EditPrintf(hwndEdit, TEXT("Selected command: Write contents of file %s to the EPROM."), &szDataToWriteFileName[0]);

                            /* Allocate buffer for reading the file */
                            pBuffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, BytesToRead);
                            if (NULL == pBuffer)
                            {
                                EditPrintf(hwndEdit, TEXT("Error. Could not allocate %d bytes."), BytesToRead);
                                /* No sense to continue, break this case */
                                break;
                            }

                            /* Open file for reading */
                            hDataToWrite = CreateFile(
                                    &szDataToWriteFileName[0],
                                    GENERIC_READ,
                                    FILE_SHARE_WRITE,
                                    NULL,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    NULL);


                            if (INVALID_HANDLE_VALUE == hDataToWrite)
                            {
                                EditPrintf(hwndEdit, TEXT("Error. Could not open file %s."), &szDataToWriteFileName[0]);
                                HeapFree(GetProcessHeap(), 0, pBuffer);
                                break;
                            }
                            else if (FALSE == ReadFile(hDataToWrite, pBuffer, BytesToRead, &BytesRead, NULL))
                            {
                                EditPrintf(hwndEdit, TEXT("Error. Could not read from file %s."), &szDataToWriteFileName[0]);
                                HeapFree(GetProcessHeap(), 0, pBuffer);
                                CloseHandle(hDataToWrite);
                                break;
                            }
                            else
                            {
                                /* File can be closed now, it is not used any more */
                                CloseHandle(hDataToWrite);

                                if (BytesToRead != BytesRead)
                                {
                                    EditPrintf(hwndEdit, TEXT("Error. Could not read %u bytes from file %s."), BytesToRead, &szDataToWriteFileName[0]);
                                    HeapFree(GetProcessHeap(), 0, pBuffer);
                                    break;
                                }
                                else if (FALSE == PostThreadMessage(UartThreadId, (UINT)IDM_PROM_WRITE, (WPARAM)BytesToRead, (LPARAM)pBuffer))
                                {
                                    EditPrintf(hwndEdit, TEXT("Could not send WRITE command to PROM device."));
                                    HeapFree(GetProcessHeap(), 0, pBuffer);
                                    break;
                                }
                                else
                                {
                                    /* Everything seems to be ok here. */
                                    /* Do not free the buffer here, it must be done by the UART thread... */

                                    hMenu = GetMenu(hwnd);
                                    EnableMenuItem(hMenu, IDM_PROM_READ_OUT, MF_GRAYED);
                                    EnableMenuItem(hMenu, IDM_PROM_WRITE, MF_GRAYED);

                                    EditPrintf(hwndEdit, TEXT("File %s opened and read. Writing data."), &szDataToWriteFileName[0]);
                                }
                            }

                            break;
                        }

                    default:
                        {
                            break;
                        }
                }

                return 0;
            }

        case WM_DESTROY:
            {
                if (INVALID_HANDLE_VALUE == hFileLog)
                {
                    /* Log-file already closed */
                }
                else
                {
                    CloseHandle(hFileLog);
                    hFileLog = INVALID_HANDLE_VALUE;
                }

                PostQuitMessage(0);

                return 0;
            }

        default:
            {
                /* no action here */
                break;
            }
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

static VOID InitUartDCB(DCB *dcb)
{
    dcb[0].DCBlength = sizeof(DCB);
    dcb[0].BaudRate = CBR_19200;
    dcb[0].fBinary = TRUE;
    dcb[0].fParity = FALSE;
    dcb[0].fOutxCtsFlow = FALSE;
    dcb[0].fOutxDsrFlow = FALSE;
    dcb[0].fDtrControl = DTR_CONTROL_DISABLE;
    dcb[0].fDsrSensitivity = FALSE;
    dcb[0].fTXContinueOnXoff = FALSE;
    dcb[0].fOutX = FALSE;
    dcb[0].fInX = FALSE;
    dcb[0].fErrorChar = FALSE;
    dcb[0].fNull = FALSE;
    dcb[0].fRtsControl = RTS_CONTROL_DISABLE;
    dcb[0].fAbortOnError = FALSE;
    dcb[0].fDummy2 = 0;
    dcb[0].wReserved = 0;
    dcb[0].XonLim = 0;
    dcb[0].XoffLim = 0;
    dcb[0].ByteSize = 8u;
    dcb[0].Parity = NOPARITY;
    dcb[0].StopBits = ONESTOPBIT;
    dcb[0].XonChar = 0;
    dcb[0].XoffChar = 0;
    dcb[0].ErrorChar = 0;
    dcb[0].EofChar = 0;
    dcb[0].EvtChar = 0;
    dcb[0].wReserved1 = 0;

    return;
}

static DWORD WINAPI UartThread(LPVOID pvoid)
{
    TCHAR szPortName[256u] = { 0 };
    DCB dcb = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    struct tThreadParams params = { 0, 0 };
    BYTE Command[2u];
    HANDLE hFileCom = INVALID_HANDLE_VALUE;
    MSG msg;
    enum tIdmItem PromCmd = IDM_BASE;
    BOOL done = FALSE;
    TCHAR* pMsg = NULL;
    BYTE* pDataToRead = NULL;
    DWORD cbDataToRead = 0;
    const DWORD comCnt = 7u;
    DWORD cbWritten = 0u;
    DWORD cbRead = 0;

    memcpy(&params, pvoid, sizeof(struct tThreadParams));

    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    SetEvent(params.event);

    /* generate COM device name */
    memset(&szPortName[0], 0, sizeof(szPortName));
    wsprintf(&szPortName[0], TEXT("\\\\.\\COM%u"), comCnt);

    /* try open it */
    hFileCom = CreateFile(
            &szPortName[0],
            (GENERIC_WRITE | GENERIC_READ),
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

    if (INVALID_HANDLE_VALUE == hFileCom)
    {
        pMsg = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 256u);
        if (NULL != pMsg)
        {
            wsprintf(pMsg, TEXT("Could not open COM port %d."), comCnt);
            UartReport(params.hwnd, WM_USER_THREAD_MSG, TRUE, (LPARAM)pMsg);
        }
    }
    else
    {
        ZeroMemory(&dcb, sizeof(dcb));
        InitUartDCB(&dcb);

        if (0 == SetCommState(hFileCom, &dcb))
        {
            CloseHandle(hFileCom);
            hFileCom = INVALID_HANDLE_VALUE;

            pMsg = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 256u);
            if (NULL != pMsg)
            {
                wsprintf(pMsg, TEXT("Could not initialize COM port %s."), &szPortName[0]);
                UartReport(params.hwnd, WM_USER_THREAD_MSG, TRUE, (LPARAM)pMsg);
            }
        }
        else
        {
            /* SetCommState's return value is ok */
            pMsg = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 256u);
            if (NULL != pMsg)
            {
                wsprintf(pMsg, TEXT("Initialization successfull for %s."), &szPortName[0]);
                UartReport(params.hwnd, WM_USER_THREAD_MSG, FALSE, (LPARAM)pMsg);
            }
        }
    }

    while (FALSE == done)
    {
        if (FALSE == PeekMessage(&msg, (HWND)-1, 0, 0, PM_REMOVE))
        {
            /* no message available, do nothing */
            Sleep(10);
        }
        else
        {
            PromCmd = (enum tIdmItem)(msg.message);

            switch (PromCmd)
            {
                case IDM_BASE:
                    {
                        break;
                    }

                case IDM_PROM_READ_OUT:
                    {
                        Command[0u] = 0xAAu;
                        Command[1u] = 0xAAu;

                        if (FALSE == WriteFile(hFileCom, &Command[0], 2u, &cbWritten, NULL))
                        {
                            /* Failed writing command over UART */
                            /* Report error message */
                            pMsg = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 256u);
                            if (NULL != pMsg)
                            {
                                wsprintf(pMsg, TEXT("Could not send READ OUT command."));
                                UartReport(params.hwnd, WM_USER_THREAD_MSG, TRUE, (LPARAM)pMsg);
                            }
                            UartCallback(params.hwnd, NULL, 0u);
                        }
                        else
                        {
                            /* No errors */
                            cbDataToRead = (2u * 65536u);
                            pDataToRead = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbDataToRead);

                            if (NULL == pDataToRead)
                            {
                                /* Failed allocating memory, try to report error message */
                                pMsg = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 256u);
                                if (NULL != pMsg)
                                {
                                    wsprintf(pMsg, TEXT("Could not allocate memory."));
                                    UartReport(params.hwnd, WM_USER_THREAD_MSG, TRUE, (LPARAM)pMsg);
                                }
                                UartCallback(params.hwnd, NULL, 0u);
                            }
                            else
                            {
                                /* Memory allocated, read data from UART */
                                if (FALSE == ReadFile(hFileCom, pDataToRead, cbDataToRead, &cbRead, NULL))
                                {
                                    /* Failed reading data from UART */
                                    pMsg = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 256u);
                                    if (NULL != pMsg)
                                    {
                                        wsprintf(pMsg, TEXT("Failed reading data from UART."));
                                        UartReport(params.hwnd, WM_USER_THREAD_MSG, TRUE, (LPARAM)pMsg);
                                    }
                                    UartCallback(params.hwnd, NULL, 0u);
                                    HeapFree(GetProcessHeap(), 0, pDataToRead);
                                }
                                else if (cbDataToRead != cbRead)
                                {
                                    /* Not enough data received */
                                    pMsg = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 256u);
                                    if (NULL != pMsg)
                                    {
                                        wsprintf(pMsg, TEXT("Not enough data received: only %d bytes."), cbRead);
                                        UartReport(params.hwnd, WM_USER_THREAD_MSG, TRUE, (LPARAM)pMsg);
                                    }
                                    UartCallback(params.hwnd, NULL, 0u);
                                    HeapFree(GetProcessHeap(), 0, pDataToRead);
                                }
                                else
                                {
                                    pMsg = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 256u);
                                    if (NULL != pMsg)
                                    {
                                        wsprintf(pMsg, TEXT("No errors. %d bytes received."), cbDataToRead);
                                        UartReport(params.hwnd, WM_USER_THREAD_MSG, FALSE, (LPARAM)pMsg);
                                    }
                                    /* No errors receiving data from UART */
                                    UartCallback(params.hwnd, pDataToRead, cbDataToRead);
                                }
                            }
                        }

                        break;
                    }

                case IDM_PROM_WRITE:
                    {
                        DWORD i = 0u;
                        const DWORD cbDataToWrite = (DWORD)msg.wParam;
                        const BYTE* const pDataToWrite = (BYTE*)msg.lParam;

                        Command[0u] = 0xBBu;
                        Command[1u] = 0xBBu;

                        if (FALSE == WriteFile(hFileCom, &Command[0], 2u, &cbWritten, NULL))
                        {
                            /* Failed writing command over UART */
                            /* Report error message */
                            pMsg = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 256u);
                            if (NULL != pMsg)
                            {
                                wsprintf(pMsg, TEXT("Could not send WRITE command."));
                                UartReport(params.hwnd, WM_USER_THREAD_MSG, TRUE, (LPARAM)pMsg);
                            }
                            UartCallback(params.hwnd, NULL, 0u);
                        }
                        else
                        {
                            /* No errors, send further data here */

                            /* This loop sends data (cbDataToWrite must be even) */
                            for (i = 0u; i < cbDataToWrite; i += 2u)
                            {
                                if (FALSE == WriteFile(hFileCom, &pDataToWrite[i], 2u, &cbWritten, NULL))
                                {
                                    /* Failed sending the bytes */
                                    pMsg = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 256u);
                                    if (NULL != pMsg)
                                    {
                                        wsprintf(pMsg, TEXT("Could not send data."));
                                        UartReport(params.hwnd, WM_USER_THREAD_MSG, TRUE, (LPARAM)pMsg);
                                    }
                                    UartCallback(params.hwnd, NULL, 0u);
                                    /* Break the loop */
                                    i = cbDataToWrite;
                                }
                                else
                                {
#if 1
                                    if (FALSE == FlushFileBuffers(hFileCom))
                                    {
                                        /* Failed flushing the bytes */
                                        pMsg = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 256u);
                                        if (NULL != pMsg)
                                        {
                                            wsprintf(pMsg, TEXT("Could not flush data."));
                                            UartReport(params.hwnd, WM_USER_THREAD_MSG, TRUE, (LPARAM)pMsg);
                                        }
                                        UartCallback(params.hwnd, NULL, 0u);
                                        /* Break the loop */
                                        i = cbDataToWrite;
                                    }
                                    else
                                    {
                                        Sleep(1u);
                                    }
#endif
                                }

                                /* Report current state */
                                if ((0u != i) && (0u == (i % 1024u)))
                                {
                                    pMsg = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 256u);
                                    if (NULL != pMsg)
                                    {
                                        wsprintf(pMsg, TEXT("%d of %d Bytes sent..."), i, cbDataToWrite);
                                        UartReport(params.hwnd, WM_USER_THREAD_MSG, FALSE, (LPARAM)pMsg);
                                    }
                                }
                            }
                        }

                        /* Report, sending data is ok */
                        pMsg = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 256u);
                        if (NULL != pMsg)
                        {
                            wsprintf(pMsg, TEXT("Ready. No errors."));
                            UartReport(params.hwnd, WM_USER_THREAD_MSG, FALSE, (LPARAM)pMsg);
                        }
                        UartCallback(params.hwnd, NULL, 0);

                        /* This thread is responsible for freeing the data buffer */
                        HeapFree(GetProcessHeap(), 0, (LPVOID)pDataToWrite);

                        break;
                    }

                default:
                    {
                        /* Report error message */
                        pMsg = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 256u);
                        if (NULL != pMsg)
                        {
                            wsprintf(pMsg, TEXT("Unknown command received."));
                            UartReport(params.hwnd, WM_USER_THREAD_MSG, TRUE, (LPARAM)pMsg);
                        }
                        UartCallback(params.hwnd, NULL, 0u);
                        break;
                    }
            }
        }
    }

    CloseHandle(hFileCom);

    ExitThread(0);

    return 0;
}

static void UartCallback(HWND hwnd, BYTE *pData, DWORD length)
{
    static const DWORD maxRetry = 3u;
    DWORD i = 0u;
    BOOL result = FALSE;

    for (i = 0u; i < maxRetry; ++i)
    {
        if (FALSE == result)
        {
            result = PostMessage(hwnd, WM_UART_RX_CALLBACK, (WPARAM)length, (LPARAM)pData);

            /* if posting failed, wait a bit before proceeding */
            if (FALSE == result)
            {
                Sleep(100);
            }
        }
    }

    if ((FALSE == result) && (NULL != pData))
    {
        HeapFree(GetProcessHeap(), 0, pData);
    }

    return;
}

static void UartReport(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static const DWORD maxRetry = 3u;
    DWORD i = 0u;
    BOOL result = FALSE;

    for (i = 0u; i < maxRetry; ++i)
    {
        if (FALSE == result)
        {
            result = PostMessage(hwnd, message, wParam, lParam);

            /* if posting failed, wait a bit before proceeding */
            if (FALSE == result)
            {
                (void)MessageBox(NULL, TEXT("PostMessage failed."), TEXT("UartReport"), MB_ICONERROR);
                Sleep(100);
            }
        }
    }

    if ((FALSE == result) && (0UL != lParam))
    {
        /* Posting still failed, free allocated memory in lParam */
        HeapFree(GetProcessHeap(), 0, (LPVOID)lParam);
    }

    return;
}

static void EditPrintf(HWND hwndEdit, TCHAR *szFormat, ...)
{
    static TCHAR szEndLine[] = TEXT("\r\n");
    TCHAR szBuffer[1024u];
    va_list pArgList;

    if (NULL != hwndEdit)
    {
        va_start(pArgList, szFormat);
        memset(&szBuffer[0], 0, sizeof(szBuffer));
        wvsprintf(&szBuffer[0], szFormat, pArgList);
        va_end(pArgList);

        SendMessage(hwndEdit, EM_SETSEL, (WPARAM)-1, (WPARAM)-1);
        SendMessage(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)&szBuffer[0]);
        SendMessage(hwndEdit, EM_SCROLLCARET, 0, 0);

        SendMessage(hwndEdit, EM_SETSEL, (WPARAM)-1, (WPARAM)-1);
        SendMessage(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)&szEndLine[0]);
        SendMessage(hwndEdit, EM_SCROLLCARET, 0, 0);
    }
}

