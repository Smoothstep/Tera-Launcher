#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <process.h>
#include <sstream>
#include <algorithm>
#include <map>
#include <iostream>
#include <numeric>
#include <vadefs.h>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string_regex.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "launcher.h"
#include "resource.h"
#include "config.h"
#include "log.h"

CefRefPtr<CLauncher> g_pLauncher = NULL;

BOOL CALLBACK API_EW_FIND_CALLBACK(HWND hWnd, LPARAM lParam)
{
	static wchar_t szClass[32];

	if (!lParam)
	{
		return false;
	}

	if (!g_pLauncher)
	{
		return false;
	}

	static DWORD dwProcessId;

	if (GetWindowThreadProcessId(hWnd, &dwProcessId) == 0)
	{
		return true;
	}

	if (dwProcessId != g_pLauncher->GetTLProcessId())
	{
		return true;
	}

	GetClassName(hWnd, szClass, 32);

	if (wcscmp(szClass, LAUNCHER_CLASS_NAME) == 0)
	{
		*reinterpret_cast<HWND*>(lParam) = hWnd;
		return false;
	}

	return true;
}

ATOM Launcher_RegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex = { 0 };

	wcex.cbSize			= sizeof(WNDCLASSEX);
	wcex.lpfnWndProc	= DefWindowProc;
	wcex.hInstance		= hInstance;
	wcex.lpszClassName	= L"Launcher";

	return RegisterClassEx(&wcex);
}

CLauncher * GetLauncher()
{
	return g_pLauncher;
}

void LauncherShutdown()
{
	if (g_pLauncher)
	{
		delete g_pLauncher;
		g_pLauncher = NULL;
	}
}

CLauncher * CreateLauncher()
{
	CefEnableHighDPISupport();

	if (g_pLauncher)
	{
		CefShutdown();
	}

	g_pLauncher = NULL;

	HINSTANCE hInstance = GetModuleHandle(NULL);

	if (!hInstance)
	{
		TRACEN("Invalid instance");
		return g_pLauncher;
	}

	g_pLauncher = new CLauncher(hInstance);

	if (CefExecute(hInstance, g_pLauncher) >= 0)
	{
		TRACEN("Executing CEF failed");
		LauncherShutdown();
		return g_pLauncher;
	}

	if (!CefInit(hInstance, g_pLauncher))
	{
		TRACEN("Initializing CEF failed");
		LauncherShutdown();
		return NULL;
	}

	if (!g_pLauncher->ReadConfiguration())
	{
		TRACEN("Failed to read configuration file");
		LauncherShutdown();
		return g_pLauncher;
	}

	if (!g_pLauncher->Initialize(hInstance))
	{
		TRACEN("Failed to initialize launcher");
		LauncherShutdown();
		return g_pLauncher;
	}

	return g_pLauncher;
}

void RunLauncher()
{
	if (!g_pLauncher)
	{
		return;
	}

	g_pLauncher->RunMessageLoop();
}

bool ValidAccount(const std::string& strResult)
{
	std::stringstream stream(strResult);

	boost::property_tree::ptree pt;

	try
	{
		boost::property_tree::json_parser::read_json(stream, pt);
	}
	catch (const boost::property_tree::json_parser_error& error)
	{
		TRACEN("Could not parse account info: %s", error.message().c_str());
		return false;
	}

	if (pt.empty())
	{
		TRACEN("Invalid account info format.");
		return false;
	}

	if (pt.find("result-message") == pt.not_found())
	{
		TRACEN("Invalid account info format.");
		return false;
	}

	boost::property_tree::ptree& ptResult = pt.get_child("result-message");

	if (ptResult.data().empty())
	{
		TRACEN("Empty result message.");
		return false;
	}

	std::string strMessage = ptResult.get_value<std::string>();

	if (strMessage != std::string("OK"))
	{
		TRACEN("Account invalid.");
		return false;
	}

	return true;
}

CTLauncher::CTLauncher() : 
	m_MainHWnd(NULL), 
	m_TLHWnd(NULL), 
	m_LauncherHWnd(NULL),
	m_InteractThread(NULL),
	m_MessageThread(NULL),
	m_bStopMessageLoop(false), 
	m_bLaunched(false),
	m_dwMsgCount(0),
	m_dwStage(ID_UNKNOWN),
	m_Config(NULL)
{
	memset(&m_pInfo,		0, sizeof(PROCESS_INFORMATION));
	memset(m_szMsgBuffer,	0, MESSAGE_BUF_SIZE);
	memset(&m_CopyData,		0, sizeof(COPYDATASTRUCT));

	wcscpy_s(m_szTitle, MAX_WND_NAME_LEN - 1, TEXT(EME_WINDOW_TITLE));
	wcscpy_s(m_szClass, MAX_WND_NAME_LEN - 1, TEXT(EME_WINDOW_CLASS_NAME));
}

