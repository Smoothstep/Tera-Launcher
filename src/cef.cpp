#include "cef.h"

#include <include/cef_browser.h>
#include <include/cef_command_line.h>
#include <include/wrapper/cef_helpers.h>
#include <include/cef_render_process_handler.h>

#include "launcher.h"
#include "extension.h"

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
	if (!CefBrowserHost::CreateBrowser(wInfo, new CCefHandler, "file:///config/login.html", settings, NULL))
	{
		MessageBox(GetActiveWindow(), L"Unable to create browser.", L"Unable to create browser.", 0); abort();
	}
}

void CCefApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefV8Context> context) 
{
	CefRefPtr<CefV8Handler> handler	= new CCefV8Handler();
	CefRefPtr<CefV8Value> object	= context->GetGlobal();

	CefRefPtr<CefV8Value> fOnLogin		= CefV8Value::CreateFunction("OnLogin", handler);
	CefRefPtr<CefV8Value> fOnStart		= CefV8Value::CreateFunction("OnStart", handler);
	CefRefPtr<CefV8Value> fOnPatch		= CefV8Value::CreateFunction("OnPatch", handler);
	CefRefPtr<CefV8Value> fResumePatch	= CefV8Value::CreateFunction("ResumePatch", handler);
	CefRefPtr<CefV8Value> fPausePatch	= CefV8Value::CreateFunction("PausePatch", handler);
	CefRefPtr<CefV8Value> fPatchStatus	= CefV8Value::CreateFunction("GetPatchStatus", handler);
	CefRefPtr<CefV8Value> fAccountData	= CefV8Value::CreateFunction("SetAccountData", handler);

	object->SetValue("OnLogin",			fOnLogin,		V8_PROPERTY_ATTRIBUTE_NONE);
	object->SetValue("OnStart",			fOnStart,		V8_PROPERTY_ATTRIBUTE_NONE);
	object->SetValue("OnPatch",			fOnPatch,		V8_PROPERTY_ATTRIBUTE_NONE);
	object->SetValue("ResumePatch",		fResumePatch,	V8_PROPERTY_ATTRIBUTE_NONE);
	object->SetValue("PausePatch",		fPausePatch,	V8_PROPERTY_ATTRIBUTE_NONE);
	object->SetValue("GetPatchStatus",	fPatchStatus,	V8_PROPERTY_ATTRIBUTE_NONE);
	object->SetValue("SetAccountData", fAccountData,	V8_PROPERTY_ATTRIBUTE_NONE);

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
		MessageBox(GetActiveWindow(), L"Failed to setup extension.", L"Failed to setup extension.", 0);
		abort();
	}
#endif
}

bool CCefV8Handler::Execute(const CefString& name,
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

		if (!GetLauncher()->GetBrowser()->SendProcessMessage(PID_BROWSER, CefProcessMessage::Create("Start")))
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

		retval = pRequest->CreateObject();

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

	return false;
}

void CefRun()
{
	CefRunMessageLoop();
	DoCefShutdown();
}
