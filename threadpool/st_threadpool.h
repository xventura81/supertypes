#pragma once

#include <vector>
#include <queue>
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
		bool cancelled() const { return *m_pbCancelled; }
		void throw_if_cancelled() const
		{
			if (cancelled())
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

		void workFunc()
		{
			auto&& wakeup = m_wakeup;
			auto fnCheck = [&wakeup] {return wakeup.wake(); };
			try {
				while (true) {
					std::function<void()> doWork;
					{
						std::unique_lock<std::mutex> lk(m_wakeup.mutex);
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
			{
				std::unique_lock<std::mutex> lk(m_wakeup.mutex);
				for (auto&& th : m_vecThreads)
					m_wakeup.quFunc.push([] {throw _details::thread_terminate(); });
			}
			m_wakeup.cond.notify_all();
			for (auto&& th : m_vecThreads)
				th.join();
		}

		size_t size_running() const
		{
			return std::count_if(m_vecThreads.cbegin(), m_vecThreads.cend(),
				[](const std::thread& th) {return th.joinable(); });
		}

		template <typename F>
		using res_t = std::result_of_t<F()>;

		template <typename F>
		using resC_t = std::result_of_t<F(cancellation_token)>;

		template <typename F>
		std::shared_future<resC_t<F>> run_async(F&& func, cancellation_token token)
		{
			try {
				token.throw_if_cancelled();
				auto fut = std::async(std::launch::deferred, [fn = std::forward<F>(func), token] {return fn(token); });
				auto share_ft = fut.share();
				{
					std::unique_lock<std::mutex> lk(m_wakeup.mutex);
					m_wakeup.quFunc.emplace([share_ft] { share_ft.wait(); });
				}
				m_wakeup.cond.notify_one();	// What if all are busy?
				return share_ft;
			}
			catch (_details::cancelled_exception&)
			{
				std::promise<resC_t<F>> prm;
				auto fut = prm.get_future();
				prm.set_exception(std::current_exception());
				return fut.share();
			}
		}

		template <typename F>
		std::shared_future<res_t<F>> run_async(F&& func)
		{
			cancellation_token token;
			auto fnC = [fn = std::forward<F>(func)](cancellation_token ) {
				return fn();
			};
			return run_async<decltype(fnC)>(std::move(fnC), token);
		}


	};

};



