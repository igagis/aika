#include "tick.hpp"

#include <utki/config.hpp>
#include <utki/Exc.hpp>

#if M_COMPILER == M_COMPILER_MSVC
#	pragma warning(disable:4290) //WARNING: C++ exception specification ignored except to indicate a function is not __declspec(nothrow)
#endif


#if M_OS == M_OS_WINDOWS
#	include "windows.hpp"

#elif M_OS == M_OS_MACOSX
#	include<sys/time.h>

#elif M_OS == M_OS_LINUX
#include <ctime>

#else
#	error "Unknown OS"
#endif




std::uint32_t aika::getTicks(){
#if M_OS == M_OS_WINDOWS
	static LARGE_INTEGER perfCounterFreq = {{0, 0}};
	if(perfCounterFreq.QuadPart == 0){
		if(QueryPerformanceFrequency(&perfCounterFreq) == FALSE){
			//looks like the system does not support high resolution tick counter
			return GetTickCount();
		}
	}
	LARGE_INTEGER ticks;
	if(QueryPerformanceCounter(&ticks) == FALSE){
		return GetTickCount();
	}

	return std::uint32_t((ticks.QuadPart * 1000) / perfCounterFreq.QuadPart);
#elif M_OS == M_OS_MACOSX
	//Mac os X doesn't support clock_gettime
	timeval t;
	gettimeofday(&t, 0);
	return std::uint32_t(t.tv_sec * 1000 + (t.tv_usec / 1000));
#elif M_OS == M_OS_LINUX
	timespec ts;
	if(clock_gettime(CLOCK_MONOTONIC, &ts) == -1){
		throw utki::Exc("GetTicks(): clock_gettime() returned error");
	}

	return std::uint32_t(std::uint32_t(ts.tv_sec) * 1000 + std::uint32_t(ts.tv_nsec / 1000000));
#else
#	error "Unsupported OS"
#endif
}

