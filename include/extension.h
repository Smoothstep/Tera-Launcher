#ifndef __EXTENSION_H__
#define __EXTENSION_H__

/*
*	Implementation for Cross Site requests and responses.
*/

#pragma once

#include "handler.h"

#include <mutex>
#include <condition_variable>

namespace Extension
{
	extern CefRefPtr<CefCookieManager> g_pCookieManager = NULL;

	extern bool SetCookieManager(const std::string& path)
	{
		g_pCookieManager = CefCookieManager::CreateManager(path, true, NULL);

		if (!g_pCookieManager)
		{
			return false;
		}

		g_pCookieManager->AddRef();

		return true;
	}

	typedef std::pair<CefRefPtr<CefV8Context>, CefRefPtr<CefV8Value> >	TCallback;
	typedef std::map<std::pair<std::string, int>, TCallback >			TCallbackMap;

	class CCookieVisitor : public CefCookieVisitor
	{
		IMPLEMENT_REFCOUNTING(CCookieVisitor);

	protected:
		std::deque<CefCookie> m_Cookies;

	public:
		virtual bool Visit(const CefCookie& cookie, int count, int total, bool& deleteCookie) OVERRIDE
		{
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
		IMPLEMENT_REFCOUNTING(CRequestContextHandler);

	public:
		virtual CefRefPtr<CefCookieManager> GetCookieManager() OVERRIDE
		{
			return CefCookieManager::GetGlobalManager(NULL);
		}
	};

	class RequestClient : public CCefURLRequestClient
	{
	public:
		virtual void OnRequestComplete(CefRefPtr<CefURLRequest> request) OVERRIDE
		{
			auto cookies = GetCookies(request->GetResponse());

			std::multimap<CefString, CefString> headers;
			request->GetResponse()->GetHeaderMap(headers);

			if (cookies.empty())
			{

			}

			return;
		}
	};

	class CXSHttpResponse : public CefBase
	{
		IMPLEMENT_REFCOUNTING(CXSHttpResponse);

	private:
		cookie_container_t	m_vCookies;
		std::string			m_strDownloadData;

		TCallback m_CookieCB;
		TCallback m_DownloadDataCB;

		CefRefPtr<CefV8Value> m_Response;
		CefRefPtr<CefV8Value> m_Request;

		bool OnGetResponse()
		{
			CefRefPtr<CefV8Value> callback = m_CookieCB.second;
			CefRefPtr<CefV8Context> context = m_CookieCB.first;

			if (!context)
			{
				return false;
			}

			if (!callback)
			{
				return false;
			}

			if (!callback->IsFunction())
			{
				return false;
			}

			if (!context->Enter())
			{
				return false;
			}

			CefV8ValueList list;
			list.push_back(m_Response);

			CefRefPtr<CefV8Value> result = callback->ExecuteFunction(NULL, list);

			if (!result)
			{
				context->Exit();
				return false;
			}

			return context->Exit();
		}

		bool OnGetDownloadData()
		{
			CefRefPtr<CefV8Value> callback = m_DownloadDataCB.second;
			CefRefPtr<CefV8Context> context = m_DownloadDataCB.first;

			if (!context)
			{
				return false;
			}

			if (!callback)
			{
				return false;
			}

			if (!callback->IsFunction())
			{
				return false;
			}

			if (!context->Enter())
			{
				return false;
			}

			CefV8ValueList list;
			list.push_back(m_Response);

			CefRefPtr<CefV8Value> result = callback->ExecuteFunction(NULL, list);

			if (!result)
			{
				context->Exit();
				return false;
			}

			return context->Exit();
		}

	public:
		inline cookie_container_t& GetCookieContainer()
		{
			return m_vCookies;
		}

		inline std::string& GetDownloadData()
		{
			return m_strDownloadData;
		}

		inline CefRefPtr<CefV8Value> GetRequest()
		{
			return m_Request;
		}

		CXSHttpResponse(
			CefRefPtr<CefV8Value> request,
			TCallback& getCookieCB, 
			TCallback& getDownloadDataCB)
		{
			m_Request		= request;
			m_CookieCB		= getCookieCB;
			m_DownloadDataCB= getDownloadDataCB;

			m_RequestClient = new CRequestClient(this);
		}

