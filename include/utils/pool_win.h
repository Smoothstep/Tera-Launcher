#ifndef __POOL_WIN_H__
#define __POOL_WIN_H__

#pragma once

#include <WinBase.h>
#include <vector>

#include "function.h"

namespace Thread
{
	class CPool
	{
	private:
		PTP_POOL m_Pool;
		PTP_WAIT m_Wait;

		HANDLE m_hWait;

	public:
		CPool() :
			m_Pool(NULL),
			m_Wait(NULL) {}

		~CPool()
		{
			if (m_Wait)
			{
				CloseThreadpoolWait(m_Wait);
			}

			if (m_Pool)
			{
				CloseThreadpool(m_Pool);
			}

			if (m_hWait)
			{
				CloseHandle(m_hWait);
			}
		}

		inline PTP_POOL operator()()
		{
			return m_Pool;
		}

		bool Create()
		{
			if (!(m_Pool = CreateThreadpool(NULL)))
			{
				return false;
			}

			return true;
		}

		bool Wait(PTP_CALLBACK_ENVIRON pEnv)
		{
			m_hWait = CreateEvent(NULL, false, false, NULL);

			if (!(m_Wait = CreateThreadpoolWait(WaitCallback, this, pEnv)))
			{
				return false;
			}

			SetEvent(m_hWait);

			SetThreadpoolWait(m_Wait, m_hWait, 0);
			{
				WaitForThreadpoolWaitCallbacks(m_Wait, false);
			}

			WaitForSingleObject(m_hWait, INFINITE);

			if (!CloseHandle(m_hWait))
			{
				return false;
			}

			return true;
		}

		static void NTAPI WaitCallback(
			PTP_CALLBACK_INSTANCE Instance,
			PVOID                 Context,
			PTP_WAIT              Wait,
			TP_WAIT_RESULT        WaitResult)
		{
			CPool* pPool = reinterpret_cast<CPool*>(Context);

			if (pPool)
			{
				SetEventWhenCallbackReturns(Instance, pPool->m_hWait);
			}
		}
	};

	class CCleanupGrp
	{
	private:
		PTP_CLEANUP_GROUP m_CleanupGroup;

	public:
		CCleanupGrp() : m_CleanupGroup(NULL) {}
		~CCleanupGrp()
		{
			if (m_CleanupGroup)
			{
				CloseThreadpoolCleanupGroup(m_CleanupGroup);
			}
		}

		inline bool Create()
		{
			if (!(m_CleanupGroup = CreateThreadpoolCleanupGroup()))
			{
				return false;
			}

			return true;
		}

		inline PTP_CLEANUP_GROUP operator()()
		{
			return m_CleanupGroup;
		}

		void CloseMembers()
		{
			CloseThreadpoolCleanupGroupMembers(m_CleanupGroup, false, NULL);
		}
	};

	class CCallbackEnvironment
	{
	private:
		TP_CALLBACK_ENVIRON m_CallbackEnvironment;

	public:
		CCallbackEnvironment()
		{
			InitializeThreadpoolEnvironment(&m_CallbackEnvironment);
		}

		inline PTP_CALLBACK_ENVIRON operator()()
		{
			return &m_CallbackEnvironment;
		}
	};

	class CWork
	{
	private:
		PTP_WORK m_Work;

	private:
		CFunction* m_pFunction;

	public:
		CWork() :
			m_Work(NULL),
			m_pFunction(NULL) {}

		~CWork()
		{
			if (m_pFunction)
			{
				delete m_pFunction;
			}
		}

		bool Create(CCallbackEnvironment& cb, CFunction* pFunction)
		{
			m_pFunction = pFunction;

			if (!m_pFunction)
			{
				return false;
			}

			if (!(m_Work = CreateThreadpoolWork(&CWork::WorkCallback, this, cb())))
			{
				return false;
			}

			return true;
		}

		PTP_WORK operator()()
		{
			return m_Work;
		}

		static void NTAPI WorkCallback(
			PTP_CALLBACK_INSTANCE Instance,
			PVOID                 Parameter,
			PTP_WORK              Work)
		{
			CWork* pWork = reinterpret_cast<CWork*>(Parameter);

			if (pWork->m_pFunction)
			{
				pWork->m_pFunction->Run();
			}

			delete pWork;
		}
	};

	class CWinThreadPool
	{
	private:
		CPool m_Pool;
		CCleanupGrp m_CleanupGrp;
		CCallbackEnvironment m_CallbackEnvironment;

	protected:
		std::vector<CWork*> m_Work;

	public:
		CWinThreadPool() {}
		~CWinThreadPool()
		{
			DestroyThreadpoolEnvironment(m_CallbackEnvironment());
		}

		bool Create(DWORD dwMin, DWORD dwMax)
		{
			if (!m_Pool.Create())
			{
				return false;
			}

			if (!SetThreadpoolThreadMinimum(m_Pool(), dwMin))
			{
				return false;
			}

			SetThreadpoolThreadMaximum(m_Pool(), dwMax);

			if (!m_CleanupGrp.Create())
			{
				return false;
			}

			SetThreadpoolCallbackPool(m_CallbackEnvironment(), m_Pool());
			SetThreadpoolCallbackCleanupGroup(m_CallbackEnvironment(), m_CleanupGrp(), NULL);
			SetThreadpoolCallbackRunsLong(m_CallbackEnvironment());

			return true;
		}

		bool PostTask(CFunction* pFunction)
		{
			if (!pFunction)
			{
				return false;
			}

			std::auto_ptr<CWork> pWork(new CWork());

			if (!pWork->Create(m_CallbackEnvironment, pFunction))
			{
				return false;
			}

			SubmitThreadpoolWork((*pWork.release())());

			return true;
		}

		void WaitAll()
		{
			m_CleanupGrp.CloseMembers();
		}
	};
}

#endif