CTLauncher::~CTLauncher()
{
	if (m_InteractThread)
	{
		delete m_InteractThread;
		m_InteractThread = NULL;
	}

	if (m_MessageThread)
	{
		delete m_MessageThread;
		m_MessageThread = NULL;
	}

	if (m_Config)
	{
		delete m_Config;
		m_Config = NULL;
	}
}

void CTLauncher::OnEndPopup()
{
	m_bLaunched = false;
	m_dwMsgCount = 0;

	if (!GetAccountInfo())
	{
		TRACEN("Unable to retrieve account info - Logging out.");

		CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("Logout");

		if (!CEF->Browser()->SendProcessMessage(PID_BROWSER, msg))
		{
			TRACEN("Unable to logout.");
		}
	}
}

bool CTLauncher::SetAccountData(const std::string& strAccountData)
{
	std::stringstream stream(strAccountData);

	boost::property_tree::ptree pt;

	try
	{
		boost::property_tree::json_parser::read_json(stream, pt);
	}
	catch (const boost::property_tree::json_parser_error& error)
	{
		TRACEN("Could not parse account info: %s\n", error.message().c_str());
		return false;
	}

	optional_pt ptTicket = pt.get_child_optional("ticket");

	if (!ptTicket)
	{
		TRACEN("Invalid account data: No ticket child.");
		return false;
	}

	optional_pt ptCharsPerServer = pt.get_child_optional("chars_per_server");

	if (!ptCharsPerServer)
	{
		TRACEN("Invalid account data: No chars_per_server child.");
		return false;
	}

	optional_pt ptAccountBits = pt.get_child_optional("account_bits");

	if (!ptAccountBits)
	{
		TRACEN("Invalid account data: No account_bits child.");
		return false;
	}

	optional_pt ptResultMessage = pt.get_child_optional("result-message");

	if (!ptResultMessage)
	{
		TRACEN("Invalid account data: No result-message child.");
		return false;
	}

	optional_pt ptResultCode = pt.get_child_optional("result-code");

	if (!ptResultCode)
	{
		TRACEN("Invalid account data: No result-code child.");
		return false;
	}

	optional_pt ptGameAccountName = pt.get_child_optional("game_account_name");

	if (!ptGameAccountName)
	{
		TRACEN("Invalid account data: No game_account_name child.");
		return false;
	}

	optional_pt ptAccessLevel = pt.get_child_optional("access_level");

	if (!ptAccessLevel)
	{
		TRACEN("Invalid account data: No access_level child.");
		return false;
	}

	optional_pt ptMasterAccountName = pt.get_child_optional("master_account_name");

	if (!ptMasterAccountName)
	{
		TRACEN("Invalid account data: No master_account_name child.");
		return false;
	}

	optional_pt ptLastConnectedServerId = pt.get_child_optional("last_connected_server_id");

	if (!ptLastConnectedServerId)
	{
		TRACEN("Invalid account data: No last_connected_server_id child.");
		return false;
	}

	optional_pt ptUserPermission = pt.get_child_optional("user_permission");

	if (!ptUserPermission)
	{
		TRACEN("Invalid account data: No user_permission child.");
		return false;
	}

	m_strTicket				= ptTicket->get_value<std::string>();
	m_strCharsPerServer		= ptCharsPerServer->get_value<std::string>();
	m_strAccountBits		= ptAccountBits->get_value<std::string>();
	m_strResultMessage		= ptResultMessage->get_value<std::string>();
	m_strResultCode			= ptResultCode->get_value<std::string>();
	m_strGameAccountName	= ptGameAccountName->get_value<std::string>();
	m_strAccessLevel		= ptAccessLevel->get_value<std::string>();
	m_strMasterAccountName	= ptMasterAccountName->get_value<std::string>();
	m_strLastServer			= ptLastConnectedServerId->get_value<std::string>();
	m_strUserPermissiosn	= ptUserPermission->get_value<std::string>();
	m_strAccessLevel		= ptAccessLevel->get_value<std::string>();

	m_strAccountData = strAccountData;

	if (m_bLaunched)
	{
		if (m_dwMsgCount >= ID_TICKET)
		{
			SendTicket();
		}
	}
	else
	{
		if (m_dwMsgCount >= ID_GAME_STR)
		{
			SendAccountList();
		}
	}

	return true;
}

bool CTLauncher::IndicateTLPath(const std::string & strPath)
{
	SetLauncherPath(strPath);

	if (!m_Config)
	{
		if (!ReadConfiguration())
		{
			return false;
		}
	}

	m_Config->ChangeNode("tl_path", strPath.c_str());

	if (!m_Config->WriteIni((boost::filesystem::current_path().string() + "\\" + LAUNCHER_CONFIG_PATH).c_str()))
	{
		TRACEN("Failed to set TL.exe path");
		return false;
	}

	return true;
}

