// AliasPlugin.cpp : �A�v���P�[�V�����̃G���g�� �|�C���g���`���܂��B
//

#include "stdafx.h"
#include "AliasPlugin.h"
#include "AliasController.h"
#include "Miscellaneous.h"
#include "I4C3DCommon.h"
#include <WinSock2.h>
#include <ShellAPI.h>

#define MAX_LOADSTRING	100
#define TIMER_ID		1

static const int BUFFER_SIZE = 256;
static const PCSTR COMMAND_INIT	= "init";
static const PCSTR COMMAND_EXIT	= "exit";

// �O���[�o���ϐ�:
HINSTANCE hInst;								// ���݂̃C���^�[�t�F�C�X
TCHAR szTitle[MAX_LOADSTRING];					// �^�C�g�� �o�[�̃e�L�X�g
TCHAR szWindowClass[MAX_LOADSTRING];			// ���C�� �E�B���h�E �N���X��
static USHORT g_uPort = 0;

// ���̃R�[�h ���W���[���Ɋ܂܂��֐��̐錾��]�����܂�:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

static int AnalyzeMessage(LPCSTR szMessage, LPSTR szCommand, SIZE_T size, double* pDeltaX, double* pDeltaY, char cTermination);

static SOCKET InitializeController(HWND hWnd, USHORT uPort);
static void UnInitializeController(SOCKET socketHandler);

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: �����ɃR�[�h��}�����Ă��������B
	MSG msg;
	HACCEL hAccelTable;

	// �O���[�o������������������Ă��܂��B
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_ALIASPLUGIN, szWindowClass, MAX_LOADSTRING);

	if (!ExecuteOnce(szTitle)) {
		return EXIT_SUCCESS;
	}

	MyRegisterClass(hInstance);

	//int argc = 0;
	//LPTSTR *argv = NULL;
	//argv = CommandLineToArgvW(GetCommandLine(), &argc);
	//if (argc != 2) {
	//	MessageBox(NULL, _T("[ERROR] ����������܂���[��: AliasPlugin.exe 10001]�B<AliasPlugin>"), szTitle, MB_OK | MB_ICONERROR);
	//}
	//g_uPort = static_cast<USHORT>(_wtoi(argv[1]));
	//OutputDebugString(argv[1]);
	//LocalFree(argv);
	g_uPort = 10003;

	static WSAData wsaData;
	WORD wVersion;
	int nResult;

	wVersion = MAKEWORD(2,2);
	nResult = WSAStartup(wVersion, &wsaData);
	if (nResult != 0) {
		MessageBox(NULL, _T("[ERROR] Initialize Winsock."), szTitle, MB_OK | MB_ICONERROR);
		return FALSE;
	}
	if (wsaData.wVersion != wVersion) {
		MessageBox(NULL, _T("[ERROR] Winsock �o�[�W����."), szTitle, MB_OK | MB_ICONERROR);
		WSACleanup();
		return FALSE;
	}

#if _DEBUG || DEBUG
	LogInitialize(Log_Debug);
#else
	LogInitialize(Log_Error);
#endif

	// �A�v���P�[�V�����̏����������s���܂�:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_ALIASPLUGIN));

	// ���C�� ���b�Z�[�W ���[�v:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	WSACleanup();
	CleanupMutex();
	return (int) msg.wParam;
}



//
//  �֐�: MyRegisterClass()
//
//  �ړI: �E�B���h�E �N���X��o�^���܂��B
//
//  �R�����g:
//
//    ���̊֐�����юg�����́A'RegisterClassEx' �֐����ǉ����ꂽ
//    Windows 95 ���O�� Win32 �V�X�e���ƌ݊�������ꍇ�ɂ̂ݕK�v�ł��B
//    �A�v���P�[�V�������A�֘A�t����ꂽ
//    �������`���̏������A�C�R�����擾�ł���悤�ɂ���ɂ́A
//    ���̊֐����Ăяo���Ă��������B
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ALIASPLUGIN));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_ALIASPLUGIN);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   �֐�: InitInstance(HINSTANCE, int)
//
//   �ړI: �C���X�^���X �n���h����ۑ����āA���C�� �E�B���h�E���쐬���܂��B
//
//   �R�����g:
//
//        ���̊֐��ŁA�O���[�o���ϐ��ŃC���X�^���X �n���h����ۑ����A
//        ���C�� �v���O���� �E�B���h�E���쐬����ѕ\�����܂��B
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // �O���[�o���ϐ��ɃC���X�^���X�������i�[���܂��B

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   //ShowWindow(hWnd, nCmdShow);
   //UpdateWindow(hWnd);

   return TRUE;
}

