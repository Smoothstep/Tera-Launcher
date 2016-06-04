/*
*	To Communicate between patcher and requester.
*/

#ifndef _PATCHER_CALLBACK_H_
#define _PATCHER_CALLBACK_H_

#pragma once

#include <boost/atomic.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/chrono/time_point.hpp>

#include <boost/ptr_container/ptr_deque.hpp>

#include <sstream>
#include <istream>
#include <deque>

namespace Callback
{
	typedef boost::chrono::time_point<boost::chrono::steady_clock> timepoint_t;

	enum ETypes
	{
		kPatchFinish, kDownloadBegin, kDownload, kPatchBegin, kPatch, kMultiPatch, kMessage, kFatal
	};

	class CAlert
	{
	public:
		CAlert();

		virtual std::string Message() = 0;
		virtual int Type() = 0;

		timepoint_t Timestamp() const;

	protected:
		std::string m_Message;

	private:
		timepoint_t m_CreationTime;
	};

	struct SDownloadAlert : CAlert
	{
		SDownloadAlert();
		SDownloadAlert(uint64_t downloaded, uint64_t needed);

		virtual std::string Message() override;
		virtual int Type() override;

		uint64_t downloaded;
		uint64_t needed;
	};

	struct SDownloadBeginAlert : CAlert
	{
		SDownloadBeginAlert();
		SDownloadBeginAlert(uint64_t total);

		virtual std::string Message() override;
		virtual int Type() override;

		uint64_t download_size_total;
	};

	struct SFilePatchAlert : CAlert
	{
		SFilePatchAlert();
		SFilePatchAlert(std::string file, std::string status, uint64_t size);
		SFilePatchAlert(std::string&& file, std::string&& status, uint64_t size);

		virtual std::string Message() override;
		virtual int Type() override;

		std::string file;
		std::string status;
		uint64_t size;
	};

	struct SMultiFilePatchAlert : CAlert
	{
		SMultiFilePatchAlert();
		SMultiFilePatchAlert(size_t count);

		virtual std::string Message() override;
		virtual int Type() override;

		std::vector<std::string> files;
		std::vector<std::string> status;
		size_t file_count;
	};

	struct SFilePatchBeginAlert : CAlert
	{
		SFilePatchBeginAlert();
		SFilePatchBeginAlert(size_t file_count);

		virtual std::string Message() override;
		virtual int Type() override;

		size_t file_count;
	};

	struct SMessageAlert : CAlert
	{
		SMessageAlert();

		template<typename T>
		inline SMessageAlert(T&& error)
		{
			error_stream << error;
		}

		template<typename T>
		inline SMessageAlert& operator<<(T&& msg)
		{
			error_stream << msg;
			return *this;
		}

		virtual std::string Message() override;
		virtual int Type() override;

		std::stringstream error_stream;
	};

	struct SPatchFatalAlert : SMessageAlert
	{
		SPatchFatalAlert();

		virtual int Type() override;
	};

	struct SPatchFinishAlert : public CAlert
	{
		SPatchFinishAlert();

		virtual std::string Message() override;
		virtual int Type() override;
	};

	static SPatchFatalAlert FATAL;

	typedef struct SCloneHandler
	{
		template<class U>
		static inline U* allocate_clone(const U& r)
		{
			return const_cast<U*>(&r);
		}

		template<class U>
		static inline void deallocate_clone(const U* p)
		{
			delete p;
		}
	} TCloneHandler;

	class CAlertDeque : public boost::ptr_deque<CAlert, TCloneHandler>
	{
	public: 
		~CAlertDeque()
		{
			this->clear();
		}
	};

	class CAlertCopyDeque : public std::deque<CAlert*> {};

	typedef CAlertDeque TAlerts;
	typedef CAlertCopyDeque TAlertsCopy;

	class CPatchCallback
	{
	private:
		TAlerts m_vAlerts;
		TAlertsCopy m_vStaticAlerts;

	private:
		boost::mutex m_Mutex;

	public:
		CPatchCallback();
		~CPatchCallback();
		
		void ClearAll();
		void ClearAlerts();
		void ClearStaticAlerts();

		bool EnqueStaticAlert(CAlert* alert);
		bool PopStaticAlert(int type);

		CAlert* GetStaticAlert(int type);
		CAlert* GetStaticAlertOwnership(int type);

		void EnqueAlert(CAlert* alert);

		void GetAlerts(TAlerts& alerts);
		void GetAlertsCopy(TAlertsCopy& alerts);

		template<class T>
		inline SMessageAlert& operator<<(T&& error)
		{
			return *reinterpret_cast<SMessageAlert*>(new SMessageAlert(error));
		}

		inline SMessageAlert& operator<<(CAlert* alert)
		{
			return *reinterpret_cast<SMessageAlert*>(alert);
		}

		inline SMessageAlert& ThrowFatalAlert(SMessageAlert& alert)
		{
			return *reinterpret_cast<SMessageAlert*>(&(*reinterpret_cast<void**>(&alert) = *reinterpret_cast<void**>(&FATAL)));
		}

	private:
		CAlert* StaticAlert(int32_t type, bool bRemove = false);
	};
}

#define alert(callback, alert) (callback).EnqueAlert(&(callback << alert))
#define fatal(callback, alert) (callback).EnqueAlert(&(callback.ThrowFatalAlert(callback << alert)))

typedef Callback::SDownloadBeginAlert	TADownloadBegin;
typedef Callback::SDownloadAlert		TADownload;
typedef Callback::SMessageAlert			TAMessage;
typedef Callback::SPatchFatalAlert		TAPatchFatal;
typedef Callback::SFilePatchBeginAlert	TAFilePatchBegin;
typedef Callback::SFilePatchAlert		TAFilePatch;
typedef Callback::SMultiFilePatchAlert	TAMultiFilePatch;
typedef Callback::SPatchFinishAlert		TAPatchFinish;

#endif