bool CTLauncher::IndicateSLURL(const std::string & slsUrl)
{
	if (!SetServerList(slsUrl))
	{
		return false;
	}

	if (!m_Config)
	{
		if (!ReadConfiguration())
		{
			return false;
		}
	}

	m_Config->ChangeNode("serverlist_url", slsUrl.c_str());

	if (!m_Config->WriteIni((boost::filesystem::current_path().string() + "\\" + LAUNCHER_CONFIG_PATH).c_str()))
	{
		TRACEN("Failed to set serverlist url");
		return false;
	}

	return true;
}

bool CTLauncher::SetServerList(const std::string& strServerList)
{
	if (!	boost::regex_match(strServerList,
			boost::regex("(\b(https?|ftp|file)://)?[-A-Za-z0-9+&@#/%?=~_|!:,.;]+[-A-Za-z0-9+&@#/%=~_|]",
			boost::regex::extended)))
	{
		TRACEN("Invalid Serverlist url: %s", strServerList.c_str());
		return false;
	}

	m_strServerList = strServerList;

	return true;
}

void CTLauncher::SetLauncherPath(const std::string& strPath)
{
	m_strTLPath = strPath;
}

void CTLauncher::RunMessageLoop()
{
	while (!m_bStopMessageLoop)
	{
		MSG msg;

#ifndef PATCHER_PEEK_MESSAGE
		UINT_PTR pTimer = SetTimer(NULL, NULL, GET_MESSAGE_TIMEOUT, NULL);

		while (!m_bStopMessageLoop)
		{
			if (GetMessage(&msg, NULL, 0, 0) > 0)
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			switch (m_dwStage)
			{
			case ID_HELLO:
				SendHello();
				break;

			case ID_SLS_URL:
				SendServerList();
				break;

			case ID_GAME_STR:
				SendAccountList();
				break;

			case ID_TICKET:
				if (!GetAccountInfo())
				{
					TRACEN("Unable to retrieve account info.");
				}

				break;

			case ID_CHAR_CNT:
				SendCharCnt();
				break;

			case ID_LAST_SVR:
				SendLastServer();
				break;

			case ID_END_POPUP:
				OnEndPopup();
				break;
			}

			m_dwStage = -1;
		}

		if (pTimer)
		{
			KillTimer(NULL, pTimer);
		}
#else
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		switch (m_dwStage)
		{
		case ID_HELLO:
			SendHello();
			break;

		case ID_SLS_URL:
			SendServerList();
			break;

		case ID_GAME_STR:
			SendAccountList();
			break;

		case ID_TICKET:
			if (!GetAccountInfo())
			{
				TRACEN("Unable to retrieve account info.");
			}

			break;

		case ID_CHAR_CNT:
			SendCharCnt();
			break;

		case ID_LAST_SVR:
			SendLastServer();
			break;

		case ID_END_POPUP:
			OnEndPopup();
			break;
			}

		m_dwStage = -1;
#endif
	}

	Shutdown();
}

std::string & CTLauncher::GetAccountData()
{
	return m_strAccountData;
}

std::string & CTLauncher::GetServerList()
{
	if (m_strServerList.empty())
	{
		if (!ReadConfiguration())
		{
			TRACEN("Failed to read configureation");
		}
	}

	return m_strServerList;
}

bool CTLauncher::Launch(HINSTANCE hInstance)
{
	if (!m_MainHWnd)
	{
		if (!CreateEMEWindow(hInstance))
		{
			TRACEN("Failed in creating eme window");
			return false;
		}
	}

	if (!LaunchTL())
	{
		TRACEN("Failed launching TL.exe");
		return false;
	}

	return true;
}

bool CTLauncher::Initialize(HINSTANCE hInstance)
{
	if (!CreateEMEWindow(hInstance))
	{
		TRACEN("Failed when creating eme window");
		return false;
	}

#ifdef CEF_CHILD
	if (m_LauncherHWnd)
	{
		DestroyWindow(m_LauncherHWnd);
	}

	m_LauncherHWnd = CreateWindow(L"Launcher", L"Launcher Window", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1024, 640, NULL, NULL, hInstance, NULL);

	if (!m_LauncherHWnd)
	{
		TRACEN("Error when creating launcher parent window");
		return false;
	}

	if (!SetWindowPos(m_LauncherHWnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER))
	{
		TRACEN("Launcher parent window repos error");
		return false;
	}

	ShowWindow(m_LauncherHWnd, 1);

	if (!UpdateWindow(m_LauncherHWnd))
	{
		TRACEN("Launcher parent window update error");
		return false;
	}
#endif

	return true;
}

