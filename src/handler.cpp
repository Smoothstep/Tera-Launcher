#include "Handler.h"

#include <include/base/cef_bind.h>
#include <include/cef_app.h>
#include <include/wrapper/cef_closure_task.h>
#include <include/wrapper/cef_helpers.h>

#include "launcher.h"

// Needed for Execute JS
std::string TransformPath(std::string string)
{
	size_t size = string.size();
	size_t add = 0;

	for (size_t i = 0; i < size; ++i)
	{
		if (string[i + add] == '\\')
		{
			string.insert(string.begin() + i + add++, '\\');
		}
	}

	return string;
}

std::wstring GetCookies(cookie_container_t& vCookies)
{
	std::wstring cookies;

	for (cookie_container_t::iterator it = vCookies.begin(); it != vCookies.end(); ++it)
	{
		cookies += it->name;
		cookies += L"=";
		cookies += it->value;

		if (it + 1 != vCookies.end())
		{
			cookies += L"; ";
		}
	}

	return cookies;
}

cookie_container_t GetCookies(CefRefPtr<CefResponse> response)
{
	cookie_container_t vCookies;
	std::multimap<CefString, CefString> headers;

	if (response)
	{
		response->GetHeaderMap(headers);
	}

	for (auto it = headers.begin(); it != headers.end(); ++it)
	{
		if (it->first == "Set-Cookie")
		{
			vCookies.push_back(
				cookie_t(
					it->second.ToWString().substr(0, it->second.ToWString().find_first_of('=')),
					it->second.ToWString().substr(it->second.ToWString().find_first_of('=') + 1,
					it->second.ToWString().substr(it->second.ToWString().find_first_of('=') + 1).find_first_of(';'))));
		}
	}

	return vCookies;
}

class CCefCookieVisitor : public CefCookieVisitor
{
private:
	IMPLEMENT_REFCOUNTING(CCefCookieVisitor);

private:
	bool m_bDelete;

protected:
	std::deque<CefCookie> m_Cookies;

public:
	CCefCookieVisitor(bool bDelete = false) : m_bDelete(bDelete) {}

	virtual bool Visit(const CefCookie& cookie, int count, int total, bool& deleteCookie) OVERRIDE
	{
		deleteCookie = m_bDelete;
		m_Cookies.push_back(cookie);
		return true;
	}

	void Clear()
	{
		m_Cookies.clear();
	}

	std::deque<CefCookie>& GetCookies()
	{
		return m_Cookies;
	}
};

class CRequestContextHandler : public CefRequestContextHandler
{
private:
	IMPLEMENT_REFCOUNTING(CRequestContextHandler);

public:
	virtual CefRefPtr<CefCookieManager> GetCookieManager() OVERRIDE
	{
		return CefCookieManager::GetGlobalManager(NULL);
	}

	std::deque<CefCookie> GetCookies()
	{
		if (!GetCookieManager())
		{
			return std::deque<CefCookie>();
		}

		CefRefPtr<CCefCookieVisitor> visitor;
		{
			GetCookieManager()->VisitAllCookies(visitor);
		}

		return visitor->GetCookies();
	}
};

static CCefHandler*	g_MainHandler = NULL;

CCefHandler::CCefHandler()
{
	g_MainHandler = this;
}

CCefHandler::~CCefHandler()
{
	g_MainHandler = NULL;
}

CCefHandler * CCefHandler::GetInstance()
{
	return g_MainHandler;
}

