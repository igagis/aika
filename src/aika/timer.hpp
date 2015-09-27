/* The MIT License:

Copyright (c) 2008-2012 Ivan Gagis <igagis@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE. */

// Home page: http://ting.googlecode.com



/**
 * @file Timer.hpp
 * @author Ivan Gagis <igagis@gmail.com>
 * @author Jose Luis Hidalgo <joseluis.hidalgo@gmail.com> - Mac OS X port
 * @brief Timer library.
 */

#pragma once

//#define M_ENABLE_TIMER_TRACE
#ifdef M_ENABLE_TIMER_TRACE
#	define M_TIMER_TRACE(x) TRACE(<< "[Timer]" x)
#else
#	define M_TIMER_TRACE(x)
#endif


#include "config.hpp"


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


#include <vector>
#include <map>
#include <algorithm>

#include "debug.hpp"
#include "types.hpp"
#include "Singleton.hpp"
#include "math.hpp"

#include "mt/Thread.hpp"
#include "mt/Semaphore.hpp"



namespace ting{
namespace timer{


/**
 * @brief Get constantly increasing millisecond ticks.
 * It is not guaranteed that the ticks counting started at the system start.
 * @return constantly increasing millisecond ticks.
 */
inline std::uint32_t GetTicks(){
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
		throw ting::Exc("GetTicks(): clock_gettime() returned error");
	}

	return std::uint32_t(std::uint32_t(ts.tv_sec) * 1000 + std::uint32_t(ts.tv_nsec / 1000000));
#else
#	error "Unsupported OS"
#endif
}



/**
 * @brief General purpose timer.
 * This is a class representing a timer. Its accuracy is not expected to be high,
 * approximately it is tens of milliseconds, i.e. 0.01 second.
 * Before using the timers it is necessary to initialize the timer library, see
 * description of ting::TimerLib class for details.
 */
class Timer{
	friend class Lib;

	//This constant is for testing purposes.
	//Should be set to std::uint32_t(-1) in release.
	inline static std::uint32_t DMaxTicks(){
		return std::uint32_t(-1);
	}
	
	bool isRunning = false;//true if timer has been started and has not stopped yet

private:
	typedef std::multimap<std::uint64_t, Timer*> T_TimerList;
	typedef T_TimerList::iterator T_TimerIter;

	T_TimerIter i;//if timer is running, this is the iterator into the map of timers

public:

	/**
	 * @brief Timer expiration handler.
	 * This method is called when timer expires.
	 * Note, that the method is called from a separate thread, so user should
	 * do all the necessary synchronization when implementing this method.
	 * Also, note that expired methods from different timers are called sequentially,
	 * that means that, for example, if two timers have expired simultaneously then
	 * the expired method of the first timer is called first, and only after it returns
	 * the expired method of the second timer is called.
	 * That means, that one should handle the timer expiration as fast as possible to
	 * avoid inaccuracy of other timers which have expired at the same time, since
	 * the longer your expired handler method is executed, the latter expired method of those other timers will be called.
	 * Do not do any heavy calculations or logics in the expired handler method. Do just
	 * quick initiation of the action which should be taken on timer expiration,
	 * for example, post a message to the message queue of another thread to be handled by that another thread.
	 */
	virtual void OnExpired()noexcept = 0;

	/**
	 * @brief Constructor for new Timer instance.
	 * The newly created timer is initially not running.
	 */
	inline Timer(){
		ASSERT(!this->isRunning)
	}

	virtual ~Timer()noexcept;

	/**
	 * @brief Start timer.
	 * After calling this method one can be sure that the timer state has been
	 * switched to running. This means that if you call Stop() after that and it
	 * returns false then this will mean that the timer has expired rather than not started.
	 * It is allowed to call the Start() method from within the handler of the timer expired signal.
	 * If the timer is already running (i.e. it was already started before and has not expired yet)
	 * the ting::Exc exception will be thrown.
	 * This method is thread-safe.
	 * @param millisec - timer timeout in milliseconds.
	 */
	inline void Start(std::uint32_t millisec);