void CTLauncher::Shutdown()
{
	if (!m_MainHWnd)
	{
		TRACE("Main window handle NULL");
		return;
	}

	if (!DestroyWindow(m_MainHWnd))
	{
		TRACEN("Unable to destroy launcher window");
		return;
	}

	m_MainHWnd = NULL;

	if (!m_InteractThread)
	{
		return;
	}

	delete m_InteractThread;
	m_InteractThread = NULL;
}

bool CTLauncher::ReadConfiguration()
{
	boost::filesystem::path pCurrent = boost::filesystem::current_path();

	pCurrent += "\\";
	pCurrent += LAUNCHER_CONFIG_PATH;

	if (!boost::filesystem::exists(pCurrent))
	{
		TRACEN("Unable to find " LAUNCHER_CONFIG_PATH);
		return false;
	}

	boost::filesystem::ifstream ifConfig(pCurrent, std::ios_base::in);

	if (!ifConfig.is_open())
	{
		TRACEN("Unable to open " LAUNCHER_CONFIG_PATH);
		return false;
	}

	if (!m_Config)
	{
		m_Config = new CInitializationDocument();
	}

	if (!m_Config->ParseIni(ifConfig))
	{
		TRACE("Invalid config: %s", pCurrent.string().c_str());
		return false;
	}

	CPairNode* pNode;

	if (!!(pNode = m_Config->GetNode("tl_path")))
	{
		SetLauncherPath(pNode->GetValue());
	}

	if (!!(pNode = m_Config->GetNode("serverlist_url")))
	{
		if (!SetServerList(pNode->GetValue()))
		{
			TRACEN("Invalid serverlist url: %s", pNode->GetValue());
			return false;
		}
	}

	if (m_strServerList.empty())
	{
		TRACEN("No serverlist_url specifed. Choosing " DEFAULT_SLS_URL " as default");
		m_strServerList = DEFAULT_SLS_URL;
	}

	if (m_strTLPath.empty())
	{
		TRACEN("No tl_path specifed. Choosing " DEFAULT_TL_PATH " as default");
		m_strTLPath = DEFAULT_TL_PATH;
	}

	return true;
}

ATOM CTLauncher::ARegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex = { 0 };
	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName	= m_szClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wcex.cbSize			= sizeof(WNDCLASSEX);
	wcex.lpfnWndProc	= CTLauncher::ProcessClassMessages;
	wcex.hInstance		= hInstance;
	wcex.lpszClassName	= m_szClass;

	return RegisterClassEx(&wcex);
}

bool CTLauncher::CreateEMEWindow(HINSTANCE hInstance)
{
	if (!ARegisterClass(hInstance))
	{
		WNDCLASSEX wndClass;

		if (!GetClassInfoEx(hInstance, m_szClass, &wndClass))
		{
			return false;
		}

		if (m_MainHWnd)
		{
			TRACEN("Launcher class already registered");
			return true;
		}
	}

	m_MainHWnd = CreateWindow(m_szClass, m_szClass, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, this);

	if (!m_MainHWnd)
	{
		return false;
	}

	SetWindowLongPtr(m_MainHWnd, GWL_USERDATA, reinterpret_cast<LONG_PTR>(this));

	return true;
}

bool CTLauncher::LaunchTL()
{
	STARTUPINFOA aInfo = { 0 };
	aInfo.cb = sizeof(STARTUPINFOA);

	if (m_strTLPath.empty())
	{
		TRACEN("No TL.exe path given. Set tl_path first.");
		return false;
	}

	if (m_TLHWnd)
	{
		SendMessage(m_TLHWnd, WM_DESTROY,	reinterpret_cast<WPARAM>(m_MainHWnd), 0);
		SendMessage(m_TLHWnd, WM_QUIT,		reinterpret_cast<WPARAM>(m_MainHWnd), 0);
	}

	m_TLHWnd = NULL;

	if (CreateProcessA(NULL, LPSTR(m_strTLPath.c_str()), NULL, NULL, true, 36, NULL, NULL, &aInfo, &m_pInfo))
	{
		ResumeThread(m_pInfo.hThread);
		CloseHandle(m_pInfo.hProcess);

		WaitForSingleObject(m_pInfo.hProcess, INFINITE);
		WaitForInputIdle(m_pInfo.hProcess, INFINITE);

		DWORD dwExitCode = STILL_ACTIVE;

		while (m_TLHWnd == NULL)
		{
			GetExitCodeProcess(m_pInfo.hProcess, &dwExitCode);

			if (dwExitCode != STILL_ACTIVE)
			{
				break;
			}

			EnumWindows(API_EW_FIND_CALLBACK, LPARAM(&m_TLHWnd));
			Sleep(1000);
		}

		if (m_TLHWnd == NULL)
		{
			TRACEN("TL.exe process was not found or was not started correctly.");
			return false;
		}

		return true;
	}

	TRACEN("Unable to start TL.exe correctly.");
	return false;
}

