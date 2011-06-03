#pragma once

#include "VirtualMotion.h"

class AliasController
{
public:
	AliasController(void);
	~AliasController(void);

	BOOL Initialize(LPCSTR szBuffer, char* termination);
	void Execute(HWND hWnd, LPCSTR szCommand, double deltaX, double deltaY);
	void ModKeyUp(void);

private:
	void TumbleExecute(int deltaX, int deltaY);
	void TrackExecute(int deltaX, int deltaY);
	void DollyExecute(int deltaX, int deltaY);

	//void HotkeyExecute(I4C3DContext* pContext, PCTSTR szCommand) const;

	BOOL InitializeModifierKeys(PCSTR szModifierKeys);
	BOOL GetTargetChildWnd(void);
	BOOL CheckTargetState(int deltaX, int deltaY);
	void ModKeyDown(void);
	BOOL IsModKeysDown(void);

	HWND m_hTargetTopWnd;
	HWND m_hMouseInputWnd;
	HWND m_hKeyInputWnd;

	VMMouseMessage m_mouseMessage;
	POINT m_currentPos;
	BOOL m_ctrl;
	BOOL m_alt;
	BOOL m_shift;
	BOOL m_bUsePostMessageToSendKey;
	BOOL m_bUsePostMessageToMouseDrag;
	BOOL m_bSyskeyDown;
	int m_DisplayWidth;
	int m_DisplayHeight;
	DWORD m_millisecSleepAfterKeyDown;
	double m_fTumbleRate;
	double m_fTrackRate;
	double m_fDollyRate;
};

