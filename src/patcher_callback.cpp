
#include "patcher_callback.h"

#define LOCK(x) boost::mutex::scoped_lock __LOCK__(x)

namespace Callback
{
	CAlert::CAlert()
	{
		m_CreationTime = boost::chrono::steady_clock::now();
	}

	timepoint_t CAlert::Timestamp() const
	{
		return m_CreationTime;
	}

	SDownloadAlert::SDownloadAlert()
	{
		needed		= 0;
		downloaded	= 0;
	}

	SDownloadAlert::SDownloadAlert(uint64_t Downloaded, uint64_t Needed) :
		downloaded(Downloaded), needed(Needed)
	{
	}

	std::string SDownloadAlert::Message()
	{
		return "Info: Download update: " + std::to_string(downloaded);
	}

	int SDownloadAlert::Type()
	{
		return kDownload;
	}

	SDownloadBeginAlert::SDownloadBeginAlert()
	{
		download_size_total = 0;
	}

	SDownloadBeginAlert::SDownloadBeginAlert(uint64_t total) :
		download_size_total(total)
	{
	}

	std::string SDownloadBeginAlert::Message()
	{
		return "Info: Download begin: " + std::to_string(download_size_total);
	}

	int SDownloadBeginAlert::Type()
	{
		return kDownloadBegin;
	}


	SMessageAlert::SMessageAlert()
	{
		error_stream.clear();
	}

	std::string SMessageAlert::Message()
	{
		return error_stream.str();
	}

	int SMessageAlert::Type()
	{
		return kMessage;
	}

	CPatchCallback::CPatchCallback()
	{
	}

	CPatchCallback::~CPatchCallback()
	{
		ClearStaticAlerts();
	}

	void CPatchCallback::ClearAll()
	{
		ClearAlerts();
		ClearStaticAlerts();
	}

	void CPatchCallback::ClearAlerts()
	{
		m_vAlerts.clear();
	}

	void CPatchCallback::ClearStaticAlerts()
	{
		for (TAlertsCopy::iterator it = m_vStaticAlerts.begin(); it != m_vStaticAlerts.end(); ++it)
		{
			delete *it;
		}

		m_vStaticAlerts.clear();
	}

	bool CPatchCallback::EnqueStaticAlert(CAlert * alert)
	{
		LOCK(m_Mutex);

		if (!alert)
		{
			return false;
		}

		for (TAlertsCopy::iterator it = m_vStaticAlerts.begin(); it != m_vStaticAlerts.end(); ++it)
		{
			if (*it == alert)
			{
				delete &*it;
				m_vStaticAlerts.erase(it);
				break;
			}
		}

		m_vStaticAlerts.push_back(alert);

		return true;
	}

	bool CPatchCallback::PopStaticAlert(int type)
	{
		LOCK(m_Mutex);

		CAlert* alert = StaticAlert(type);

		if (!alert)
		{
			return false;
		}

		for (TAlertsCopy::iterator it = m_vStaticAlerts.begin(); it != m_vStaticAlerts.end(); ++it)
		{
			if (*it == alert)
			{
				delete *it;
				m_vStaticAlerts.erase(it);
				return true;
			}
		}

		return false;
	}

	CAlert * CPatchCallback::GetStaticAlert(int type)
	{
		LOCK(m_Mutex);
		{
			return StaticAlert(type);
		}
	}

	CAlert * CPatchCallback::GetStaticAlertOwnership(int type)
	{
		LOCK(m_Mutex);
		{
			return StaticAlert(type, true);
		}
	}

	void CPatchCallback::EnqueAlert(CAlert * alert)
	{
		LOCK(m_Mutex);
		{
			m_vAlerts.push_back(alert);
		}
	}

	void CPatchCallback::GetAlerts(TAlerts & alerts)
	{
		LOCK(m_Mutex);
		{
			alerts.transfer(alerts.end(), m_vAlerts.begin(), m_vAlerts.end(), m_vAlerts);
		}

		m_vAlerts.clear();
	}

	void CPatchCallback::GetAlertsCopy(TAlertsCopy & alerts)
	{
		LOCK(m_Mutex);
		{
			for (TAlerts::iterator it = m_vAlerts.begin(); it != m_vAlerts.end(); ++it)
			{
				alerts.insert(alerts.end(), &*it);
			}
		}
	}

	CAlert * CPatchCallback::StaticAlert(int32_t type, bool bRemove)
	{
		for (TAlertsCopy::iterator it = m_vStaticAlerts.begin(); it != m_vStaticAlerts.end(); ++it)
		{
			CAlert *alert = *it;

			if (alert->Type() == type)
			{
				if (bRemove)
				{
					m_vStaticAlerts.erase(it);
				}

				return alert;
			}
		}

		return NULL;
	}

	SPatchFatalAlert::SPatchFatalAlert()
	{
		error_stream << "Fatal: ";
	}

	int SPatchFatalAlert::Type()
	{
		return kFatal;
	}

	SFilePatchAlert::SFilePatchAlert()
	{
		file = "";
		status = "";
		size = 0;
	}

	SFilePatchAlert::SFilePatchAlert(std::string File, std::string Status, uint64_t Size)
	{
		file = File;
		status = Status;
		size = Size;
	}

	SFilePatchAlert::SFilePatchAlert(std::string && File, std::string && Status, uint64_t Size)
	{
		file = File;
		status = Status;
		size = Size;
	}

	std::string SFilePatchAlert::Message()
	{
		return std::string("File patch: ") + file + " Status: " + status;
	}

	int SFilePatchAlert::Type()
	{
		return kPatch;
	}

	SFilePatchBeginAlert::SFilePatchBeginAlert()
	{
		file_count = 0;
	}

	SFilePatchBeginAlert::SFilePatchBeginAlert(size_t File_count)
	{
		file_count = File_count;
	}

	std::string SFilePatchBeginAlert::Message()
	{
		return std::string("File patch begin: ") + std::to_string(file_count) + " files";
	}

	int SFilePatchBeginAlert::Type()
	{
		return kPatchBegin;
	}

	SMultiFilePatchAlert::SMultiFilePatchAlert()
	{
		file_count = 0;
	}

	SMultiFilePatchAlert::SMultiFilePatchAlert(size_t count)
	{
		file_count = count;
	}

	std::string SMultiFilePatchAlert::Message()
	{
		return std::string("Patched " + std::to_string(file_count) + " files");
	}

	int SMultiFilePatchAlert::Type()
	{
		return kMultiPatch;
	}

	SPatchFinishAlert::SPatchFinishAlert()
	{
	}

	std::string SPatchFinishAlert::Message()
	{
		return std::string("Finished patching");
	}

	int SPatchFinishAlert::Type()
	{
		return kPatchFinish;
	}
}