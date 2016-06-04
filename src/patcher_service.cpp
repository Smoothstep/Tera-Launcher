#include "patcher_service.h"

void CPatchService::RunSingle()
{
	boost::system::error_code error;

	while (!m_Service.stopped())
	{
		try
		{
			m_Service.run_one(error);

			if (error)
			{
				break;
			}
		}
		catch (...)
		{
			break;
		}

		if (m_WorkCount-- == 1)
		{
			m_WorkCondition.notify_all();
		}
	}

	m_WorkCondition.notify_all();
}

CPatchService::~CPatchService()
{
	m_WorkerGroup.interrupt_all();
}

CPatchService::CPatchService() :
	m_Strand(m_Service),
	m_WorkCount(0)
{
}

CPatchService::CPatchService(size_t iCount) :
	m_Strand(m_Service),
	m_WorkCount(0)
{
	SetupPatchThreads(iCount);
}

void CPatchService::ShutdownAll()
{
	m_Service.stop();

	for (std::vector<boost::thread*>::iterator it = m_Threads.begin(); it != m_Threads.end(); ++it)
	{
		m_WorkerGroup.remove_thread(*it);
		delete *it;
	}

	m_Threads.clear();
}

void CPatchService::InterruptService()
{
	m_Service.stop();
}

void CPatchService::JoinAll()
{
	boost::mutex::scoped_lock lock(m_WorkMutex);

	m_WorkCondition.wait(lock, [this]
	{
		return m_WorkCount < 1 || m_Service.stopped();
	});
}

void CPatchService::InterruptAll()
{
	m_WorkerGroup.interrupt_all();
}

size_t CPatchService::ThreadCount()
{
	return m_Threads.size();
}

void CPatchService::SetupPatchThreads(size_t iCount)
{
	if (!m_Threads.empty())
	{
		ShutdownAll();
	}

	m_Work.reset(new boost::asio::io_service::work(m_Service));

	for (size_t i = 0; i < iCount; ++i)
	{
		boost::thread *pThread;

		try
		{
			pThread = new boost::thread(boost::bind(&CPatchService::RunSingle, this));
		}
		catch (...)
		{
			return;
		}

		m_Threads.push_back(pThread);
	}
}
