#include "stdafx.h"
#include "CppUnitTest.h"

#include <chrono>
#include "st_threadpool.h"

using namespace std::chrono_literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace zgTesting
{		
	TEST_CLASS(ThreadPool)
	{
	private:
		

	public:

		using cancel_t = st::cancellation_token;
		
		TEST_METHOD(StartsAndStops)
		{
			auto pThreadPool = std::make_unique<st::threadpool>(5U);
			Assert::AreEqual(5U, pThreadPool->size_running());
			pThreadPool.reset();
			Assert::IsNull(pThreadPool.get());
		}

		TEST_METHOD(DoesNotRunIfCancelled)
		{
			st::threadpool threadPool;
			cancel_t token;
			bool bWasRun = false;
			auto func = [&bWasRun](cancel_t token) {
				bWasRun = true;
				return true;
			};
			token.cancel();

			auto fut = threadPool.run_async(func, token);
			
			Assert::ExpectException<st::_details::cancelled_exception>([&fut] {return fut.get(); });
			Assert::IsFalse(bWasRun);
		}

		TEST_METHOD(RunsSimpleFunction)
		{
			st::threadpool threadPool;
			
			auto fut = threadPool.run_async([]() { return 1 + 2 + 3; });
			
			Assert::AreEqual(1 + 2 + 3, fut.get());
		}

		TEST_METHOD(RunsInParallel)
		{
			static auto nThreads = 5U;
			st::threadpool threadPool(nThreads);
			std::promise<void> evGo;
			std::promise<void> evStop;
			auto ftGo = evGo.get_future().share();
			auto ftStop = evStop.get_future().share();
			std::vector<int> vecbRunning(nThreads, 0);
			for (auto&& bRun : vecbRunning)
			{
				threadPool.run_async([ftGo, &bRun, ftStop] {
					ftGo.wait();
					bRun = 1;
					ftStop.wait();
					bRun = 0;
				});
			}
			Assert::IsFalse(std::any_of(vecbRunning.cbegin(), vecbRunning.cend(),
				[](int bRunning) {return bRunning == 1; }));
			evGo.set_value();
			std::this_thread::sleep_for(200ms);
			Assert::IsTrue(std::all_of(vecbRunning.cbegin(), vecbRunning.cend(),
				[](int bRunning) {return bRunning == 1; }));
			evStop.set_value();
			std::this_thread::sleep_for(200ms);
			Assert::IsFalse(std::any_of(vecbRunning.cbegin(), vecbRunning.cend(),
				[](int bRunning) {return bRunning == 1; }));
		}

		TEST_METHOD(CanBeCancelled)
		{
			st::threadpool threadPool;
			cancel_t ct;
			auto fut = threadPool.run_async([](cancel_t token) {
				auto nSum = 0;
				for (auto k = 0; k < 1'000'000; ++k)
				{
					nSum += k;
					std::this_thread::sleep_for(10ms);
					if (k % 100 == 0)
						token.throw_if_cancelled();
				}
				return nSum;
			}, ct);
			ct.cancel();
			Assert::ExpectException<st::_details::cancelled_exception>([&fut] {return fut.get(); });
		}

		TEST_METHOD(AcceptsMoreTasksThanThreads)
		{
			st::threadpool threadPool(1U);
			std::promise<void> evGo;
			auto ftGo = evGo.get_future().share();
			std::vector<std::pair<std::promise<void>, int>> vecRunState;
			std::vector<std::shared_future<bool>> vecftRes;
			for (auto k = 0U; k < 10U; ++k)
			{
				vecRunState.push_back(std::make_pair(std::promise<void>{}, 0));
				auto itState = vecRunState.rbegin();
				auto ft = itState->first.get_future();
				auto&& bRun = itState->second;
				vecftRes.push_back(
					threadPool.run_async([ftGo = std::move(ft), &bRun]{
						ftGo.wait();
						bRun = true;
						return bRun == 1;
					})
				);
			}

			auto itStart = vecRunState.begin();
			itStart->first.set_value();
			Assert::IsTrue(vecftRes.front().get());	
			++itStart;
			Assert::IsFalse(std::any_of(itStart, vecRunState.end(),
				[](const auto& state) {return state.second == 1; }));
			
			for (; itStart != vecRunState.end(); ++itStart)
				itStart->first.set_value();
			std::this_thread::sleep_for(200ms);
			Assert::IsTrue(std::all_of(vecftRes.begin(), vecftRes.end(), [](auto& ftRes) {return ftRes.get(); }));
		}

		TEST_METHOD(RunsToEndBeforeCleanup)
		{
			bool bHasRun = false;
			{
				st::threadpool threadPool;
				threadPool.run_async([&bHasRun] {
					std::this_thread::sleep_for(500ms);
					bHasRun = true;
				});
			}
			Assert::IsTrue(bHasRun);
		}

	};
}