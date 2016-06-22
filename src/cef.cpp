#include "cef.h"

#include <include/cef_browser.h>
#include <include/cef_command_line.h>
#include <include/wrapper/cef_helpers.h>
#include <include/cef_render_process_handler.h>

#include <Shlobj.h>

#include "launcher.h"
#include "extension.h"

/*
*	Executed by Render process
*/
bool CCefApp::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser,
	CefProcessId source_process,
	CefRefPtr<CefProcessMessage> message)
{
	if (message->GetName() == "Browser")
	{
		m_MainBrowser = browser;
	}
	else if (message->GetName() == "AccountInfo")
	{
		GetAccountInfo();
	}

	return false;
}

void CCefApp::OnContextInitialized()
{
	CEF_REQUIRE_UI_THREAD();;

	CefWindowInfo wInfo;
	
#ifdef CEF_CHILD
	RECT rect;
	GetClientRect(GetLauncher()->HWNDGetLauncher(), &rect)

	wInfo.SetAsChild(GetLauncher()->HWNDGetLauncher(), rect);
#else
	wInfo.style = WS_POPUP | WS_CHILDWINDOW;
	wInfo.SetAsPopup(NULL, "Tera Launcher");
#endif

	CefBrowserSettings settings;
	if (!CefBrowserHost::CreateBrowser(wInfo, 
		new CCefHandler, LOGIN_HTML, settings, NULL))
	{
		MessageBox(GetActiveWindow(), 
			L"Unable to create browser.", L"Unable to create browser.", 0); 
		abort();
	}
}

class CLauncherAccessor : public CefV8Accessor 
{
	IMPLEMENT_REFCOUNTING(CLauncherAccessor);

public:
	virtual bool Get(
		const CefString& name,
		const CefRefPtr<CefV8Value> object,
		CefRefPtr<CefV8Value>& retval,
		CefString& exception) OVERRIDE 
	{
		if (!GetLauncher())
		{
			exception = "No Launcher object";
			return true;
		}

		if (name == "SLS_URL") 
		{
			retval = CefV8Value::CreateString(GetLauncher()->GetServerList());
			return true;
		}
		else if (name == "ACC_DATA")
		{
			retval = CefV8Value::CreateString(GetLauncher()->GetAccountData());
			return true;
		}

		return false;
	}

	virtual bool Set(
		const CefString& name,
		const CefRefPtr<CefV8Value> object,
		const CefRefPtr<CefV8Value> value,
		CefString& exception) OVERRIDE 
	{
		if (!GetLauncher())
		{
			exception = "No Launcher object";
			return true;
		}

		if (!GetLauncher()->GetBrowser())
		{
			exception = "No browser object";
			return true;
		}

		if (name == "SLS_URL") 
		{
			if (value->IsString()) 
			{
				if (!GetLauncher()->IndicateSLURL(value->GetStringValue()))
				{
					exception = "Invalid Serverlist url";
				}
				else
				{
					GetLauncher()->GetBrowser()->SendProcessMessage(PID_BROWSER,
						CefProcessMessage::Create("UpdateConfig"));
				}
			}
			else
			{
				exception = "Invalid value type";
			}

			return true;
		}
		else if (name == "ACC_DATA")
		{
			if (value->IsString())
			{
				if (!ValidAccount(value->GetStringValue()))
				{
					exception = "Invalid Account";
				}
				else
				{
					CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("Account");
					{
						msg->GetArgumentList()->SetString(0, value->GetStringValue());
					}

					GetLauncher()->GetBrowser()->SendProcessMessage(PID_BROWSER, msg);
				}
			}
			else
			{
				exception = "Invalid value type";
			}

			return true;
		}

		return false;
	}
};