void CTLauncher::SendHello()
{
	m_dwMsgCount = 0;

	if (!m_MainHWnd)
	{
		TRACEN("Cannot send messages without launcher window.");
		return;
	}

	if (!m_TLHWnd)
	{
		TRACEN("Cannot send messasge without TL window.");
		return;
	}

	if (m_bLaunched)
	{
		m_strAccountData = "";

		if (!GetAccountInfo())
		{
			TRACEN("Cannot request account data.");
			return;
		}
	}

#if 8 >= MESSAGE_BUF_SIZE
#error Message buffer way too small
#endif

	memcpy(m_szMsgBuffer, "Hello!!", 7);
	{
		m_szMsgBuffer[7] = 0;
	}

	m_CopyData.cbData = 8;
	m_CopyData.dwData = ID_HELLO;
	m_CopyData.lpData = m_szMsgBuffer;

	DWORD p;
	SendMessageTimeoutW(m_TLHWnd, WM_COPYDATA, WPARAM(m_MainHWnd), LPARAM(&m_CopyData), SMTO_NORMAL, MESSAGE_TIMEOUT, &p);

	if (!p)
	{
		TRACEN("Send Message failed: ID_HELLO: %d", GetLastError());
	}
	else
	{
		TRACEN("Sent new message: ID_HELLO.");
	}

	m_bLaunched = false;
}

void CTLauncher::SendCharCnt()
{
	if (!m_MainHWnd)
	{
		TRACEN("Cannot send messages without launcher window.");
		return;
	}

	if (!m_TLHWnd)
	{
		TRACEN("Cannot send messasge without TL window.");
		return;
	}

	std::string strCharCnt = 
		std::string("{\"chars_per_server\":{")		+ m_strCharsPerServer	+ 
		std::string("},\"account_bits\":\"")		+ m_strAccountBits		+ 
		std::string("\",\"result-message\":\"")		+ m_strResultMessage	+ 
		std::string("\",\"result-code\":")			+ m_strResultCode		+ 
		std::string(",\"game_account_name\":\"")	+ m_strGameAccountName	+ 
		std::string("\",\"access_level\":")			+ m_strAccessLevel		+ 
		std::string(",\"master_account_name\":\"")	+ m_strMasterAccountName+ "\"}";

	if (strCharCnt.length() >= MESSAGE_BUF_SIZE)
	{
		TRACEN("Copydata too big for buffer");
		return;
	}

	memcpy(m_szMsgBuffer, strCharCnt.data(), strCharCnt.length());
	{
		m_szMsgBuffer[strCharCnt.length()] = 0;
	}

	m_CopyData.cbData = strCharCnt.length();
	m_CopyData.dwData = m_dwMsgCount;
	m_CopyData.lpData = m_szMsgBuffer;

	DWORD p;
	SendMessageTimeoutW(m_TLHWnd, WM_COPYDATA, WPARAM(m_MainHWnd), LPARAM(&m_CopyData), SMTO_NORMAL, MESSAGE_TIMEOUT, &p);

	if (!p)
	{
		TRACEN("Send Message failed: ID_CHAR_CNT: %d", GetLastError());
	}
	else
	{
		TRACEN("Sent new message: ID_CHAR_CNT");
	}
}

void CTLauncher::SendLastServer()
{
	if (!m_MainHWnd)
	{
		TRACEN("Cannot send messages without launcher window.");
		return;
	}

	if (!m_TLHWnd)
	{
		TRACEN("Cannot send messasge without TL window.");
		return;
	}

	std::string strLastServer =
		std::string("{\"last_connected_server_id\":") + m_strLastServer + "}";


	if (strLastServer.length() >= MESSAGE_BUF_SIZE)
	{
		TRACEN("CopyData too big for buffer");
		return;
	}

	memcpy(m_szMsgBuffer, strLastServer.data(), strLastServer.length());
	{
		m_szMsgBuffer[strLastServer.length()] = 0;
	}

	m_CopyData.cbData = strLastServer.length();
	m_CopyData.dwData = m_dwMsgCount;
	m_CopyData.lpData = m_szMsgBuffer;

	DWORD p;
	SendMessageTimeoutW(m_TLHWnd, WM_COPYDATA, WPARAM(m_MainHWnd), LPARAM(&m_CopyData), SMTO_NORMAL, MESSAGE_TIMEOUT, &p);

	if (!p)
	{
		TRACEN("Send Message failed: ID_LAST_SVR: %d", GetLastError());
	}
	else
	{
		TRACEN("Sent new message: ID_LAST_SVR");
	}
}