		CefRefPtr<CefV8Value> CreateObject()
		{
			m_Response = CefV8Value::CreateObject(NULL);

			CefRefPtr<CefV8Handler> pHandler(new CHandler);

			m_Response->SetUserData(this);

			m_Response->SetValue("GetCookies",		CefV8Value::CreateFunction("GetCookies",		pHandler), V8_PROPERTY_ATTRIBUTE_NONE);
			m_Response->SetValue("GetDownloadData", CefV8Value::CreateFunction("GetDownloadData",	pHandler), V8_PROPERTY_ATTRIBUTE_NONE);
			m_Response->SetValue("GetRequest",		CefV8Value::CreateFunction("GetRequest",		pHandler), V8_PROPERTY_ATTRIBUTE_NONE);
			m_Response->SetValue("Release",			CefV8Value::CreateFunction("Release",			pHandler), V8_PROPERTY_ATTRIBUTE_NONE);

			return m_Response;
		}

	private:
		class CRequestClient : public CCefURLRequestClient
		{
			IMPLEMENT_REFCOUNTING(CRequestClient);

		private:
			CXSHttpResponse* m_Response;

		public:
			CRequestClient(CXSHttpResponse* pResponse)
			{
				m_Response = pResponse;
			}

			void SetResponse(CXSHttpResponse* pResponse)
			{
				m_Response = pResponse;
			}

			virtual void OnRequestComplete(CefRefPtr<CefURLRequest> request) OVERRIDE
			{
				if (!m_Response)
				{
					return;
				}

				m_Response->GetCookieContainer() = GetCookies(request->GetResponse());

				if (m_Response)
				{
					m_Response->OnGetResponse();
				}
			}

			virtual void OnDownloadData(CefRefPtr<CefURLRequest> request,
				const void* data,
				size_t data_length) OVERRIDE
			{
				if (!m_Response)
				{
					return;
				}

				m_Response->GetDownloadData() = std::string(reinterpret_cast<const char*>(data),
					reinterpret_cast<const char*>(data) + data_length);

				if (m_Response)
				{
					m_Response->OnGetDownloadData();
				}
			}
		};

		CefRefPtr<CRequestClient> m_RequestClient;

	public:
		inline CefRefPtr<CRequestClient> GetRequestClient()
		{
			return m_RequestClient;
		}

		~CXSHttpResponse()
		{
			if (m_RequestClient)
			{
				m_RequestClient->SetResponse(NULL);
			}
		}

	private:
		class CHandler : public CefV8Handler
		{
			IMPLEMENT_REFCOUNTING(CHandler);

		private:
			virtual bool Execute(const CefString& name,
				CefRefPtr<CefV8Value> object,
				const CefV8ValueList& arguments,
				CefRefPtr<CefV8Value>& retval,
				CefString& exception) OVERRIDE
			{
				CXSHttpResponse* pResponse = reinterpret_cast<CXSHttpResponse*>(object->GetUserData().get());

				if (!pResponse)
				{
					return false;
				}

				if (name == "GetCookies")
				{
					CefRefPtr<CefV8Value> cookies = CefV8Value::CreateArray(pResponse->m_vCookies.size());

					for (size_t i = 0; i < pResponse->GetCookieContainer().size(); ++i)
					{
						CefRefPtr<CefV8Value> cookie = CefV8Value::CreateArray(2);

						cookie->SetValue(0, CefV8Value::CreateString(pResponse->GetCookieContainer()[i].name));
						cookie->SetValue(1, CefV8Value::CreateString(pResponse->GetCookieContainer()[i].value));

						cookies->SetValue(i, cookie);
					}

					retval = cookies;

					return true;
				}
				else if (name == "GetDownloadData")
				{
					retval = CefV8Value::CreateString(pResponse->GetDownloadData());
					return true;
				}
				else if (name == "GetRequest")
				{
					retval = pResponse->GetRequest();
					return true;
				}
				else if (name == "Release")
				{
					object->SetUserData(NULL);
					return true;
				}

				return false;
			}
		};
	};