void CCefApp::OnContextCreated(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefV8Context> context) 
{
	CefRefPtr<CefV8Handler> handler	= new CCefV8Handler();
	CefRefPtr<CefV8Value> object	= context->GetGlobal();

	CefRefPtr<CefV8Value> fAccessorObj = CefV8Value::CreateObject(new CLauncherAccessor);

	fAccessorObj->SetValue("SLS_URL",	V8_ACCESS_CONTROL_DEFAULT, V8_PROPERTY_ATTRIBUTE_NONE);
	fAccessorObj->SetValue("ACC_DATA",	V8_ACCESS_CONTROL_DEFAULT, V8_PROPERTY_ATTRIBUTE_NONE);

	object->SetValue("Launcher", fAccessorObj, V8_PROPERTY_ATTRIBUTE_NONE);

	CefRefPtr<CefV8Value> fOnLogin		= CefV8Value::CreateFunction("OnLogin",				handler);
	CefRefPtr<CefV8Value> fOnStart		= CefV8Value::CreateFunction("OnStart",				handler);
	CefRefPtr<CefV8Value> fOnPatch		= CefV8Value::CreateFunction("OnPatch",				handler);
	CefRefPtr<CefV8Value> fResumePatch	= CefV8Value::CreateFunction("ResumePatch",			handler);
	CefRefPtr<CefV8Value> fPausePatch	= CefV8Value::CreateFunction("PausePatch",			handler);
	CefRefPtr<CefV8Value> fPatchStatus	= CefV8Value::CreateFunction("GetPatchStatus",		handler);
	CefRefPtr<CefV8Value> fAccountData	= CefV8Value::CreateFunction("SetAccountData",		handler);
	CefRefPtr<CefV8Value> fGetTeraDir	= CefV8Value::CreateFunction("GetTeraDirectory",	handler);

	object->SetValue("OnLogin",			fOnLogin,		V8_PROPERTY_ATTRIBUTE_NONE);
	object->SetValue("OnStart",			fOnStart,		V8_PROPERTY_ATTRIBUTE_NONE);
	object->SetValue("OnPatch",			fOnPatch,		V8_PROPERTY_ATTRIBUTE_NONE);
	object->SetValue("ResumePatch",		fResumePatch,	V8_PROPERTY_ATTRIBUTE_NONE);
	object->SetValue("PausePatch",		fPausePatch,	V8_PROPERTY_ATTRIBUTE_NONE);
	object->SetValue("GetPatchStatus",	fPatchStatus,	V8_PROPERTY_ATTRIBUTE_NONE);
	object->SetValue("SetAccountData",	fAccountData,	V8_PROPERTY_ATTRIBUTE_NONE);
	object->SetValue("GetTeraDirectory",fGetTeraDir,	V8_PROPERTY_ATTRIBUTE_NONE);

	CefRefPtr<CefV8Value> fCreateXSRequest = CefV8Value::CreateFunction("CreateXSRequest", handler);

	object->SetValue("CreateXSRequest", fCreateXSRequest, V8_PROPERTY_ATTRIBUTE_NONE);
}

void CCefApp::OnWebKitInitialized()
{
#ifdef _USE_OFFICIAL_PATCHER_
	std::string extensionCode =
		"function CopyCub() { this.id=0; };"
		"function parent()	{ this.copyCub=0; };"
		"parent.copyCub =	{ getString: function() { return 1; } };";

	if (!CefRegisterExtension("v8/copyCub", extensionCode, new CCefV8Handler()))
	{
		MessageBox(GetActiveWindow(), 
			L"Failed to setup extension.", L"Failed to setup extension.", 0);
		abort();
	}
#endif
}

