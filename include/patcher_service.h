#ifndef __PATCHER_SERVICE__H_
#define __PATCHER_SERVICE__H_

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/atomic.hpp>

class CPatchService
{
private:
	boost::asio::io_service m_Service;
	boost::asio::strand m_Strand;

	boost::shared_ptr<boost::asio::io_service::work> m_Work;

	boost::thread_group m_WorkerGroup;

	boost::mutex m_WorkMutex;
	boost::condition_variable m_WorkCondition;

	boost::atomic_int m_WorkCount;

private:
	void RunSingle();

protected:
	std::vector<boost::thread*> m_Threads;

public:
	~CPatchService();

	CPatchService();
	CPatchService(size_t iCount);

	void ShutdownAll();
	void JoinAll();
	void InterruptAll();
	void InterruptService();

	size_t ThreadCount();

	void SetupPatchThreads(size_t iCount);

	template<typename T, typename... Args>
	void Work(T& func, Args&&... args)
	{
		m_WorkCount++;
		m_Service.post(boost::bind(func, args...));
	}

	template<typename C, typename T, typename... Args>
	void Work(T(C::*func)(Args...), C* p, Args&&... args)
	{
		m_WorkCount++;
		m_Service.post(boost::bind(func, p, args...));
	}

	template<typename C, typename T, typename... Args>
	void Work(T(C::*func)(Args...), C* p, Args&... args)
	{
		m_WorkCount++;
		m_Service.post(boost::bind(func, p, args...));
	}

	template<typename C>
	void Work(BOOST_THREAD_RV_REF(C) f)
	{
		m_WorkCount++;
		m_Service.post(f);
	}
};

#endif