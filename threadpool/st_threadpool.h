#pragma once

#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <future>

#include "details.h"


namespace st
{

	class cancellation_token final
	{
	private:
		std::shared_ptr<bool> m_pbCancelled;
	public:
		cancellation_token() : m_pbCancelled(std::make_shared<bool>(false)) {}
		cancellation_token(const cancellation_token&) = default;
		cancellation_token& operator=(const cancellation_token&) = default;
		void cancel() { *m_pbCancelled = true; }
		bool isCancelled() const { return *m_pbCancelled; }
		void throwIfCancelled() const
		{
			if (isCancelled())
				throw _details::cancelled_exception{};
		}
	};


	class threadpool final
	{

	private:

		struct threadNotify
		{
			std::condition_variable cond;
			std::mutex mutex;
			std::queue<std::function<void()>> quFunc;
			bool wake() const { return !quFunc.empty(); }
			threadNotify() = default;
		};

		std::vector<std::thread> m_vecThreads;
		threadNotify m_wakeup;

		template <typename T>
		static std::function<void()> wrap(std::function<T(cancellation_token)> func, std::promise<T>&& prm, cancellation_token token)
		{
			return [prom(std::move(prm)), func, token]() -> void
			{
				try
				{
					auto&& t = func(token);
					prom.set_value(std::move(t));
				}
				catch (std::exception&)
				{
					prom.set_exception(std::current_exception());
				}
			};
		}


		void workFunc()
		{
			auto&& wakeup = m_wakeup;
			auto fnCheck = [&wakeup] {return wakeup.wake(); };
			try {
				while (true) {
					std::function<void()> doWork;
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

		size_t size_running() const
		{
			return std::count_if(m_vecThreads.cbegin(), m_vecThreads.cend(),
				[](const std::thread& th) {return th.joinable(); });
		}


		template <typename T>
		std::future<T> run_async(std::function<T(cancellation_token)> func, cancellation_token token) 
		{
			std::promise<T> prm;
			auto fut = prm.get_future();
			try {
				token.throwIfCancelled();
				auto&& fnRun = wrap<T>(func, std::move(prm), token);
				m_wakeup.quFunc.push(std::move(fnRun));
				m_wakeup.cond.notify_one();	// What if all are busy?
			}
			catch (_details::cancelled_exception&)
			{
				prm.set_exception(std::current_exception());
			}
			return fut;
		}


		template <typename T>
		std::future<T> run_async(std::function<T()> func)
		{
			cancellation_token token;
			auto fnC = [func](cancellation_token ct)
			{
				T res;
				if (!ct.isCancelled())
					res = func();
				ct.throwIfCancelled();
				return res;
			};
			return run_async<T>(fnC, token);
		}


	};

};



