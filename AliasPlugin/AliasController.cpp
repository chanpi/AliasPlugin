#include "StdAfx.h"
#include "AliasController.h"
#include "I4C3DKeysHook.h"
#include "I4C3DCommon.h"
#include "Miscellaneous.h"

static const int BUFFER_SIZE = 256;

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
	m_fTumbleRate		= 1;
	m_fTrackRate		= 1;
	m_fDollyRate		= 1;
	m_bUsePostMessageToSendKey		= FALSE;
	m_bUsePostMessageToMouseDrag	= TRUE;
	m_millisecSleepAfterKeyDown		= 30;
	m_ctrl = m_shift = m_bSyskeyDown = FALSE;
	m_alt = TRUE;

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
 * Control�I�u�W�F�N�g�̏��������s���܂��B
 * 
 * @returns
 * �������ɐ��������ꍇ�ɂ�TRUE�A���s�����ꍇ�ɂ�FALSE��Ԃ��܂��B
 * 
 * Control�I�u�W�F�N�g�̏��������s���܂��B
 * �ǂ�Control�I�u�W�F�N�g���́A������I4C3DContext�|�C���^�ɓo�^����Ă���Control�h���N���X�ɂ��܂��B
 * 
 * @remarks
 * InitializeModifierKeys()�ŏC���L�[�̐ݒ���s���܂��B
 * 
 * @see
 * InitializeModifierKeys()
 */

BOOL AliasController::Initialize(LPCSTR szBuffer, char* termination)
{
	char tmpCommand[16];
	char szModKeys[32];

	sscanf_s(szBuffer, g_initCommandFormat, tmpCommand,	sizeof(tmpCommand), szModKeys, sizeof(szModKeys), &m_fTumbleRate, &m_fTrackRate, &m_fDollyRate, termination);
	if (!m_fTumbleRate) {
		m_fTumbleRate = 1.0;
	} else if (!m_fTrackRate){
		m_fTrackRate = 1.0;
	} else if (!m_fDollyRate) {
		m_fDollyRate = 1.0;
	}

	return InitializeModifierKeys(szModKeys);
}

/**
 * @brief
 * 3D�\�t�g�Ŏg�p����C���L�[�̎擾�A�ݒ�A����уL�[�����̊Ď��v���O�����ւ̓o�^���s���܂��B
 * 
 * @returns
 * �C���L�[�̎擾�A�ݒ�A�o�^�ɐ��������ꍇ�ɂ�TRUE�A���s�����ꍇ�ɂ�FALSE��Ԃ��܂��B
 * 
 * 3D�\�t�g�Ŏg�p����C���L�[�̎擾�A�ݒ�A����уL�[�����̊Ď��v���O�����ւ̓o�^���s���܂��B
 * 
 * @remarks
 * I4C3DKeysHook.dll��AddHookedKeyCode()�ŃL�[�t�b�N�̓o�^���s���܂��B
 * 
 * @see
 * AddHookedKeyCode()
 */
BOOL AliasController::InitializeModifierKeys(PCSTR szModifierKeys)
{
	m_ctrl = m_alt = m_shift = m_bSyskeyDown = FALSE;
	if (_strcmpi(szModifierKeys, "NULL") == 0 || szModifierKeys == NULL) {
		m_alt = TRUE;
		AddHookedKeyCode(VK_MENU);
		return TRUE;
	}

	PCSTR pType = NULL;
	do {
		char szKey[BUFFER_SIZE] = {0};
		pType = strchr(szModifierKeys, '+');
		if (pType != NULL) {
			strncpy_s(szKey, _countof(szKey), szModifierKeys, pType-szModifierKeys+1);
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
			AddHookedKeyCode( VK_SHIFT );
			m_shift = TRUE;
			break;

		case _T('A'):
		case _T('a'):
			AddHookedKeyCode( VK_MENU );
			m_alt = TRUE;
			break;
		}
	} while (pType != NULL);

	return TRUE;
}


BOOL AliasController::GetTargetChildWnd(void)
{
	m_hKeyInputWnd = m_hTargetTopWnd;
	EnumChildWindows(m_hKeyInputWnd, EnumChildProcForMouseInput, (LPARAM)&m_hMouseInputWnd);
	if (m_hMouseInputWnd == NULL) {
		return FALSE;
	}
	return TRUE;
}

BOOL AliasController::CheckTargetState(void)
{
	if (m_hTargetTopWnd == NULL) {
		//ReportError(_T("�^�[�Q�b�g�E�B���h�E���擾�ł��܂���B<AliasController::CheckTargetState>"));
		LogDebugMessage(Log_Error, _T("�^�[�Q�b�g�E�B���h�E���擾�ł��܂���B<AliasController::CheckTargetState>"));

	} else if (m_hKeyInputWnd == NULL) {
		LogDebugMessage(Log_Error, _T("�L�[���̓E�B���h�E���擾�ł��܂���B<AliasController::CheckTargetState>"));

	} else if (m_hMouseInputWnd == NULL) {
		LogDebugMessage(Log_Error, _T("�}�E�X���̓E�B���h�E���擾�ł��܂���B<AliasController::CheckTargetState>"));

	} else {
		// �^�[�Q�b�g�E�B���h�E�̈ʒu�̃`�F�b�N
		POINT tmpCurrentPos = m_currentPos;
		ClientToScreen(m_hMouseInputWnd, &tmpCurrentPos);

		RECT windowRect;
		GetWindowRect(m_hMouseInputWnd, &windowRect);
		if (WindowFromPoint(tmpCurrentPos) != m_hMouseInputWnd ||
			tmpCurrentPos.x < windowRect.left+200 || windowRect.right-500 < tmpCurrentPos.x ||
			tmpCurrentPos.y < windowRect.top || windowRect.bottom < tmpCurrentPos.y) {
				if (m_mouseMessage.dragButton != DragNONE) {
					VMMouseClick(&m_mouseMessage, TRUE);
					m_mouseMessage.dragButton = DragNONE;
				}

				RECT rect;
				GetClientRect(m_hMouseInputWnd, &rect);
				m_currentPos.x = rect.left + (rect.right - rect.left) / 2;
				m_currentPos.y = rect.top + (rect.bottom - rect.top) / 2;
		}
		return TRUE;
	}

	return FALSE;
}


void AliasController::Execute(LPCSTR szCommand, double deltaX, double deltaY)
{
	HWND tmpWnd = GetForegroundWindow();
	if (tmpWnd != m_hTargetTopWnd) {
		m_hTargetTopWnd = tmpWnd;
	}

	// ���ۂɉ��z�L�[�E���z�}�E�X������s���q�E�B���h�E�̎擾
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
			TrackExecute((INT)(deltaX * m_fTumbleRate), (INT)(deltaY * m_fTumbleRate));
		}

	} else if (_strcmpi(szCommand, COMMAND_DOLLY) == 0) {
		ModKeyDown();
		if (m_bSyskeyDown) {
			DollyExecute((INT)(deltaX * m_fTumbleRate), (INT)(deltaY * m_fTumbleRate));
		}

	} else {
//#if _UNICODE || UNICODE
//		TCHAR wszCommand[BUFFER_SIZE] = {0};
//		MultiByteToWideChar(CP_ACP, 0, szCommand, -1, wszCommand, _countof(wszCommand));
//		ModKeyUp();
//		HotkeyExecute(pContext, wszCommand);
//#else
//		pContext->pController->HotkeyExecute(lpszCommand);
//#endif
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

	if (!CheckTargetState()) {
		return;
	}
	m_mouseMessage.bUsePostMessage	= m_bUsePostMessageToMouseDrag;
	m_mouseMessage.hTargetWnd		= m_hMouseInputWnd;
	m_mouseMessage.dragStartPos		= m_currentPos;
	m_currentPos.x					+= deltaX;
	m_currentPos.y					+= deltaY;
	m_mouseMessage.dragEndPos		= m_currentPos;

	m_mouseMessage.uKeyState		= MK_LBUTTON;
	if (m_ctrl) {
		m_mouseMessage.uKeyState |= MK_CONTROL;
	}
	if (m_shift) {
		m_mouseMessage.uKeyState |= MK_SHIFT;
	}

	if (m_mouseMessage.dragButton != LButtonDrag) {
		m_mouseMessage.dragButton = LButtonDrag;
		VMMouseClick(&m_mouseMessage, FALSE);
	}
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
	if (!CheckTargetState()) {
		return;
	}
	m_mouseMessage.bUsePostMessage	= m_bUsePostMessageToMouseDrag;
	m_mouseMessage.hTargetWnd		= m_hMouseInputWnd;
	m_mouseMessage.dragStartPos		= m_currentPos;
	m_currentPos.x					+= deltaX;
	m_currentPos.y					+= deltaY;
	m_mouseMessage.dragEndPos		= m_currentPos;

	m_mouseMessage.uKeyState		= MK_MBUTTON;
	if (m_ctrl) {
		m_mouseMessage.uKeyState	|= MK_CONTROL;
	}
	if (m_shift) {
		m_mouseMessage.uKeyState	|= MK_SHIFT;
	}
	if (m_mouseMessage.dragButton != MButtonDrag) {
		m_mouseMessage.dragButton = MButtonDrag;
		VMMouseClick(&m_mouseMessage, FALSE);
	}
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

	if (!CheckTargetState()) {
		return;
	}
	m_mouseMessage.bUsePostMessage	= m_bUsePostMessageToMouseDrag;
	m_mouseMessage.hTargetWnd		= m_hMouseInputWnd;
	m_mouseMessage.dragStartPos		= m_currentPos;
	m_currentPos.x					+= deltaX;
	m_currentPos.y					+= deltaY;
	m_mouseMessage.dragEndPos		= m_currentPos;

	m_mouseMessage.uKeyState		= MK_RBUTTON;
	if (m_ctrl) {
		m_mouseMessage.uKeyState	|= MK_CONTROL;
	}
	if (m_shift) {
		m_mouseMessage.uKeyState	|= MK_SHIFT;
	}
	if (m_mouseMessage.dragButton != RButtonDrag) {
		m_mouseMessage.dragButton = RButtonDrag;
		VMMouseClick(&m_mouseMessage, FALSE);
	}
	VMMouseMove(&m_mouseMessage);
}

