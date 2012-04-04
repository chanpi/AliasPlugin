#include "StdAfx.h"
#include "AliasController.h"
#include "I4C3DKeysHook.h"
#include "I4C3DCommon.h"
#include "Misc.h"
#include "SharedConstants.h"
#include <math.h>
#include <float.h>

#if UNICODE || _UNICODE
static LPCTSTR g_FILE = __FILEW__;
#else
static LPCTSTR g_FILE = __FILE__;
#endif

extern const int BUFFER_SIZE = 256;

static BOOL CALLBACK EnumChildProcForMouseInput(HWND hWnd, LPARAM lParam);

static const PCTSTR g_szChildWindowTitle	= _T("Alias.glw");

const PCSTR COMMAND_TUMBLE	= "tumble";
const PCSTR COMMAND_TRACK	= "track";
const PCSTR COMMAND_DOLLY	= "dolly";

AliasController::AliasController(void)
{
	m_hTargetTopWnd		= NULL;
	m_hKeyInputWnd		= NULL;
	m_hMouseInputWnd	= NULL;
	m_currentPos.x		= 0;
	m_currentPos.y		= 0;
	m_DisplayWidth		= GetSystemMetrics(SM_CXSCREEN);
	m_DisplayHeight		= GetSystemMetrics(SM_CYSCREEN);
	m_fTumbleRate		= 0;
	m_fTrackRate		= 0;
	m_fDollyRate		= 0;
	m_bUsePostMessageToSendKey		= FALSE;
	m_bUsePostMessageToMouseDrag	= TRUE;
	m_ctrl = m_alt = m_shift = m_bSyskeyDown = FALSE;

	ZeroMemory(&m_mouseMessage, sizeof(m_mouseMessage));
	m_mouseMessage.dragButton = DragNONE;
	
	MakeHook(NULL);
	m_hKeyInputWnd	= NULL;
}

AliasController::~AliasController(void)
{
	ModKeyUp();
	UnHook();
}

/**
 * @brief
 * Controlオブジェクトの初期化を行います。
 * 
 * @returns
 * 初期化に成功した場合にはTRUE、失敗した場合にはFALSEを返します。
 * 
 * Controlオブジェクトの初期化を行います。
 * どのControlオブジェクトかは、引数のI4C3DContextポインタに登録されているControl派生クラスによります。
 * 
 * @remarks
 * InitializeModifierKeys()で修飾キーの設定を行います。
 * 
 * @see
 * InitializeModifierKeys()
 */
BOOL AliasController::Initialize(LPCSTR szBuffer, char* termination)
{
	char tmpCommand[BUFFER_SIZE] = {0};
	char szModKeys[BUFFER_SIZE] = {0};

	sscanf_s(szBuffer, g_initCommandFormat, tmpCommand,	sizeof(tmpCommand), szModKeys, sizeof(szModKeys), &m_fTumbleRate, &m_fTrackRate, &m_fDollyRate, termination, sizeof(*termination));
	if (fabs(m_fTumbleRate - 0.0) < DBL_EPSILON) {
		m_fTumbleRate = 1.0;
	}
	if (fabs(m_fTrackRate - 0.0) < DBL_EPSILON) {
		m_fTrackRate = 1.0;
	}
	if (fabs(m_fDollyRate - 0.0) < DBL_EPSILON) {
		m_fDollyRate = 1.0;
	}

	{
		TCHAR szBuf[32];
		_stprintf_s(szBuf, 32, _T("tum:%.2f, tra:%.2f dol:%.2f\n"), m_fTumbleRate, m_fTrackRate, m_fDollyRate);
		OutputDebugString(szBuf);
	}

	return InitializeModifierKeys(szModKeys);
}

/**
 * @brief
 * 3Dソフトで使用する修飾キーの取得、設定、およびキー押下の監視プログラムへの登録を行います。
 * 
 * @returns
 * 修飾キーの取得、設定、登録に成功した場合にはTRUE、失敗した場合にはFALSEを返します。
 * 
 * 3Dソフトで使用する修飾キーの取得、設定、およびキー押下の監視プログラムへの登録を行います。
 * 
 * @remarks
 * I4C3DKeysHook.dllのAddHookedKeyCode()でキーフックの登録を行います。
 * 
 * @see
 * AddHookedKeyCode()
 */
