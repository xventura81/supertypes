#pragma once

#include <vector>
#include <queue>
#include <functional>

#include "Details.h"


namespace st
{

	class cancellation_token final
	{
	private:
		bool m_bCancelled;
	public:
		cancellation_token() : m_bCancelled(false) {}
		void cancel() { m_bCancelled = true; }
		bool isCancelled() const { return m_bCancelled; }
		void throwIfCancelled() const
		{
			if (m_bCancelled)
				throw _details::cancelled_exception{};
		}
	};


	class threadpool final
	{

	private:

		using threadFunc_t = std::function<void()>;

		struct threadNotify
		{
			std::condition_variable cond;
			std::mutex mutex;
			std::queue<threadFunc_t> quFunc;
			bool wake() const { return !quFunc.empty(); }
			threadNotify() = default;
		};

		std::vector<std::thread> m_vecThreads;
		threadNotify m_wakeup;

		void workFunc()
		{
			auto&& wakeup = m_wakeup;
			auto fnCheck = [&wakeup] {return wakeup.wake(); };
			try {
				while (true) {
					threadFunc_t doWork;
					std::unique_lock<std::mutex> lk(m_wakeup.mutex);
					{
						m_wakeup.cond.wait(lk, fnCheck);
						doWork = std::move(m_wakeup.quFunc.front());
						m_wakeup.quFunc.pop();
					}
					if (doWork)
						doWork();
				}
			} catch( _details::thread_terminate&){}
		}

	public:

		explicit threadpool(size_t nThreads = 20U) :
			m_vecThreads(),
			m_wakeup()
		{
			for (auto k = 0U; k < nThreads; ++k)
				m_vecThreads.emplace_back(std::bind(&threadpool::workFunc, this));
		}

		~threadpool()
		{
			for (auto&& th : m_vecThreads)
				m_wakeup.quFunc.push([] {throw _details::thread_terminate(); });
			m_wakeup.cond.notify_all();
			for (auto&& th : m_vecThreads)
				th.join();
		}

		size_t size_running() const {
			return std::count_if(m_vecThreads.cbegin(), m_vecThreads.cend(),
				[](const std::thread& th) {return th.joinable(); });
		}


		template <typename T>
		std::future<T> run_async(std::function<T(const cancellation_token&)> func, const cancellation_token& token) 
		{
			std::promise<T> pr;
			auto fut = pr.get_future();
			try {
				token.throwIfCancelled();
				//...
				token.throwIfCancelled();
			}
			catch (_details::cancelled_exception&)
			{
				pr.set_exception(std::current_exception());
			}
			return fut;
		}


		template <typename T>
		std::future<T> run_async(std::function<T()> func)
		{
			cancellation_token token;
			auto fnC = [func](const cancellation_token& ct)
			{
				T res;
				if (!ct.isCancelled)
					res = func();
				ct.throwIfCancelled();
				return res;
			};
			return runAsync(fnC, token);
		}


	};

};



