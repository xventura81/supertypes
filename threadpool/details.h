#pragma once

#include <exception>

#include "st_future.h"

namespace st
{
	namespace _details
	{

		struct cancelled_exception : public std::exception {};

		struct thread_terminate : public std::exception {};

		class generic_promise
		{
		private:

			std::shared_ptr<unsigned char[]> m_pBuffer;
			std::promise<void> m_promise;

			explicit generic_promise(size_t nSize) : m_pBuffer(new unsigned char[nSize]()), m_promise()
			{}

		public:

			template <typename T>
			static generic_promise create() { return generic_promise(sizeof(T)); }

			template <typename T>
			void set_value(T&& value)
			{
				auto pVal = new(m_pBuffer.get()) T(std::forward<T>(value));
				m_promise.set_value();
			}

			void set_exception(std::exception_ptr pExc)
			{
				m_promise.set_exception(pExc);
			}

			template <typename T>
			generic_future<T> get_future() { return generic_future<T>(m_promise.get_future(), m_pBuffer); }

		};

	};
};