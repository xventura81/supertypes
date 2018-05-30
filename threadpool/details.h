#pragma once

#include <exception>
#include <memory>
#include <functional>
#include <future>

namespace st
{
	namespace _details
	{
		struct cancelled_exception : public std::exception {};
		struct thread_terminate : public std::exception {};
	}

};