//
//  �֐�: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  �ړI:  ���C�� �E�B���h�E�̃��b�Z�[�W���������܂��B
//
//  WM_COMMAND	- �A�v���P�[�V���� ���j���[�̏���
//  WM_PAINT	- ���C�� �E�B���h�E�̕`��
//  WM_DESTROY	- ���~���b�Z�[�W��\�����Ė߂�
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	const int timer_interval = 100;
	static DWORD counter = 0;
	static BOOL doCount = FALSE;

	static AliasController controller;
	static SOCKET socketHandler = INVALID_SOCKET;
	char buffer[BUFFER_SIZE*2] = {0};
	char szCommand[BUFFER_SIZE] = {0};
	double deltaX = 0;
	double deltaY = 0;
	static char cTermination = '?';

	int nBytes = 0;
	TCHAR szError[BUFFER_SIZE] = {0};

	switch (message)
	{
	case WM_CREATE:
		socketHandler = InitializeController(hWnd, g_uPort);
		SetTimer(hWnd, TIMER_ID, timer_interval, NULL);
		break;

	case MY_WINSOCKSELECT:
		switch (WSAGETSELECTEVENT(lParam)) {
		case FD_READ:
			nBytes = recv(socketHandler, buffer, _countof(buffer), 0);
			if (nBytes == SOCKET_ERROR) {
				_stprintf_s(szError, _countof(szError), _T("recv() : %d <AliasPlugin>"), WSAGetLastError());
				ReportError(szError);
				break;

			}

			int scanCount = AnalyzeMessage(buffer, szCommand, _countof(szCommand), &deltaX, &deltaY, cTermination);
			if (scanCount == 3) {
				counter = 0;
				controller.Execute(szCommand, deltaX, deltaY);
				doCount = TRUE;

			} else if (scanCount == 1) {
				if (_strcmpi(szCommand, COMMAND_INIT) == 0) {
					if (!controller.Initialize(buffer, &cTermination)) {
						_stprintf_s(szError, _countof(szError), _T("Alias�R���g���[���̏������Ɏ��s���Ă��܂��B"));
						ReportError(szError);
					}
				} else if (_strcmpi(szCommand, COMMAND_EXIT) == 0) {
					OutputDebugString(_T("exit\n"));
					DestroyWindow(hWnd);
				}
			}
		}
		break;

	case WM_TIMER:
		if (doCount) {
			counter += timer_interval;
			if (300 < counter) {
				controller.ModKeyUp();
				doCount = FALSE;
			}
		}
		break;

	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// �I�����ꂽ���j���[�̉��:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;

	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: �`��R�[�h�������ɒǉ����Ă�������...
		EndPaint(hWnd, &ps);
		break;

	case MY_I4C3DREBOOT:
		UnInitializeController(socketHandler);
		socketHandler = InitializeController(hWnd, g_uPort);
		break;

	case MY_I4C3DDESTROY:
	case WM_DESTROY:
		UnInitializeController(socketHandler);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// �o�[�W�������{�b�N�X�̃��b�Z�[�W �n���h���[�ł��B
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

SOCKET InitializeController(HWND hWnd, USHORT uPort)
{
	SOCKET socketHandler;
	SOCKADDR_IN address;
	TCHAR szError[BUFFER_SIZE];
	int nResult = 0;

	socketHandler = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socketHandler == INVALID_SOCKET) {
		_stprintf_s(szError, _countof(szError), _T("[ERROR] socket() : %d"), WSAGetLastError());
		ReportError(szError);
		LogDebugMessage(Log_Error, szError);
		return INVALID_SOCKET;
	}

	address.sin_family = AF_INET;
	address.sin_port = htons(uPort);
	address.sin_addr.S_un.S_addr = INADDR_ANY;

	nResult = bind(socketHandler, (const SOCKADDR*)&address, sizeof(address));
	if (nResult == SOCKET_ERROR) {
		_stprintf_s(szError, _countof(szError), _T("[ERROR] bind() : %d"), WSAGetLastError());
		ReportError(szError);
		LogDebugMessage(Log_Error, szError);
		closesocket(socketHandler);
		return INVALID_SOCKET;
	}

	if (WSAAsyncSelect(socketHandler, hWnd, MY_WINSOCKSELECT, FD_READ) == SOCKET_ERROR) {
		TCHAR* szError = _T("�\�P�b�g�C�x���g�ʒm�ݒ�Ɏ��s���܂����B<AliasPlugin::InitializeController>");
		ReportError(szError);
		LogDebugMessage(Log_Error, szError);
	}

	return socketHandler;
}

void UnInitializeController(SOCKET socketHandler)
{
	closesocket(socketHandler);
}

int AnalyzeMessage(LPCSTR szMessage, LPSTR szCommand, SIZE_T size, double* pDeltaX, double* pDeltaY, char cTermination)
{
	static char szFormat[BUFFER_SIZE/4] = {0};
	int scanCount = 0;
	double deltaX = 0., deltaY = 0.;
	if (szFormat[0] == '\0') {
		sprintf_s(szFormat, sizeof(szFormat), "%%s %%lf %%lf%c", cTermination);
	}
	
	scanCount = sscanf_s(szMessage, szFormat, szCommand, size, &deltaX, &deltaY);
	if (3 <= scanCount) {
		*pDeltaX = deltaX;
		*pDeltaY = deltaY;
	}
	return scanCount;
}