/*
*	Executed by Browser Process
*/
bool CCefHandler::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser,
	CefProcessId source_process,
	CefRefPtr<CefProcessMessage> message)
{
	if (!GetLauncher())
	{
		return false;
	}

	if (message->GetName() == "Start")
	{
		return GetLauncher()->Launch(GetModuleHandle(NULL));
	}
	else if (message->GetName() == "Account")
	{
		if (message->GetArgumentList()->GetSize() == 0)
		{
			return false;
		}

		return GetLauncher()->SetAccountData(message->GetArgumentList()->GetString(0).ToString());
	}
	else if (message->GetName() == "Patch")
	{
		return GetLauncher()->Patch();
	}
	else if (message->GetName() == "PatchMessage")
	{
		if (message->GetArgumentList()->GetSize() == 0)
		{
			return false;
		}

		return SetPatchMessage(message->GetArgumentList()->GetString(0));
	}
	else if (message->GetName() == "PatchProgress")
	{
		if (message->GetArgumentList()->GetSize() == 0)
		{
			return false;
		}

		return SetPatchProgress(message->GetArgumentList()->GetDouble(0));
	}
	else if (message->GetName() == "PatchFinish")
	{
		return FinishPatch();
	}
	else if (message->GetName() == "LoginResult")
	{
		if (message->GetArgumentList()->GetSize() == 0)
		{
			return false;
		}

		return LoginResult(message->GetArgumentList()->GetBool(0));
	}
	else if (message->GetName() == "Logout")
	{
		return Logout();
	}
	else if (message->GetName() == "SetSLURL")
	{
		if (message->GetArgumentList()->GetSize() == 0)
		{
			return false;
		}

		return GetLauncher()->IndicateSLURL(message->GetArgumentList()->GetString(0));
	}
	else if (message->GetName() == "SetTLPath")
	{
		if (message->GetArgumentList()->GetSize() == 0)
		{
			return false;
		}

		return GetLauncher()->IndicateTLPath(message->GetArgumentList()->GetString(0));
	}
	else if (message->GetName() == "UpdateConfig")
	{
		return GetLauncher()->ReadConfiguration();
	}

	return false;
}

void CCefHandler::OnLoadStart(CefRefPtr<CefBrowser> browser,
CefRefPtr<CefFrame> frame)
{
}

void CCefHandler::OnResourceLoadComplete(CefRefPtr<CefBrowser> browser,
CefRefPtr<CefFrame> frame,
CefRefPtr<CefRequest> request,
CefRefPtr<CefResponse> response,
URLRequestStatus status,
int64 received_content_length)
{
}

bool CCefHandler::OnResourceResponse(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefRequest> request,
	CefRefPtr<CefResponse> response)
{
	return false;
}

CefRequestHandler::ReturnValue CCefHandler::OnBeforeResourceLoad(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefRequest> request,
	CefRefPtr<CefRequestCallback> callback)
{
	return RV_CONTINUE;
}

void CCefHandler::OnResourceRedirect(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefRequest> request,
	CefString& new_url)
{
}

void CCefHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser,
CefRefPtr<CefFrame> frame,
int httpStatusCode)
{
}

void CCefHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
	m_Browser = browser;
	m_Browser->SendProcessMessage(PID_RENDERER, 
		CefProcessMessage::Create("Browser"));

	CefRefPtr<CefCookieManager> cookieManager = CefCookieManager::GetGlobalManager(NULL);

	if (cookieManager)
	{
		cookieManager->VisitAllCookies(new CCefCookieVisitor(true));
	}
}

void CCefHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
	CEF_REQUIRE_UI_THREAD();
	{
		CefQuitMessageLoop();
	}
}

