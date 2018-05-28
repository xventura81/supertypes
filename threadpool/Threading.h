#pragma once

#include <future>

#ifdef ZG_EXPORTING
#define DLL __declspec(dllexport)
#else
#define DLL __declspec(dllimport)
#endif

namespace zeitgleich {

	template<typename T>
	DLL std::future<T> runAsync()


}