// Processed by Render process
bool CCefV8Handler::Execute(
	const CefString& name,
	CefRefPtr<CefV8Value> object,
	const CefV8ValueList& arguments,
	CefRefPtr<CefV8Value>& retval,
	CefString& exception)
{
	if (name == "OnLogin")
	{
		if (arguments.size() != 2)
		{
			return true;
		}

		CefRefPtr<CefV8Value> email		= arguments[0];
		CefRefPtr<CefV8Value> password	= arguments[1];

		if (!email->IsString())
		{
			return true;
		}

		if (!password->IsString())
		{
			return true;
		}

		if (email->GetStringValue().empty())
		{
			return true;
		}

		if (password->GetStringValue().empty())
		{
			return true;
		}

		std::string sEmail		= email->GetStringValue().ToString();
		std::string sPassword	= password->GetStringValue().ToString();

		SendRequestLogin(sEmail, sPassword);

		retval = CefV8Value::CreateBool(true);

		return true;
	}
	else if (name == "OnStart")
	{
		if (!GetLauncher())
		{
			retval = CefV8Value::CreateBool(false);
			return true;
		}

		if (!GetLauncher()->GetBrowser())
		{
			retval = CefV8Value::CreateBool(false);
			return true;
		}

		if (!GetLauncher()->GetBrowser()->SendProcessMessage(PID_BROWSER, 
			CefProcessMessage::Create("Start")))
		{
			retval = CefV8Value::CreateBool(false);
			return true;
		}

		retval = CefV8Value::CreateBool(true);
		return true;
	}
	else if (name == "OnPatch")
	{
		if (!GetLauncher())
		{
			retval = CefV8Value::CreateBool(false);
			return true;
		}

		if (!GetLauncher()->GetBrowser())
		{
			retval = CefV8Value::CreateBool(false);
			return true;
		}

		if (GetLauncher())
		{
			if (GetLauncher()->Patch())
			{
				retval = CefV8Value::CreateBool(true);
				return true;
			}
		}

		retval = CefV8Value::CreateBool(false);
		return true;
	}
	else if (name == "PausePatch")
	{
		if (GetLauncher())
		{
			if (GetLauncher()->PausePatch())
			{
				retval = CefV8Value::CreateBool(true);
				return true;
			}
		}

		retval = CefV8Value::CreateBool(false);
		return true;
	}
	else if (name == "ResumePatch")
	{
		if (GetLauncher())
		{
			if (GetLauncher()->ResumePatch())
			{
				retval = CefV8Value::CreateBool(true);
				return true;
			}
		}

		retval = CefV8Value::CreateBool(false);
		return true;
	}
	else if (name == "GetPatchStatus")
	{
		if (!GetLauncher())
		{
			retval = CefV8Value::CreateBool(false);
			return true;
		}

		retval = CefV8Value::CreateBool(GetLauncher()->RetrievePatchStatus());

		return true;
	}
	else if (name == "CreateXSRequest")
	{
		Extension::CXSHttpRequest* pRequest = new Extension::CXSHttpRequest();

		if (pRequest)
		{
			retval = pRequest->CreateObject();
		}

		return true;
	}
	else if (name == "SetAccountData")
	{
		if (!GetLauncher())
		{
			retval = CefV8Value::CreateBool(false);
			return true;
		}

		if (!GetLauncher()->GetBrowser())
		{
			retval = CefV8Value::CreateBool(false);
			return true;
		}

		if (arguments.empty())
		{
			retval = CefV8Value::CreateBool(false);
			return true;
		}

		if (!arguments[0]->IsString())
		{
			retval = CefV8Value::CreateBool(false);
			return true;
		}

		CefString data = arguments[0]->GetStringValue();

		if (!ValidAccount(data.ToString()))
		{
			retval = CefV8Value::CreateBool(false);
			return true;
		}

		CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("Account");
		{
			msg->GetArgumentList()->SetString(0, data);
		}

		if (!GetLauncher()->GetBrowser()->SendProcessMessage(PID_BROWSER, msg))
		{
			retval = CefV8Value::CreateBool(false);
			return true;
		}

		retval = CefV8Value::CreateBool(true);
		return true;
	}
	else if (name == "GetTeraDirectory")
	{
		if (!GetLauncher())
		{
			return true;
		}

		if (!GetLauncher()->GetBrowser())
		{
			return true;
		}

		CefString strInitialPath = "";

		if (!arguments.empty())
		{
			if (!arguments[0]->IsString())
			{
				return true;
			}

			strInitialPath = arguments[0]->GetStringValue();
		}

		BROWSEINFOA browseInfo = { 0 };

		browseInfo.lpszTitle= "Browse Directory";
		browseInfo.hwndOwner= GetForegroundWindow();
		browseInfo.lParam	= reinterpret_cast<LPARAM>(strInitialPath.ToString().c_str());

		LPITEMIDLIST lpList = SHBrowseForFolderA(&browseInfo);
		
		if (lpList != NULL)
		{
			char path[_MAX_PATH] = { 0 };

			if (SHGetPathFromIDListA(lpList, path))
			{
				if (GetLauncher()->IndicateGameDirectory(path))
				{
					retval = CefV8Value::CreateString(path);

					CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("SetTLPath");
					{
						msg->GetArgumentList()->SetString(0, std::string(path) + "\\" DEFAULT_TL_PATH);
					}

					GetLauncher()->GetBrowser()->SendProcessMessage(PID_BROWSER, msg);
				}
			}

			CoTaskMemFree(lpList);
		}

		return true;
	}

	return false;
}

void CefRun()
{
	CefRunMessageLoop();
	DoCefShutdown();
}
