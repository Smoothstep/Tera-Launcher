#ifndef __POOL_H__
#define __POOL_H__

#pragma once

#include <cstdlib>
#include <cstdio>
#include <vector>
#include <tuple>
#include <utility>

#include <Windows.h>

#include "function.h"

namespace Thread
{
	class CTask
	{
	private:
		CFunction* m_pFunction;
		CTask* m_pNext;

	public:
		CTask(CFunction* pFunction) :
			m_pNext(NULL)
		{
			m_pFunction = pFunction;
		}

		~CTask()
		{
			if (m_pFunction)
			{
				delete m_pFunction;
			}
		}

		inline void SetNext(CTask* pNext)
		{
			m_pNext = pNext;
		}

		inline CTask* Next()
		{
			return m_pNext;
		}

		inline void Run()
		{
			m_pFunction->Run();
		}
	};

	class CTaskQueue
	{
	private:
		CTask* m_pBegin;
		CTask* m_pEnd;

	public:
		CTaskQueue() :
			m_pBegin(NULL),
			m_pEnd(NULL) {}

		~CTaskQueue()
		{
			ClearTasks();
		}

		inline void ClearTasks()
		{
			CTask* pNext = NULL;

			for (CTask* pTask = m_pBegin; pTask = pNext; pTask)
			{
				pNext = pTask->Next();
				delete pTask;
			}

			m_pBegin = NULL;
			m_pEnd = NULL;
		}

		inline void AddTask(CTask* pTask)
		{
			if (!m_pBegin)
			{
				m_pBegin = pTask;
				m_pEnd = m_pBegin;
			}
			else
			{
				m_pEnd->SetNext(pTask);
				m_pEnd = pTask;
			}
		}

		inline CTask* GetTask()
		{
			return m_pBegin;
		}

		inline CTask* GetPopTask()
		{
			if (!m_pBegin)
			{
				return NULL;
			}

			CTask* p = m_pBegin;
			m_pBegin = m_pBegin->Next();
			return p;
		}

		inline void PopTask()
		{
			m_pBegin = m_pBegin->Next();
		}

		inline bool Empty()
		{
			return !m_pBegin;
		}
	};

	class CThreadGroup : public std::vector<boost::thread*> {};

	class CThreadPool
	{
	private:
		CTaskQueue m_TaskQueue;
		CThreadGroup m_vThreads;

		long m_bStopWork;
		long m_bStopAfterWork;
		long m_bNeedDispatch;
		long m_iTasks;

		HANDLE m_hQueue;

		boost::mutex m_WaitMutex;
		boost::condition_variable m_WaitCondition;

	private:
		void ThreadMain()
		{
			CTask* pTask = NULL;

			DWORD bytes_transferred = 0;
			DWORD completion_key = 0;

			while (m_hQueue)
			{
				if (!GetQueuedCompletionStatus(m_hQueue, &bytes_transferred,
					&completion_key, reinterpret_cast<LPOVERLAPPED*>(&pTask), INFINITE))
				{
					continue;
				}

				if (!pTask)
				{
					PostQueuedCompletionStatus(m_hQueue, 0, 0, 0);
					return;
				}

				pTask->Run();
				{
					delete pTask;
				}

				if (InterlockedDecrement(&m_iTasks) == 0)
				{
					if (InterlockedCompareExchange(&m_bStopAfterWork, 0, 0) == 1)
					{
						PostQueuedCompletionStatus(m_hQueue, 0, 0, 0);
						return;
					}
					else
					{
						pTask = m_TaskQueue.GetPopTask();

						if (pTask)
						{
							PostQueuedCompletionStatus(m_hQueue, 0, 0,
								reinterpret_cast<LPOVERLAPPED>(pTask));
						}
						else
						{
							m_WaitMutex.lock();
							m_WaitCondition.notify_all();
							m_WaitMutex.unlock();
						}

						continue;
					}
				}
			}
		}

	public:
		CThreadPool(int iCount) :
			m_bStopWork(false),
			m_bStopAfterWork(false),
			m_bNeedDispatch(true),
			m_hQueue(NULL),
			m_iTasks(0)
		{
			SetupThreads(iCount);
		}

		CThreadPool() :
			m_bStopWork(false),
			m_bStopAfterWork(false),
			m_bNeedDispatch(true),
			m_hQueue(NULL),
			m_iTasks(0) {}

		~CThreadPool()
		{
			ShutdownThreads();

			if (m_hQueue)
			{
				CloseHandle(m_hQueue);
			}
		}

		bool Empty()
		{
			return m_TaskQueue.Empty();
		}

		bool Initialize()
		{
			m_hQueue = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 10);

			if (!m_hQueue)
			{
				return false;
			}

			return true;
		}

		void ShutdownThreads()
		{
			std::for_each(m_vThreads.begin(), m_vThreads.end(), [](boost::thread* pThread)
			{
				pThread->interrupt();
				delete pThread;
			});

			m_vThreads.clear();
		}

		bool SetupThreads(int iCount)
		{
			if (!m_hQueue)
			{
				if (!Initialize())
				{
					return false;
				}
			}

			ShutdownThreads();

			if (iCount < 1)
			{
				return false;
			}

			for (int i = 0; i < iCount; ++i)
			{
				m_vThreads.push_back(new boost::thread(
					boost::bind(&CThreadPool::ThreadMain, this)));
			}

			return true;
		}

		size_t ThreadCount()
		{
			return m_vThreads.size();
		}

		void PostTask(CTask* pTask)
		{
			if (!PostQueuedCompletionStatus(m_hQueue, 0, 0, (OVERLAPPED*)pTask))
			{
				m_TaskQueue.AddTask(pTask);
			}
			else
			{
				InterlockedIncrement(&m_iTasks);
			}
		}

		void JoinAllWork()
		{
			InterlockedExchange(&m_bStopAfterWork, true);

			if (InterlockedCompareExchange(&m_iTasks, 0, 0) == 0)
			{
				PostQueuedCompletionStatus(m_hQueue, 0, 0, 0);
			}

			std::for_each(m_vThreads.begin(), m_vThreads.end(),
				boost::bind(std::mem_fun(&boost::thread::join), _1));
		}

		bool HasWork()
		{
			return m_iTasks != 0 || !Empty();
		}

		void JoinAll(DWORD dwMs = INFINITE)
		{
			boost::unique_lock<boost::mutex> lock(m_WaitMutex);
			{
				m_WaitCondition.wait(lock, [this]
				{
					return !HasWork();
				});
			}
		}

		bool JoinAndInterruptAll()
		{
			if (!Cancel())
			{
				CloseHandle(m_hQueue);
			}

			m_hQueue = NULL;

			if (!HasWork())
			{
				return Initialize();
			}

			std::for_each(m_vThreads.begin(), m_vThreads.end(),
				boost::bind(std::mem_fun(&boost::thread::join), _1));

			m_iTasks = 0;

			return Initialize();
		}

	private:
		bool Cancel()
		{
			if (!CancelIo(m_hQueue))
			{
				return false;
			}

			CloseHandle(m_hQueue);

			return true;
		}
	};
}

#endif