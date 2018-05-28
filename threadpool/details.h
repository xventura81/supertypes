#pragma once

#include <exception>
#include <functional>
#include <future>
#include <thread>

namespace st
{
	namespace _details
	{

		struct cancelled_exception : public std::exception {};

		struct thread_terminate : public std::exception {};



	};
};