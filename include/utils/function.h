#ifndef __FUNCTION_H__
#define __FUNCTION_H__

#include <cstdlib>
#include <cstdio>
#include <tuple>

#include <boost/bind.hpp>

template<int ...>
struct seq {};

template<int N, int ...S>
struct gens : gens<N - 1, N - 1, S...> {};

template<int ...S>
struct gens<0, S...>
{
	typedef seq<S...> type;
};

class CFunction
{
	class CFunctionWrapper
	{
	public:
		virtual ~CFunctionWrapper() {}

	public:
		virtual void Run() = 0;
		virtual void operator()()
		{
			Run();
		}
	};

	template<typename T, typename... Args>
	class CFunctionHolder : public CFunctionWrapper
	{
	public:
		std::tuple<Args...> m_Tuple;
		std::function<T(Args...)> m_Function;

	public:
		CFunctionHolder(const std::function<T(Args...)> & func, Args... args) :
			m_Tuple(std::tuple<Args...>(args...)),
			m_Function(func) {}

		CFunctionHolder(T&& bound, Args... args) :
			m_Tuple(std::tuple<Args...>(args...)),
			m_Function(bound) {}

	public:
		virtual void Run() override
		{
			Dispatch(typename gens<sizeof...(Args)>::type());
		}

	private:
		template<int ...S>
		inline void Dispatch(seq<S...>)
		{
			m_Function(std::get<S>(m_Tuple) ...);
		}
	};

	template<typename T, typename C, typename... Args>
	class CMemberFunctionHolder : public CFunctionWrapper
	{
	public:
		std::tuple<Args...> m_Tuple;
		std::function<T(Args...)> m_Function;

	public:
		inline CMemberFunctionHolder(std::mem_fun1_t<T, C, Args...> member_func, C* instance, Args... args) :
			m_Tuple(std::tuple<Args...>(args...)),
			m_Function(std::bind(member_func, instance, args...)) {}

	public:
		virtual void Run() override
		{
			Dispatch(typename gens<sizeof...(Args)>::type());
		}

	private:
		template<int ...S>
		inline void Dispatch(seq<S...>)
		{
			m_Function(std::get<S>(m_Tuple) ...);
		}
	};

	class CServiceFunction : public CFunctionWrapper
	{
	private:
		boost::detail::thread_data_ptr m_Context;

	public:
		template<typename C>
		void Initialize(BOOST_THREAD_RV_REF(C) f) 
		{
			m_Context = GetContext(boost::thread_detail::decay_copy(boost::forward<C>(f)));
		}

		template<typename C>
		explicit CServiceFunction(BOOST_THREAD_RV_REF(C) f) 
		{
			Initialize(f);
		}

		template<class T, class... Args>
		explicit CServiceFunction(T f, Args&&... args) 
		{
			Initialize(boost::bind(f, std::forward<Args>(args)...));
		}

		virtual ~CServiceFunction() 
		{
		}

		template<class C>
		explicit CServiceFunction(boost::thread::attributes &attr, BOOST_THREAD_RV_REF(C) f) 
		{
			m_Context = boost::thread_detail::decay_copy(boost::forward<C>(GetContext(f)));
		}

		template<typename C>
		static inline boost::detail::thread_data_ptr GetContext(BOOST_THREAD_RV_REF(C) f) 
		{
			return boost::detail::thread_data_ptr(
				boost::detail::heap_new<boost::detail::thread_data<typename boost::remove_reference<C>::type> >(
				boost::forward<C>(f)));
		}

		inline void Run() override
		{
			m_Context->run();
		}
	};

	CFunctionWrapper* m_pFunction;

public:
	~CFunction()
	{
		if (m_pFunction)
		{
			delete m_pFunction;
		}
	}

	CFunction()
	{
		m_pFunction = NULL;
	}

	template<typename T>
	CFunction(BOOST_THREAD_RV_REF(T) f)
	{
		AssignFunction(f);
	}

	template<typename T, typename... Args>
	CFunction(T(*func)(Args...), Args... args)
	{
		AssignFunction(func, args...);
	}

	template<typename T, typename C, typename... Args>
	CFunction(T(C::*member_func)(Args...), C* instance, Args... args)
	{
		AssignFunction(member_func, instance, args...);
	}

	template<typename T, typename C, typename... Args>
	inline void AssignFunction(T(C::*member_func)(Args...), C* instance, Args... args)
	{
		m_pFunction = new CMemberFunctionHolder<T, C, Args...>(std::mem_fun(member_func), instance, args...);
	}

	template<typename T>
	inline void AssignFunction(BOOST_THREAD_RV_REF(T) bound)
	{
		m_pFunction = new CServiceFunction((BOOST_THREAD_RV_REF(T))bound);
	}

	template<typename T, typename... Args>
	inline void AssignFunction(T(*func)(Args...), Args... args)
	{
		m_pFunction = new CFunctionHolder<T, Args...>(func, args...);
	}

	inline void Run()
	{
		if (m_pFunction)
		{
			m_pFunction->Run();
		}
	}

	inline void operator()()
	{
		Run();
	}
};

#endif