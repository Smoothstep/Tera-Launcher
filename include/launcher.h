#ifndef _LAUNCHER_H_
#define _LAUNCHER_H_

#pragma once

#include "cef.h"
#include "patcher_callback.h"

#define MESSAGE_BUF_SIZE	1024
#define MESSAGE_TIMEOUT		5000

#define GET_MESSAGE_TIMEOUT 1000

#define LAUNCHER_CLASS_NAME L"LAUNCHER_CLASS"

#define DEFAULT_SLS_URL "http://web-sls.tera.gameforge.com:4566/servers/list.de"
#define DEFAULT_TL_PATH "Client\\TL.exe"

static const std::string g_strGameEvents[] =
{
	"gameEvent(1001)",	// Start game
	"gameEvent(1002)",	// Server select
	"gameEvent(1003)",	// Login
	"gameEvent(1004)",	// Char select
	"gameEvent(1005)",	// Char create
	"gameEvent(1006)",	// Server change
	"gameEvent(1007)",	// 
	"gameEvent(1008)",	//
	"gameEvent(1009)",	// Char creation screen
	"gameEvent(1010)",	// Char created
	"gameEvent(1011)",	// Enter world
	"gameEvent(1012)",	// World entered
	"gameEvent(1013)",	// World logout
	"endPopup(0)"		// End game
};

enum EGameEvents
{
	EVENT_START_GAME	= 1001,
	EVENT_SERVER_SELECT = 1002,
	EVENT_LOGIN			= 1003,
	EVENT_CHAR_SELECT	= 1004,
	EVENT_CHAR_CREATE	= 1005,
	EVENT_SERVER_CHANGE = 1006,
	EVENT_BEGIN_CREATE	= 1009,
	EVENT_CHAR_CREATED	= 1010,
	EVENT_ENTER_WORLD	= 1011,
	EVENT_WORLD_ENTERED = 1012,
	EVENT_WORLD_LOGOUT	= 1013
};

class CPatcher;

class CTLauncher
{
private:
	wchar_t m_szTitle[100];
	wchar_t m_szClass[100];

	std::string m_strTLPath;
	std::string m_strResultMessage;
	std::string m_strResultCode;
	std::string m_strGameAccountName;
	std::string m_strMasterAccountName;
	std::string m_strAccessLevel;
	std::string m_strAccountBits;
	std::string m_strUserPermissiosn;
	std::string m_strTicket;
	std::string m_strServerList;
	std::string m_strAccountData;
	std::string m_strCharsPerServer;
	std::string m_strLastServer;

	HWND m_LauncherHWnd;
	HWND m_MainHWnd;
	HWND m_TLHWnd;

	unsigned char m_szMsgBuffer[MESSAGE_BUF_SIZE];

	COPYDATASTRUCT m_CopyData;

	DWORD m_dwStage;
	DWORD m_dwMsgCount;
	
	PROCESS_INFORMATION m_pInfo;

	boost::atomic_bool m_bStopMessageLoop;
	boost::atomic_bool m_bLaunched;

protected:
	boost::thread *m_InteractThread;
	boost::thread *m_MessageThread;

public:
	enum EIdent
	{
		ID_UNKNOWN		= 0xFFFFFFFF,
		ID_HELLO		= 0x0DBADB0A,
		ID_SLS_URL		= 0x00000002,
		ID_GAME_STR		= 0x00000003,
		ID_LAST_SVR		= 0x00000005,
		ID_CHAR_CNT		= 0x00000006,
		ID_TICKET		= 0x00000008,
		ID_END_POPUP	= 0x00000000
	};

public:
	CTLauncher();
	~CTLauncher();

	void OnLogin();
	void OnLogout();
	void OnEndPopup();

	void RunMessageLoop();

	bool SetAccountData(std::string strAccountData);
	void SetServerList(std::string strServerList);
	void SetLauncherPath(std::string strPath);

	bool Initialize(HINSTANCE hInstance);
	bool Launch(HINSTANCE hInstance);

	void Shutdown();
	bool ReadConfiguration();

	void SetStage(DWORD dwStage);

	HWND HWNDGetLauncher();
	HWND HWNDGetMain();
	HWND HWNDGetTL();

	DWORD GetTLProcessId();
	DWORD GetStage();

	static LRESULT CALLBACK ProcessClassMessages(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	ATOM ARegisterClass(HINSTANCE hInstance);
	bool CreateEMEWindow(HINSTANCE hInstance);

	bool LaunchTL();

public:
	void SendHello();
	void SendCharCnt();
	void SendLastServer();
	void SendServerList();
	void SendAccountList();
	void SendTicket();
};

class CLauncher : public CCefApp, public CTLauncher
{
protected:
	Callback::CPatchCallback m_PatchCallback;

	uint64_t m_paCurrentSize	= 0;
	uint64_t m_paRequiredSize	= 0;

	size_t m_paRequiredUnpackedFileCount = 0;
	size_t m_paCurrentUnpackedFileCount = 0;

public:
	CLauncher(HINSTANCE hInstance);
	~CLauncher();

	bool Patch();
	bool RetrievePatchStatus();

	bool PausePatch();
	bool ResumePatch();
	
	bool RequestAccountInfo();

private:
	CPatcher* m_pPatcher = NULL;
};

extern CLauncher * GetLauncher();
extern CLauncher * CreateLauncher();

extern void RunLauncher();
extern void CloseLauncher();

extern bool ValidAccount(std::string strResult);

extern void ConosleRecordMessages();

#endif