	class CXSHttpRequest : public CefBase
	{
		IMPLEMENT_REFCOUNTING(CXSHttpRequest);

	private:
		cookie_container_t	m_vCookies;
		header_map_t		m_mHeaders;

		std::string m_strURL;
		std::string m_strMethod;
		std::string m_strPostData;

		CefRefPtr<CXSHttpResponse> m_Response;
		CefRefPtr<CefV8Value> m_Request;

	public:
		CefRefPtr<CefV8Value> CreateObject()
		{
			m_Request = CefV8Value::CreateObject(NULL);

			CefRefPtr<CefV8Handler> pHandler(new CHandler);

			m_Request->SetUserData(this);

			m_Request->SetValue("AddCookies",	CefV8Value::CreateFunction("AddCookies", pHandler), V8_PROPERTY_ATTRIBUTE_NONE);
			m_Request->SetValue("AddHeaders",	CefV8Value::CreateFunction("AddHeaders", pHandler), V8_PROPERTY_ATTRIBUTE_NONE);
			m_Request->SetValue("SetMethod",	CefV8Value::CreateFunction("SetMethod", pHandler), V8_PROPERTY_ATTRIBUTE_NONE);
			m_Request->SetValue("SetURL",		CefV8Value::CreateFunction("SetURL", pHandler), V8_PROPERTY_ATTRIBUTE_NONE);
			m_Request->SetValue("SetPostData",	CefV8Value::CreateFunction("SetPostData", pHandler), V8_PROPERTY_ATTRIBUTE_NONE);
			m_Request->SetValue("SendRequest",	CefV8Value::CreateFunction("SendRequest", pHandler), V8_PROPERTY_ATTRIBUTE_NONE);
			m_Request->SetValue("GetResponse",	CefV8Value::CreateFunction("GetResponse", pHandler), V8_PROPERTY_ATTRIBUTE_NONE);
			m_Request->SetValue("Release",		CefV8Value::CreateFunction("Release", pHandler), V8_PROPERTY_ATTRIBUTE_NONE);

			return m_Request;
		}

		void AddCookie(SCookie& cookie)
		{
			m_vCookies.push_back(cookie);
		}

		void AddHeader(header_map_t::value_type& header)
		{
			m_mHeaders.insert(header);
		}

		void SetMethod(const std::string& method)
		{
			m_strMethod = method;
		}

		void SetURL(const std::string& url)
		{
			m_strURL = url;
		}

		void SetPostData(const std::string& postData)
		{
			m_strPostData = postData;
		}

		CefRefPtr<CXSHttpResponse> SendRequest(TCallback& cbCookie, TCallback& cbData)
		{
			if (m_strURL.empty())
			{
				return NULL;
			}

			if (!m_vCookies.empty())
			{
				m_mHeaders.insert(std::make_pair("Cookie", GetCookies(m_vCookies)));
			}

			CefRefPtr<CefRequest> request(CefRequest::Create());
			{
				request->SetFlags(UR_FLAG_ALLOW_CACHED_CREDENTIALS);
				request->SetURL(m_strURL);

				if (!m_mHeaders.empty())
				{
					request->SetHeaderMap(m_mHeaders);
				}

				request->SetMethod(m_strMethod);
			}

			if (!m_strPostData.empty())
			{
				CefRefPtr<CefPostDataElement> postData = CefPostDataElement::Create();
				{
					postData->SetToBytes(m_strPostData.length(), m_strPostData.data());
				}

				CefRefPtr<CefPostData> post = CefPostData::Create();
				{
					post->AddElement(postData);
				}

				request->SetPostData(post);
			}

			m_Response = new CXSHttpResponse(m_Request, cbCookie, cbData);

			CefRequestContextSettings settings;
			CefRefPtr<CefURLRequest> urlRequest = CefURLRequest::Create(request, m_Response->GetRequestClient(),
				CefRequestContext::CreateContext(settings, new CRequestContextHandler));

			return m_Response;
		}

