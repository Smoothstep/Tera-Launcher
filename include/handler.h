#ifndef _CEF_HANDLER_H_
#define _CEF_HANDLER_H_

#pragma once

#include <include\cef_client.h>
#include <include\cef_urlrequest.h>
#include <include\cef_app.h>
#include <include\cef_render_process_handler.h>

class CCefURLRequestClient : public CefURLRequestClient
{
public:
	virtual void OnRequestComplete(CefRefPtr<CefURLRequest> request) OVERRIDE {}

	virtual void OnUploadProgress(CefRefPtr<CefURLRequest> request,
		int64 current,
		int64 total) OVERRIDE {}

	virtual void OnDownloadProgress(CefRefPtr<CefURLRequest> request,
		int64 current,
		int64 total) OVERRIDE {}

	virtual void OnDownloadData(CefRefPtr<CefURLRequest> request,
		const void* data,
		size_t data_length) OVERRIDE {}

	virtual bool GetAuthCredentials(bool isProxy,
		const CefString& host,
		int port,
		const CefString& realm,
		const CefString& scheme,
		CefRefPtr<CefAuthCallback> callback) OVERRIDE
	{
		return false;
	}
};

class CCefHandler : 
	public CefRequestHandler,
	public CefClient, 
	public CefDisplayHandler, 
	public CefLifeSpanHandler, 
	public CefLoadHandler
{
public:
	CCefHandler();
	~CCefHandler();

	static CCefHandler* GetInstance();

	virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() OVERRIDE 
	{
		return this;
	}

	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() OVERRIDE 
	{
		return this;
	}

	virtual CefRefPtr<CefLoadHandler> GetLoadHandler() OVERRIDE 
	{
		return this;
	}

	virtual CefRefPtr<CefRequestHandler> GetRequestHandler() OVERRIDE 
	{
		return this;
	}

	virtual bool OnProcessMessageReceived(
		CefRefPtr<CefBrowser> browser,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message) OVERRIDE;

	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE;

	virtual bool OnResourceResponse(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefRequest> request,
		CefRefPtr<CefResponse> response) OVERRIDE;

	virtual void OnResourceLoadComplete(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefRequest> request,
		CefRefPtr<CefResponse> response,
		URLRequestStatus status,
		int64 received_content_length) OVERRIDE;
	
	virtual void OnLoadStart(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame) OVERRIDE;

	virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		int httpStatusCode) OVERRIDE;

	virtual void OnResourceRedirect(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefRequest> request,
		CefString& new_url) OVERRIDE;

	virtual ReturnValue OnBeforeResourceLoad(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefRequest> request,
		CefRefPtr<CefRequestCallback> callback) OVERRIDE;
		
	void OnBeforeClose(CefRefPtr<CefBrowser> browser) OVERRIDE;
	void Close();

	CefRefPtr<CefCookieManager> CookieManager()
	{
		return m_CoookieManager;
	}

	CefRefPtr<CefBrowser> Browser()
	{
		return m_Browser;
	}

private:
	IMPLEMENT_REFCOUNTING(CCefHandler);

	CefRefPtr<CefBrowser> m_Browser;
	CefRefPtr<CefCookieManager> m_CoookieManager;
};

#define CEF CCefHandler::GetInstance()

//////////////////////////////////////

struct SCookie
{
	SCookie() {}
	SCookie(std::wstring& Name, std::wstring& Value) : name(Name), value(Value) {}
	SCookie(CefCookie& cookie)
	{
		name	= std::wstring(cookie.name.str, cookie.name.str + cookie.name.length);
		value	= std::wstring(cookie.value.str, cookie.value.str + cookie.value.length);
	}
	std::wstring name;
	std::wstring value;
};

typedef SCookie cookie_t;
typedef std::vector<cookie_t> cookie_container_t;
typedef std::multimap<CefString, CefString> header_map_t;

extern std::wstring GetCookies(cookie_container_t& vCookies);
extern cookie_container_t GetCookies(CefRefPtr<CefResponse> response);

//////////////////////////////////////

extern bool SetTeraDirectory(const std::string& path);

extern bool GetAccountInfo();

extern bool SetPatchProgress(double progress);
extern bool SetPatchMessage(const std::string& msg);
extern bool FinishPatch();

extern bool LoginResult(bool b);
extern bool Logout();

//////////////////////////////////////

extern void SendRequestGetInfo(std::vector<SCookie>& vCookies);
extern void SendRequestLogin(const std::string& email, const std::string& pw);

#endif