	/**
	 * @brief Stop the timer.
	 * Stops the timer if it was started before. In case it was not started
	 * or it has already expired this method does nothing.
	 * This method is thread-safe.
	 * After this method has returned you may be sure that the OnExpired() callback
	 * will not be called anymore, unless the timer was not started again from within the callback
	 * if the callback was called before returning from Stop() method.
	 * Such case can be caught by checking the return value of the method.
	 * @return true if timer was running and was stopped.
	 * @return false if timer was not running already when the Stop() method was called. I.e.
	 *         the timer has expired already or was not started.
	 */
	inline bool Stop()noexcept;
};



/**
 * @brief Timer library singleton class.
 * This is a singleton class which represents timer library which allows using
 * timers (see ting::Timer class). Before using timers one needs to initialize
 * the timer library, this is done just by creating the singleton object of
 * the timer library class.
 */
class Lib : public IntrusiveSingleton<Lib>{
	friend class IntrusiveSingleton<Lib>;
	static IntrusiveSingleton<Lib>::T_Instance instance;
	
	friend class ting::timer::Timer;

	class TimerThread : public ting::mt::Thread{
	public:
		volatile bool quitFlag = false;

		std::mutex mutex;
		ting::mt::Semaphore sema;

		//mutex used to make sure that after Timer::Stop() method is called the
		//expired notification callback will not be called
		std::mutex expiredTimersNotifyMutex;
		
		//map requires key uniqueness, but in our case the key is a stop ticks,
		//so, use std::multimap to allow similar keys.
		Timer::T_TimerList timers;



		std::uint64_t ticks = 0;
		bool incTicks = false;//flag indicates that high word of ticks needs increment

		//This function should be called at least once in 16 days (half of std::uint32_t(-1) milliseconds)
		//in order to function properly.
		//This is achieved by having a repeating timer set to 16 days, which will do nothing but
		//calling this function.
		inline std::uint64_t GetTicks();



		TimerThread(){
			ASSERT(!this->quitFlag)
		}

		~TimerThread()noexcept{
			//at the time of TimerLib destroying there should be no active timers
			ASSERT(this->timers.size() == 0)
		}

		void AddTimer_ts(Timer* timer, std::uint32_t timeout);

		bool RemoveTimer_ts(Timer* timer)noexcept;

		inline void SetQuitFlagAndSignalSemaphore()noexcept{
			this->quitFlag = true;
			this->sema.Signal();
		}

		//override
		void Run();

	} thread;

	class HalfMaxTicksTimer : public Timer{
	public:
		//override
		void OnExpired()noexcept{
			try{
				this->Start(Timer::DMaxTicks() / 2);
			}catch(...){
				ASSERT(false)
			}
		}
	} halfMaxTicksTimer;

public:
	inline Lib(){
		this->thread.Start();

		//start timer for half of the max ticks
		this->halfMaxTicksTimer.OnExpired();
	}

	/**
	 * @brief Destructor.
	 * Note, that before destroying the timer library singleton object all the
	 * timers should be stopped. Otherwise, in debug mode it will result in assertion failure.
	 */
	~Lib()noexcept{
		//stop half max ticks timer
		while(!this->halfMaxTicksTimer.Stop()){
			ting::mt::Thread::Sleep(10);
		}
#ifdef DEBUG
		{
			std::lock_guard<decltype(this->thread.mutex)> mutexGuard(this->thread.mutex);
			ASSERT(this->thread.timers.size() == 0)
		}
#endif
		this->thread.SetQuitFlagAndSignalSemaphore();
		this->thread.Join();
	}
};



inline Timer::~Timer()noexcept{
	ASSERT_INFO(!this->isRunning, "trying to destroy running timer. Stop the timer first and make sure its OnExpired() method will not be called, then destroy the timer object.")
}



inline void Timer::Start(std::uint32_t millisec){
	ASSERT_INFO(Lib::IsCreated(), "Timer library is not initialized, you need to create TimerLib singletone object first")

	Lib::Inst().thread.AddTimer_ts(this, millisec);
}



inline bool Timer::Stop()noexcept{
	ASSERT(Lib::IsCreated())
	return Lib::Inst().thread.RemoveTimer_ts(this);
}



inline std::uint64_t Lib::TimerThread::GetTicks(){
	std::uint32_t ticks = ting::timer::GetTicks() % Timer::DMaxTicks();

	if(this->incTicks){
		if(ticks < Timer::DMaxTicks() / 2){
			this->incTicks = false;
			this->ticks += (std::uint64_t(Timer::DMaxTicks()) + 1); //update 64 bit ticks counter
		}
	}else{
		if(ticks > Timer::DMaxTicks() / 2){
			this->incTicks = true;
		}
	}

	return this->ticks + std::uint64_t(ticks);
}



}//~namespace
}//~namespace