void CTLauncher::SendServerList()
{
	if (!m_MainHWnd)
	{
		TRACEN("Cannot send messages without launcher window.");
		return;
	}

	if (!m_TLHWnd)
	{
		TRACEN("Cannot send messasge without TL window.");
		return;
	}

	if (m_strServerList.length() >= MESSAGE_BUF_SIZE)
	{
		TRACEN("CopyData too big for buffer.");
		return;
	}

	if (m_strServerList.empty())
	{
		TRACEN("Empty server list.");
		return;
	}

	memcpy(m_szMsgBuffer, m_strServerList.data(), m_strServerList.length());
	{
		m_szMsgBuffer[m_strServerList.length()] = 0;
	}

	m_CopyData.cbData = m_strServerList.length();
	m_CopyData.dwData = m_dwMsgCount;
	m_CopyData.lpData = m_szMsgBuffer;

	DWORD p;
	SendMessageTimeoutW(m_TLHWnd, WM_COPYDATA, WPARAM(m_MainHWnd), LPARAM(&m_CopyData), SMTO_NORMAL, MESSAGE_TIMEOUT, &p);

	if (!p)
	{
		TRACEN("Send Message failed: ID_SLS_URL: %d", GetLastError());
	}
	else
	{
		TRACEN("Sent new message: ID_SLS_URL");
	}
}

void CTLauncher::SendAccountList()
{
	if (!m_MainHWnd)
	{
		TRACEN("Cannot send messages without launcher window.");
		return;
	}

	if (!m_TLHWnd)
	{
		TRACEN("Cannot send messasge without TL window.");
		return;
	}

	if (m_strAccountData.length() >= MESSAGE_BUF_SIZE)
	{
		TRACEN("CopyData too big for buffer.");
		return;
	}

	if (m_strAccountData.empty())
	{
		TRACEN("Empty account data.");
		return;
	}

	memcpy(m_szMsgBuffer, m_strAccountData.data(), m_strAccountData.length());
	{
		m_szMsgBuffer[m_strAccountData.length()] = 0;
	}

	m_CopyData.cbData = m_strAccountData.length();
	m_CopyData.dwData = m_dwMsgCount;
	m_CopyData.lpData = m_szMsgBuffer;

	DWORD p;
	SendMessageTimeoutW(m_TLHWnd, WM_COPYDATA, WPARAM(m_MainHWnd), LPARAM(&m_CopyData), SMTO_NORMAL, MESSAGE_TIMEOUT, &p);

	if (!p)
	{
		TRACEN("Send message failed: ID_GAME_STR: %d", GetLastError());
	}
	else
	{
		TRACEN("Sent new message: ID_GAME_STR");
	}

	m_bLaunched = true;
}

void CTLauncher::SendTicket()
{
	if (!m_MainHWnd)
	{
		TRACEN("Cannot send messages without launcher window.");
		return;
	}

	if (!m_TLHWnd)
	{
		TRACEN("Cannot send messasge without TL window.");
		return;
	}

	if (m_strAccountData.length() >= MESSAGE_BUF_SIZE)
	{
		TRACEN("CopyData too big for buffer.");
		return;
	}

	if (m_strAccountData.empty())
	{
		TRACEN("Empty account data.");
		return;
	}

	memset(m_szMsgBuffer, 0, MESSAGE_BUF_SIZE);
	memcpy(m_szMsgBuffer, m_strAccountData.c_str(), m_strAccountData.size());

	m_CopyData.cbData = m_strAccountData.size();
	m_CopyData.dwData = m_dwMsgCount;
	m_CopyData.lpData = m_szMsgBuffer;

	DWORD p;
	SendMessageTimeoutW(m_TLHWnd, WM_COPYDATA, WPARAM(m_MainHWnd), LPARAM(&m_CopyData), SMTO_NORMAL, MESSAGE_TIMEOUT, &p);

	if (!p)
	{
		TRACEN("Message timeout: ID_TICKET: %d", GetLastError());
	}
	else
	{
		TRACEN("Sent new message: ID_TICKET");
	}
}

HWND CTLauncher::HWNDGetLauncher()
{
	return m_LauncherHWnd;
}

HWND CTLauncher::HWNDGetMain()
{
	return m_MainHWnd;
}

HWND CTLauncher::HWNDGetTL()
{
	return m_TLHWnd;
}

DWORD CTLauncher::GetTLProcessId()
{
	return m_pInfo.dwProcessId;
}

DWORD CTLauncher::GetStage()
{
	return m_dwStage;
}

bool IsGameEvent(const char* pString)
{
	return boost::starts_with(pString, "gameEvent");
}