//void AliasController::HotkeyExecute(I4C3DContext* pContext, PCTSTR szCommand) const
//{
//	I4C3DControl::HotkeyExecute(pContext, m_hTargetTopWnd, szCommand);
//}

/**
 * @brief
 * �o�^�����C���L�[�������ꂽ���m�F���܂��B
 * 
 * @returns
 * �o�^�����C���L�[��������Ă���ꍇ�ɂ�TRUE�A�����łȂ��ꍇ��FALSE��Ԃ��܂��B
 * 
 * �o�^�����C���L�[�������ꂽ���m�F���܂��B
 * ������Ă��Ȃ��ꍇ�́ASleep���܂��B
 * Sleep�͍ő�retryCount��s���ASleep�Ԋu��
 * ����d�˂邲�Ƃ�2�{���Ă����܂��B
 * �i�ő� [1 << retryCount] msec��Sleep�j
 * 
 * @remarks
 * I4C3DKeysHook.dll��IsAllKeysDown()�֐��ŃL�[�������m�F���܂��B
 * 
 * @see
 * IsAllKeysDown()
 */
BOOL AliasController::IsModKeysDown(void)
{
	const int retryCount = 3/*12*/;
	int sleepInterval = 1;

	int i = 0;
	for (; i < retryCount; ++i) {
		Sleep(sleepInterval);
		{
			TCHAR szBuf[32];
			_stprintf_s(szBuf, 32, _T("%4d msec Sleep\n"), sleepInterval);
			OutputDebugString(szBuf);
		}

		if (m_ctrl && !IsKeyDown(VK_CONTROL)) {
			sleepInterval *= 2;
			continue;
		}
		if (m_alt && !IsKeyDown(VK_MENU)) {
			sleepInterval *= 2;
			continue;
		}
		if (m_shift && !IsKeyDown(VK_SHIFT)) {
			sleepInterval *= 2;
			continue;
		}
		break;
	}

	if (i < retryCount) {
		m_bSyskeyDown = TRUE;
		return TRUE;
	} else {
		m_bSyskeyDown = FALSE;
		return FALSE;
	}
}