BOOL AliasController::InitializeModifierKeys(PCSTR szModifierKeys)
{
	if (_strcmpi(szModifierKeys, "NULL") == 0 || szModifierKeys == NULL) {
		m_alt = m_shift = TRUE;
		AddHookedKeyCode(VK_MENU);
		AddHookedKeyCode(VK_SHIFT);
		return TRUE;
	}

	PCSTR pType = NULL;
	do {
		char szKey[BUFFER_SIZE] = {0};
		pType = strchr(szModifierKeys, '+');
		if (pType != NULL) {
			strncpy_s(szKey, _countof(szKey), szModifierKeys, pType-szModifierKeys);
			szModifierKeys = pType+1;
		} else {
			strcpy_s(szKey, _countof(szKey), szModifierKeys);
		}
		RemoveWhiteSpaceA(szKey);
		switch (szKey[0]) {
		case _T('C'):
		case _T('c'):
			m_ctrl = TRUE;
			AddHookedKeyCode( VK_CONTROL );
			break;

		case _T('S'):
		case _T('s'):
			m_shift = TRUE;
			AddHookedKeyCode( VK_SHIFT );
			break;

		case _T('A'):
		case _T('a'):
			m_alt = TRUE;
			AddHookedKeyCode( VK_MENU );
			break;
		}
	} while (pType != NULL);

	return TRUE;
}


BOOL AliasController::GetTargetChildWnd(void)
{
	m_hKeyInputWnd = m_hTargetTopWnd;
	m_hMouseInputWnd = NULL;
	EnumChildWindows(m_hKeyInputWnd, EnumChildProcForMouseInput, (LPARAM)&m_hMouseInputWnd);
	if (m_hMouseInputWnd == NULL) {
		LoggingMessage(Log_Error, _T(MESSAGE_ERROR_WINDOW_MISSING), GetLastError(), g_FILE, __LINE__);	
		return FALSE;
	}
	return TRUE;
}

// コメントアウト 2011.06.10
// GetTargetChildWndとで二重チェックになってしまうため。
// GetTargetChildWndとAdjustCursorPosを使用
//BOOL AliasController::CheckTargetState(void)
//{
//	if (m_hTargetTopWnd == NULL) {
//		//ReportError(_T("ターゲットウィンドウが取得できません。<AliasController::CheckTargetState>"));
//		LogDebugMessage(Log_Error, _T("ターゲットウィンドウが取得できません。<AliasController::CheckTargetState>"));
//
//	} else if (m_hKeyInputWnd == NULL) {
//		LogDebugMessage(Log_Error, _T("キー入力ウィンドウが取得できません。<AliasController::CheckTargetState>"));
//
//	} else if (m_hMouseInputWnd == NULL) {
//		LogDebugMessage(Log_Error, _T("マウス入力ウィンドウが取得できません。<AliasController::CheckTargetState>"));
//
//	} else {
//		return TRUE;
//	}
//
//	return FALSE;
//}

void AliasController::AdjustCursorPos(int deltaX, int deltaY)
{
	// ターゲットウィンドウの位置のチェック
	POINT tmpCurrentPos = m_currentPos;
	ClientToScreen(m_hMouseInputWnd, &tmpCurrentPos);

	RECT windowRect;
	GetWindowRect(m_hMouseInputWnd, &windowRect);

	if (WindowFromPoint(tmpCurrentPos) != m_hMouseInputWnd ||
		tmpCurrentPos.x < windowRect.left+200 || windowRect.right-500 < tmpCurrentPos.x ||
		tmpCurrentPos.y < windowRect.top+200 || windowRect.bottom < tmpCurrentPos.y ||
		windowRect.right-500 < tmpCurrentPos.x+deltaX || tmpCurrentPos.y+deltaY < windowRect.top+200) {
			if (m_mouseMessage.dragButton != DragNONE) {
				VMMouseClick(&m_mouseMessage, TRUE);
				m_mouseMessage.dragButton = DragNONE;
			}

			RECT rect;
			GetClientRect(m_hMouseInputWnd, &rect);
			m_currentPos.x = rect.left + (rect.right - rect.left) / 2;
			m_currentPos.y = rect.top + (rect.bottom - rect.top) / 2;
	}
}