std::string GameEventType(std::string str)
{
	return boost::regex_replace(str, boost::regex("[^0-9]*([0-9]+).*"), "\\1");
}

int GetIdent(const LPVOID lpData)
{
	const char* szMessage = reinterpret_cast<const char*>(lpData);

	if (IsGameEvent((szMessage)))
	{
		return std::stoi(GameEventType(szMessage));
	}

	if (!memcmp(lpData, "Hello!!", strlen("Hello!!")))
	{
		return CTLauncher::ID_HELLO;
	}

	if (!memcmp(lpData, "slsurl", strlen("slsurl")))
	{
		return CTLauncher::ID_SLS_URL;
	}

	if (!memcmp(lpData, "gamestr", strlen("gamestr")))
	{
		return CTLauncher::ID_GAME_STR;
	}

	if (!memcmp(lpData, "ticket", strlen("ticket")))
	{
		return CTLauncher::ID_TICKET;
	}

	if (!memcmp(lpData, "endPopup", strlen("endPopup")))
	{
		return CTLauncher::ID_END_POPUP;
	}

	return CTLauncher::ID_UNKNOWN;
}

LRESULT CTLauncher::ProcessClassMessages(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	CTLauncher *CTL = reinterpret_cast<CTLauncher*>(GetWindowLongPtr(hwnd, GWL_USERDATA));

	if (!CTL)
	{
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	COPYDATASTRUCT copyData;

	switch (message)
	{
	case WM_COPYDATA:

		if (!lParam)
		{
			break;
		}

		copyData = *reinterpret_cast<COPYDATASTRUCT*>(lParam);

		if (!CTL->m_TLHWnd)
		{
			CTL->m_TLHWnd = reinterpret_cast<HWND>(wParam);
		}

		if (copyData.lpData)
		{
			TRACEN("New message from TL.exe: %s", std::string(
				reinterpret_cast<const char*>(copyData.lpData),
				reinterpret_cast<const char*>(copyData.lpData) + copyData.cbData).c_str());
		}

		switch (GetIdent(copyData.lpData))
		{
		case CTLauncher::ID_HELLO:

			SendMessage(hwnd, WM_APP, reinterpret_cast<WPARAM>(hwnd), NULL);

			break;

		case CTLauncher::ID_SLS_URL:

			CTL->m_dwStage = ID_SLS_URL;

			break;

		case CTLauncher::ID_GAME_STR:

			CTL->m_dwStage = ID_GAME_STR;

			break;

		case CTLauncher::ID_CHAR_CNT:

			CTL->m_dwStage = ID_CHAR_CNT;

		case CTLauncher::ID_LAST_SVR:

			CTL->m_dwStage = ID_LAST_SVR;

			break;

		case CTLauncher::ID_END_POPUP:

			CTL->m_dwStage = ID_END_POPUP;

			break;

		case CTLauncher::ID_TICKET:

			CTL->m_dwStage = ID_TICKET;

			break;

		default:

			break;
		}

		CTL->m_dwMsgCount++;

		return 1;

	case WM_APP:

		CTL->SendHello();

		return 1;

	case WM_COMMAND:

		break;

	case WM_DESTROY:

		CTL->m_bStopMessageLoop = true;
		PostQuitMessage(0);

		break;

	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

#include "patcher.h"

//////////////////////////
// Patcher <-> Launcher //
//////////////////////////

void PatchThread(CPatcher* pPatcher)
{
	pPatcher->UpdateToLatest();
}

CLauncher::CLauncher(HINSTANCE hInstance) :
	m_paCurrentSize(0),
	m_paRequiredSize(0),
	m_paCurrentUnpackedFileCount(0),
	m_paRequiredUnpackedFileCount(0)
{
#ifdef CEF_CHILD
	Launcher_RegisterClass(hInstance);
#endif
	m_pPatcher = new CPatcher(boost::ref(m_PatchCallback));
}

CLauncher::~CLauncher()
{
	if (m_pPatcher)
	{
		m_pPatcher->Stop();

		delete m_pPatcher;
		m_pPatcher = NULL;
	}
}

bool CLauncher::Patch()
{
	if (!m_pPatcher)
	{
		m_pPatcher = new CPatcher(boost::ref(m_PatchCallback));
	}

	if (!m_pPatcher)
	{
		return false;
	}

	if (!m_pPatcher->Initialize())
	{
		return RetrievePatchStatus();
	}

	m_pPatcher->AsyncUpdateToLatest();

	return true;
}

bool CLauncher::RetrievePatchStatus()
{
	if (!m_pPatcher)
	{
		return false;
	}

	if (m_paRequiredSize == 0)
	{
		TADownloadBegin* download = reinterpret_cast<TADownloadBegin*>(m_PatchCallback.GetStaticAlertOwnership(Callback::kDownloadBegin));

		if(download)
		{
			m_paRequiredSize = download->download_size_total;

			CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("PatchMessage");
			{
				msg->GetArgumentList()->SetString(0, "Download size: " + std::to_string(m_paRequiredSize));
			}

			if (!GetLauncher()->GetBrowser()->SendProcessMessage(PID_BROWSER, msg))
			{
				return false;
			}

			m_paRequiredUnpackedFileCount = 0;
		}
	}

	if (m_paRequiredUnpackedFileCount == 0)
	{
		TAFilePatchBegin* patch = reinterpret_cast<TAFilePatchBegin*>(m_PatchCallback.GetStaticAlertOwnership(Callback::kPatchBegin));

		if (patch)
		{
			m_paRequiredUnpackedFileCount = patch->file_count;

			CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("PatchMessage");
			{
				msg->GetArgumentList()->SetString(0, "Amount of files in archives: " + std::to_string(m_paRequiredUnpackedFileCount));
			}

			if (!GetLauncher()->GetBrowser()->SendProcessMessage(PID_BROWSER, msg))
			{
				return false;
			}

			m_paRequiredSize = 0;
		}
	}

	Callback::TAlerts alerts;
	m_PatchCallback.GetAlerts(alerts);

	for (Callback::TAlerts::iterator it = alerts.begin(); it != alerts.end(); ++it)
	{
		CefRefPtr<CefProcessMessage> msg;

		switch (it->Type())
		{
		case Callback::kMessage:
		case Callback::kFatal:
			msg = CefProcessMessage::Create("PatchMessage");
			{
				msg->GetArgumentList()->SetString(0, it->Message());
			}

			if (!GetLauncher()->GetBrowser()->SendProcessMessage(PID_BROWSER, msg))
			{
				return false;
			}

			break;

		case Callback::kDownload:
			m_paCurrentSize += reinterpret_cast<TADownload*>(&*it)->downloaded;

			break;

		case Callback::kPatch:
			m_paCurrentUnpackedFileCount++;

			break;

		case Callback::kMultiPatch:
			m_paCurrentUnpackedFileCount += reinterpret_cast<TAMultiFilePatch*>(&*it)->file_count;

			break;

		case Callback::kPatchFinish:
			msg = CefProcessMessage::Create("PatchFinish");

			if (!GetLauncher()->GetBrowser()->SendProcessMessage(PID_BROWSER, msg))
			{
				return false;
			}

			break;
		}
	}

	if (m_paRequiredSize != 0 && m_paCurrentSize != 0)
	{
		CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("PatchProgress");
		{
			msg->GetArgumentList()->SetDouble(0, (double(m_paCurrentSize) / double(m_paRequiredSize)) * 100);
		}

		if (!GetLauncher()->GetBrowser()->SendProcessMessage(PID_BROWSER, msg))
		{
			return false;
		}
	}

	if (m_paRequiredUnpackedFileCount != 0 && m_paCurrentUnpackedFileCount != 0)
	{
		CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("PatchProgress");
		{
			msg->GetArgumentList()->SetDouble(0, (double(m_paCurrentUnpackedFileCount) / double(m_paRequiredUnpackedFileCount)) * 100);
		}

		if (!GetLauncher()->GetBrowser()->SendProcessMessage(PID_BROWSER, msg))
		{
			return false;
		}
	}

	return true;
}

bool CLauncher::PausePatch()
{
	if (!m_pPatcher)
	{
		return false;
	}

	if (m_pPatcher->IsStopping())
	{
		return false;
	}

	m_pPatcher->Stop();

	m_paCurrentSize = 0;
	m_paRequiredSize = 0;
	m_paCurrentUnpackedFileCount = 0;
	m_paRequiredUnpackedFileCount = 0;

	m_PatchCallback.ClearAll();

	return true;
}

bool CLauncher::ResumePatch()
{
	if (!m_pPatcher)
	{
		return false;
	}

	if (m_pPatcher->IsStopping())
	{
		return false;
	}

	return Patch();
}

bool CLauncher::IndicateGameDirectory(const std::string & strDirectory)
{
	if (!m_pPatcher)
	{
		return false;
	}

	boost::system::error_code error;

	if (!boost::filesystem::exists(strDirectory, error))
	{
		TRACEN("Invalid TERA path: %s", strDirectory.c_str());
		return false;
	}

	if (error)
	{
		TRACEN("Invalid TERA path: %s. Error: %s", strDirectory.c_str(), error.message().c_str());
		return false;
	}

	if (!m_pPatcher->SetupPath(strDirectory))
	{
		TRACEN("Setting game path to %s failed.", strDirectory.c_str());
		return false;
	}

	return true;
}
