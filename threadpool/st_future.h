#pragma once

#include <memory>
#include <future>

namespace st
{

	template <typename T>
	class generic_future
	{
	private:

		std::future<void> m_ft;
		std::shared_ptr<unsigned char[]> m_pBuffer;

	public:

		generic_future() = default;

		generic_future(std::future<void>&& vft, const std::shared_ptr<unsigned char[]>& pBuffer) :
			m_ft(std::move(vft)), m_pBuffer(pBuffer) {}

		generic_future(generic_future<T>&& ft) : m_ft(std::move(ft.m_ft)), m_pBuffer(std::move(ft.m_pBuffer)) {}

		generic_future<T>& operator=(generic_future<T>&& ft)
		{
			if (&ft != this)
			{
				m_ft = std::move(ft.m_ft);
				m_pBuffer = std::move(ft.m_pBuffer);
			}
			return *this;
		}

		void set_exception(std::exception_ptr pExc)
		{
			m_ft._Set_exception(pExc, false);
		}

		T get()
		{
			m_ft.get();
			auto pT = reinterpret_cast<T*>(m_pBuffer.get());
			return *pT;
		}

		void wait() { m_ft.wait(); }



		// TODO: std::future interface

	};



}