void AliasController::Execute(HWND hWnd, LPCSTR szCommand, double deltaX, double deltaY)
{
	m_hTargetTopWnd = hWnd;

	// 実際に仮想キー・仮想マウス操作を行う子ウィンドウの取得
	if (!GetTargetChildWnd()) {
		return;
	}

	//ShowWindow(m_hTargetParentWnd, SW_SHOWMAXIMIZED);

	if (_strcmpi(szCommand, COMMAND_TUMBLE) == 0) {
		ModKeyDown();
		if (m_bSyskeyDown) {
			TumbleExecute((INT)(deltaX * m_fTumbleRate), (INT)(deltaY * m_fTumbleRate));
		}

	} else if (_strcmpi(szCommand, COMMAND_TRACK) == 0) {
		ModKeyDown();
		if (m_bSyskeyDown) {
			TrackExecute((INT)(deltaX * m_fTrackRate), (INT)(deltaY * m_fTrackRate));
		}

	} else if (_strcmpi(szCommand, COMMAND_DOLLY) == 0) {
		ModKeyDown();
		if (m_bSyskeyDown) {
			DollyExecute((INT)(deltaX * m_fDollyRate), (INT)(deltaY * m_fDollyRate));
		}

	} else {
		ModKeyUp();
		PlayMacro(szCommand, m_hKeyInputWnd, m_bUsePostMessageToSendKey);
	}
}

void AliasController::TumbleExecute(int deltaX, int deltaY)
{
	if (m_mouseMessage.dragButton != LButtonDrag) {
		if (m_mouseMessage.dragButton != DragNONE) {
			VMMouseClick(&m_mouseMessage, TRUE);
			m_mouseMessage.dragButton = DragNONE;
		}
	}

	//if (!CheckTargetState()) {
	//	return;
	//}
	m_mouseMessage.bUsePostMessage	= m_bUsePostMessageToMouseDrag;
	m_mouseMessage.hTargetWnd		= m_hMouseInputWnd;

	m_mouseMessage.uKeyState		= MK_LBUTTON;
	if (m_ctrl) {
		m_mouseMessage.uKeyState |= MK_CONTROL;
	}
	if (m_shift) {
		m_mouseMessage.uKeyState |= MK_SHIFT;
	}

	if (m_mouseMessage.dragButton != LButtonDrag) {
		m_mouseMessage.dragButton = LButtonDrag;

		AdjustCursorPos(deltaX, deltaY);
		m_mouseMessage.dragStartPos		= m_currentPos;
		m_mouseMessage.dragEndPos.x		= m_currentPos.x + deltaX;
		m_mouseMessage.dragEndPos.y		= m_currentPos.y + deltaY;

		VMMouseClick(&m_mouseMessage, FALSE);
	}

	m_mouseMessage.dragStartPos		= m_currentPos;
	m_currentPos.x					+= deltaX;
	m_currentPos.y					+= deltaY;
	m_mouseMessage.dragEndPos		= m_currentPos;

	VMMouseMove(&m_mouseMessage);
}

void AliasController::TrackExecute(int deltaX, int deltaY)
{
	if (m_mouseMessage.dragButton != MButtonDrag) {
		if (m_mouseMessage.dragButton != DragNONE) {
			VMMouseClick(&m_mouseMessage, TRUE);
			m_mouseMessage.dragButton = DragNONE;
		}
	}
	//if (!CheckTargetState()) {
	//	return;
	//}
	m_mouseMessage.bUsePostMessage	= m_bUsePostMessageToMouseDrag;
	m_mouseMessage.hTargetWnd		= m_hMouseInputWnd;
	m_mouseMessage.uKeyState		= MK_MBUTTON;
	if (m_ctrl) {
		m_mouseMessage.uKeyState	|= MK_CONTROL;
	}
	if (m_shift) {
		m_mouseMessage.uKeyState	|= MK_SHIFT;
	}
	if (m_mouseMessage.dragButton != MButtonDrag) {
		m_mouseMessage.dragButton = MButtonDrag;

		AdjustCursorPos(deltaX, deltaY);
		m_mouseMessage.dragStartPos		= m_currentPos;
		m_mouseMessage.dragEndPos.x		= m_currentPos.x + deltaX;
		m_mouseMessage.dragEndPos.y		= m_currentPos.y + deltaY;

		VMMouseClick(&m_mouseMessage, FALSE);
	}

	m_mouseMessage.dragStartPos		= m_currentPos;
	m_currentPos.x					+= deltaX;
	m_currentPos.y					+= deltaY;
	m_mouseMessage.dragEndPos		= m_currentPos;

	VMMouseMove(&m_mouseMessage);
}