void CCefHandler::Close() 
{
	if (m_Browser) 
	{
		m_Browser->GetHost()->CloseBrowser(false);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////
//								JS call up functions
////////////////////////////////////////////////////////////////////////////////////////////

bool SetPatchProgress(double progress)
{
	if (!CEF)
	{
		return false;
	}

	if (!CEF->Browser())
	{
		return false;
	}

	CefRefPtr<CefFrame> frame = CEF->Browser()->GetMainFrame();

	if (!frame)
	{
		return false;
	}

	frame->ExecuteJavaScript("SetProgress(" + std::to_string(floor(progress)) + ");", frame->GetURL(), 0);

	return true;
}

bool SetPatchMessage(const std::string& msg)
{
	if (!CEF)
	{
		return false;
	}

	if (!CEF->Browser())
	{
		return false;
	}

	CefRefPtr<CefFrame> frame = CEF->Browser()->GetMainFrame();

	if (!frame)
	{
		return false;
	}

	frame->ExecuteJavaScript("SetProgressMessage(\"" + TransformPath(msg) + "\");", frame->GetURL(), 0);

	return true;
}

bool FinishPatch()
{
	if (!CEF)
	{
		return false;
	}

	if (!CEF->Browser())
	{
		return false;
	}

	CefRefPtr<CefFrame> frame = CEF->Browser()->GetMainFrame();

	if (!frame)
	{
		return false;
	}

	frame->ExecuteJavaScript("FinishPatch();", frame->GetURL(), 0);

	return true;
}

bool SetTeraDirectory(const std::string& path)
{
	if (!CEF)
	{
		return false;
	}

	if (!CEF->Browser())
	{
		return false;
	}

	CefRefPtr<CefFrame> frame = CEF->Browser()->GetMainFrame();

	if (!frame)
	{
		return false;
	}

	frame->ExecuteJavaScript("SetGameDirectory(" + path + ");", frame->GetURL(), 0);

	return true;
}

bool GetAccountInfo()
{
	if (!CEF)
	{
		return false;
	}

	if (!CEF->Browser())
	{
		return false;
	}

	CefRefPtr<CefFrame> frame = CEF->Browser()->GetMainFrame();

	if (!frame)
	{
		return false;
	}

	frame->ExecuteJavaScript("GetAccountInfo();", frame->GetURL(), 0);

	return true;
}

bool LoginResult(bool bSucceed)
{
	if (!GetLauncher())
	{
		return false;
	}

	if (!GetLauncher()->GetBrowser())
	{
		return false;
	}

	CefRefPtr<CefFrame> frame = GetLauncher()->GetBrowser()->GetMainFrame();

	if (!frame)
	{
		return false;
	}

	if (bSucceed)
	{
		frame->ExecuteJavaScript("LoginResult(1);", frame->GetURL(), 0);
	}
	else
	{
		frame->ExecuteJavaScript("LoginResult(0);", frame->GetURL(), 0);
	}

	return true;
}

bool Logout()
{
	if (!GetLauncher())
	{
		return false;
	}

	if (!GetLauncher()->GetBrowser())
	{
		return false;
	}

	CefRefPtr<CefFrame> frame = GetLauncher()->GetBrowser()->GetMainFrame();

	if (!frame)
	{
		return false;
	}

	frame->ExecuteJavaScript("Logout();", frame->GetURL(), 0);

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
//								Gameforge login routine
////////////////////////////////////////////////////////////////////////////////////////////

class CInfoRequestClient : public CCefURLRequestClient
{
	IMPLEMENT_REFCOUNTING(CInfoRequestClient);
public:
	virtual void OnDownloadData(CefRefPtr<CefURLRequest> request,
		const void* data,
		size_t data_length) OVERRIDE
	{
		const char* szData = reinterpret_cast<const char*>(data);

		if (GetLauncher())
		{
			if (!GetLauncher()->GetBrowser())
			{
				return;
			}

			CefRefPtr<CefFrame> frame = GetLauncher()->GetBrowser()->GetMainFrame();

			if (!frame)
			{
				return;
			}

			std::string strAccount(std::string(szData, data_length));

			if (ValidAccount(strAccount))
			{
				CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("Account");
				{
					msg->GetArgumentList()->SetString(0, strAccount);
				}

				GetLauncher()->GetBrowser()->SendProcessMessage(PID_BROWSER, msg);

				frame->ExecuteJavaScript("LoginResult(1);", frame->GetURL(), 0);
			}
			else
			{
				frame->ExecuteJavaScript("LoginResult(0);", frame->GetURL(), 0);
			}
		}
	}
};

void SendRequestGetInfo(std::vector<SCookie>& vCookies)
{
	static cookie_container_t g_vCookies;

	if (vCookies.empty())
	{
		vCookies.insert(vCookies.begin(), g_vCookies.begin(), g_vCookies.end());
	}
	else
	{
		g_vCookies.insert(g_vCookies.begin(), vCookies.begin(), vCookies.end());
	}

	std::multimap<CefString, CefString> headerMap;
	{
		headerMap.insert(std::make_pair("Referer", "https://account.tera.gameforge.com/launcher/1"));
		headerMap.insert(std::make_pair("Host", "account.tera.gameforge.com"));
		headerMap.insert(std::make_pair("X-Requested-With", "XMLHttpRequest"));
		headerMap.insert(std::make_pair("Accept", "*/*"));
		headerMap.insert(std::make_pair("Accept-Charset", "iso-8859-1,*,utf-8"));
		headerMap.insert(std::make_pair("Accept-Encoding", "gzip,deflate"));
		headerMap.insert(std::make_pair("UserAgent", "Chrome/29.0.1046.0"));
		headerMap.insert(std::make_pair("Cookie", GetCookies(vCookies)));
	}

	CefRefPtr<CefRequest> request(CefRequest::Create());
	{
		request->SetFlags(UR_FLAG_ALLOW_CACHED_CREDENTIALS);
		request->SetURL("https://account.tera.gameforge.com/launcher/1/account_server_info?attach_auth_ticket=1");
		request->SetHeaderMap(headerMap);
		request->SetMethod("GET");
	}

	CefRequestContextSettings settings;
	CefRefPtr<CefURLRequest> urlRequest = CefURLRequest::Create(request, new CInfoRequestClient,
		CefRequestContext::CreateContext(settings, new CRequestContextHandler));
}

class CAuthenticationClient : public CCefURLRequestClient
{
	IMPLEMENT_REFCOUNTING(CAuthenticationClient);

public:
	virtual void OnRequestComplete(CefRefPtr<CefURLRequest> request) OVERRIDE
	{
		SendRequestGetInfo(GetCookies(request->GetResponse()));
	}
};

void SendRequestAuthenticate(std::string& email, std::string& pw, std::vector<SCookie> vCookies)
{
	std::multimap<CefString, CefString> headerMap;
	{
		headerMap.insert(std::make_pair("Referer", "https://account.tera.gameforge.com/launcher/1/signin?lang=en&email=" + email + "&kid="));
		headerMap.insert(std::make_pair("ContentType", "application/x-www-form-urlencoded"));
		headerMap.insert(std::make_pair("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"));
		headerMap.insert(std::make_pair("Host", "account.tera.gameforge.com"));
		headerMap.insert(std::make_pair("Cookie", GetCookies(vCookies)));
	}

	std::string strPostData = "uft8=%E2%9C%93&user[client_time]=Thu Mar 20 2016 15:00:00 GMT+0100 (Central Standard Time)&user[io_black_box]=TERA&game_id=1&user[email]=" + email + "&user[password]=" + pw;

	CefRefPtr<CefPostDataElement> postData = CefPostDataElement::Create();
	{
		postData->SetToBytes(strPostData.length(), strPostData.data());
	}

	CefRefPtr<CefPostData> post = CefPostData::Create();
	{
		post->AddElement(postData);
	}

	CefRefPtr<CefRequest> request(CefRequest::Create());
	{
		request->SetFlags(UR_FLAG_ALLOW_CACHED_CREDENTIALS);
		request->SetURL("https://account.tera.gameforge.com/launcher/1/authenticate");
		request->SetHeaderMap(headerMap);
		request->SetPostData(post);
		request->SetMethod("POST");
	}

	CefRequestContextSettings settings;
	CefRefPtr<CefURLRequest> urlRequest = CefURLRequest::Create(request, new CAuthenticationClient,
		CefRequestContext::CreateContext(settings, new CRequestContextHandler));
}

class CLoginRequestClient : public CCefURLRequestClient
{
	IMPLEMENT_REFCOUNTING(CLoginRequestClient);

private:
	std::string m_Email;
	std::string m_Pw;

public:
	CLoginRequestClient(std::string email, std::string pw)
	{
		m_Email = email;
		m_Pw = pw;
	}

	virtual void OnRequestComplete(CefRefPtr<CefURLRequest> request) OVERRIDE
	{
		SendRequestAuthenticate(m_Email, m_Pw, GetCookies(request->GetResponse()));
	}
};

void SendRequestLogin(const std::string& email, const std::string& pw)
{
	CefRefPtr<CefRequest> request(CefRequest::Create());
	{
		request->SetFlags(UR_FLAG_ALLOW_CACHED_CREDENTIALS);
		request->SetURL("https://account.tera.gameforge.com/launcher/1/signin?lang=en&email=" + email + "&kid=");
		request->SetMethod("GET");
	}

	CefRequestContextSettings settings;
	CefRefPtr<CefURLRequest> urlRequest = CefURLRequest::Create(request, new CLoginRequestClient(email, pw),
		CefRequestContext::CreateContext(settings, new CRequestContextHandler));
}