#ifndef _CEF_HANDLER_H_
#define _CEF_HANDLER_H_

#pragma once

#include <include\cef_client.h>
#include <include\cef_urlrequest.h>
#include <include\cef_app.h>
#include <include\cef_render_process_handler.h>

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
	CefRefPtr<CefCookieManager> m_CoookieManager = CefCookieManager::CreateManager("C:\\cookies", true, NULL);

public:
	std::vector<CefCookie> m_vCookies;
};

#define CEF CCefHandler::GetInstance()

struct SCookie
{
	std::wstring name;
	std::wstring value;
};

extern void SendRequestGetInfo(std::vector<SCookie>& vCookies);

extern void SendRequestLogin(const std::string& email, const std::string& pw);

extern bool SetPatchProgress(double progress);
extern bool SetPatchMessage(std::string msg);
extern bool FinishPatch();

extern bool LoginResult(bool b);
extern bool Logout();

#endif