void AliasController::DollyExecute(int deltaX, int deltaY)
{
	if (m_mouseMessage.dragButton != RButtonDrag) {
		if (m_mouseMessage.dragButton != DragNONE) {
			VMMouseClick(&m_mouseMessage, TRUE);
			m_mouseMessage.dragButton = DragNONE;
		}
	}

	//if (!CheckTargetState()) {
	//	return;
	//}
	m_mouseMessage.bUsePostMessage	= m_bUsePostMessageToMouseDrag;
	m_mouseMessage.hTargetWnd		= m_hMouseInputWnd;

	m_mouseMessage.uKeyState		= MK_RBUTTON;
	if (m_ctrl) {
		m_mouseMessage.uKeyState	|= MK_CONTROL;
	}
	if (m_shift) {
		m_mouseMessage.uKeyState	|= MK_SHIFT;
	}
	if (m_mouseMessage.dragButton != RButtonDrag) {
		m_mouseMessage.dragButton = RButtonDrag;

		AdjustCursorPos(deltaX, deltaY);
		m_mouseMessage.dragStartPos		= m_currentPos;
		m_mouseMessage.dragEndPos.x		= m_currentPos.x + deltaX;
		m_mouseMessage.dragEndPos.y		= m_currentPos.y + deltaY;

		VMMouseClick(&m_mouseMessage, FALSE);
	}

	m_mouseMessage.dragStartPos		= m_currentPos;
	m_currentPos.x					+= deltaX;
	m_currentPos.y					+= deltaY;
	m_mouseMessage.dragEndPos		= m_currentPos;

	VMMouseMove(&m_mouseMessage);
}

/**
 * @brief
 * 登録した修飾キーが押されたか確認します。
 * 
 * @returns
 * 登録した修飾キーが押されている場合にはTRUE、そうでない場合はFALSEを返します。
 * 
 * 登録した修飾キーが押されたか確認します。
 * 押されていない場合は、Sleepします。
 * キーフックを利用してキー押下メッセージが発生したかどうかを調べています。
 * 対象プログラムでメッセージが処理される前のキー押下の判断です。
 * 
 * @remarks
 * I4C3DKeysHook.dllのIsAllKeysDown()関数でキー押下を確認します。
 * 
 * @see
 * IsAllKeysDown()
 */
BOOL AliasController::IsModKeysDown(void)
{
	int i = 0;
	for (i = 0; i < waitModkeyDownCount; ++i) {
		Sleep(1);
		if (m_ctrl && !IsKeyDown(VK_CONTROL)) {
			continue;
		}
		if (m_alt && !IsKeyDown(VK_MENU)) {
			continue;
		}
		if (m_shift && !IsKeyDown(VK_SHIFT)) {
			continue;
		}

		// 登録したキーは押されていた
		break;
	}

	if (i < waitModkeyDownCount) {
		return TRUE;
	} else {
		return FALSE;
	}
}

void AliasController::ModKeyDown(void)
{
	if (!m_bSyskeyDown) {
		if (m_ctrl) {
			VMVirtualKeyDown(m_hKeyInputWnd, VK_CONTROL, m_bUsePostMessageToSendKey);
		}
		if (m_alt) {
			VMVirtualKeyDown(m_hKeyInputWnd, VK_MENU, m_bUsePostMessageToSendKey);
		}
		if (m_shift) {
			VMVirtualKeyDown(m_hKeyInputWnd, VK_SHIFT, m_bUsePostMessageToSendKey);
		}

		m_bSyskeyDown = IsModKeysDown();
		Sleep(0);
		//if (!m_bSyskeyDown) {
		//	TCHAR szError[BUFFER_SIZE];
		//	_stprintf_s(szError, _countof(szError), _T("修飾キーが押されませんでした[タイムアウト]。") );
		//	LogDebugMessage(Log_Error, szError);
		//}
	}
}

void AliasController::ModKeyUp(void)
{
	if (m_bSyskeyDown) {
		if (!SetForegroundWindow(m_hTargetTopWnd)) {
			OutputDebugString(_T("SetForegroundWindow -> FALSE\n"));
		}

		if (m_mouseMessage.dragButton != DragNONE) {
			VMMouseClick(&m_mouseMessage, TRUE);
			m_mouseMessage.dragButton = DragNONE;
		}

		if (m_shift) {
			VMVirtualKeyUp(m_hKeyInputWnd, VK_SHIFT);
		}
		if (m_alt) {
			VMVirtualKeyUp(m_hKeyInputWnd, VK_MENU);
		}
		if (m_ctrl) {
			VMVirtualKeyUp(m_hKeyInputWnd, VK_CONTROL);
		}
		m_bSyskeyDown = FALSE;
	}
}


BOOL CALLBACK EnumChildProcForMouseInput(HWND hWnd, LPARAM lParam)
{
	TCHAR szWindowTitle[BUFFER_SIZE];
	GetWindowText(hWnd, szWindowTitle, _countof(szWindowTitle));
	if (!_tcsicmp(g_szChildWindowTitle, szWindowTitle)) {
		*(HWND*)lParam = hWnd;
		return FALSE;
	}
	return TRUE;
}