		class CHandler : public CefV8Handler
		{
			IMPLEMENT_REFCOUNTING(CHandler);

		private:
			virtual bool Execute(const CefString& name,
				CefRefPtr<CefV8Value> object,
				const CefV8ValueList& arguments,
				CefRefPtr<CefV8Value>& retval,
				CefString& exception) OVERRIDE
			{
				CXSHttpRequest* pRequest = reinterpret_cast<CXSHttpRequest*>(object->GetUserData().get());

				if (!pRequest)
				{
					return false;
				}

				if (name == "AddCookies")
				{
					for (auto it = arguments.begin(); it != arguments.end(); ++it)
					{
						if (!it->get()->IsArray())
						{
							continue;
						}

						if (it->get()->GetArrayLength() != 2)
						{
							continue;
						}

						if (!it->get()->GetValue(0)->IsString())
						{
							continue;
						}

						if (!it->get()->GetValue(1)->IsString())
						{
							continue;
						}

						pRequest->AddCookie(SCookie(
							it->get()->GetValue(0)->GetStringValue().ToWString(),
							it->get()->GetValue(1)->GetStringValue().ToWString()));
					}

					return true;
				}
				else if (name == "AddHeaders")
				{
					for (auto it = arguments.begin(); it != arguments.end(); ++it)
					{
						if (!it->get()->IsArray())
						{
							continue;
						}

						if (it->get()->GetArrayLength() != 2)
						{
							continue;
						}

						if (!it->get()->GetValue(0)->IsString())
						{
							continue;
						}

						if (!it->get()->GetValue(1)->IsString())
						{
							continue;
						}

						pRequest->AddHeader(header_map_t::value_type(
							it->get()->GetValue(0)->GetStringValue().ToWString(),
							it->get()->GetValue(1)->GetStringValue().ToWString()));
					}

					return true;
				}
				else if (name == "SetMethod")
				{
					if (arguments.empty())
					{
						return true;
					}

					if (!arguments[0]->IsString())
					{
						return true;
					}

					pRequest->SetMethod(arguments[0]->GetStringValue().ToString());

					return true;
				}
				else if (name == "SetURL")
				{
					if (arguments.empty())
					{
						return true;
					}

					if (!arguments[0]->IsString())
					{
						return true;
					}

					pRequest->SetURL(arguments[0]->GetStringValue().ToString());

					return true;
				}
				else if (name == "SendRequest")
				{
					return true;
				}
				else if (name == "GetResponse")
				{
					CefRefPtr<CXSHttpResponse> response = NULL;

					if (arguments.size() == 0)
					{
						response = pRequest->SendRequest(
							TCallback(CefV8Context::GetCurrentContext(), CefV8Value::CreateUndefined()),
							TCallback(CefV8Context::GetCurrentContext(), CefV8Value::CreateUndefined()));
					}
					else
					{
						CefRefPtr<CefV8Value> cookieCB = arguments[0];

						if (!cookieCB->IsFunction())
						{
							return true;
						}

						CefRefPtr<CefV8Value> downloadDataCB;

						if (arguments.size() >= 2)
						{
							downloadDataCB = arguments[1];

							if (!downloadDataCB->IsFunction())
							{
								return true;
							}
						}
						else
						{
							downloadDataCB = CefV8Value::CreateUndefined();
						}

						TCallback cbCookie = TCallback(CefV8Context::GetCurrentContext(), cookieCB);
						TCallback cbDownloadData = TCallback(CefV8Context::GetCurrentContext(), downloadDataCB);

						response = pRequest->SendRequest(cbCookie, cbDownloadData);
					}

					if (response)
					{
						retval = response->CreateObject();
					}

					return true;
				}
				else if (name == "SetPostData")
				{
					if (arguments.empty())
					{
						return true;
					}

					if (!arguments[0]->IsString())
					{
						return true;
					}

					pRequest->SetPostData(arguments[0]->GetStringValue());
					return true;
				}
				else if (name == "Release")
				{
					object->SetUserData(NULL);
					return true;
				}

				return false;
			}
		};
	};
};

#endif