void AliasController::ModKeyDown(void)
{
	if (!m_bSyskeyDown) {
		DWORD dwBuf = 0;
		HWND hForeground = GetForegroundWindow();

		DWORD dwThreadId = GetWindowThreadProcessId(hForeground, NULL);
		DWORD dwTargetThreadId = GetWindowThreadProcessId(m_hKeyInputWnd, NULL);

		AttachThreadInput(dwTargetThreadId, dwThreadId, TRUE);

		SystemParametersInfo(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &dwBuf, 0);
		SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, NULL, 0);

		SetForegroundWindow(m_hTargetTopWnd);
		Sleep(m_millisecSleepAfterKeyDown);

		if (m_ctrl) {
			VMVirtualKeyDown(m_hMouseInputWnd, VK_CONTROL, m_bUsePostMessageToSendKey);
		}
		if (m_alt) {
			VMVirtualKeyDown(m_hMouseInputWnd, VK_MENU, m_bUsePostMessageToSendKey);
		}
		if (m_shift) {
			VMVirtualKeyDown(m_hMouseInputWnd, VK_SHIFT, m_bUsePostMessageToSendKey);
		}
		SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, &dwBuf, 0);
		AttachThreadInput(dwThreadId, dwTargetThreadId, FALSE);

		m_bSyskeyDown = IsModKeysDown();
		if (!m_bSyskeyDown) {
			TCHAR szError[BUFFER_SIZE];
			_stprintf_s(szError, _countof(szError), _T("�C���L�[��������܂���ł���[�^�C���A�E�g]�B") );
			LogDebugMessage(Log_Error, szError);
		}
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
