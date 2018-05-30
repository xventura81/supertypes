#include "stdafx.h"
#include "CppUnitTest.h"

#include "st_threadpool.h"

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
			//auto func = [&bWasRun](cancel_t token) {
			//	bWasRun = true;
			//	return true;
			//};
			//token.cancel();

			//auto fut = threadPool.run_async(func, token);
			//
			//Assert::ExpectException<st::_details::cancelled_exception>([&fut] {return fut.get(); });
			Assert::IsFalse(bWasRun);
		}

		TEST_METHOD(RunsSimpleFunction)
		{
			st::threadpool threadPool;
			
			auto fut = threadPool.run_async([](cancel_t) { return 1 + 2 + 3; });
			
			Assert::AreEqual(1 + 2 + 3, fut.get());
		}

	};
}