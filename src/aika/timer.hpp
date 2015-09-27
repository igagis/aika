/**
 * @file Timer.hpp
 * @author Ivan Gagis <igagis@gmail.com>
 * @author Jose Luis Hidalgo <joseluis.hidalgo@gmail.com> - Mac OS X port
 * @brief Timer library.
 */

#pragma once


#include <vector>
#include <map>
#include <algorithm>

#include <utki/debug.hpp>
#include <utki/Singleton.hpp>
//#include "math.hpp"

#include <nitki/Thread.hpp>
#include <nitki/Semaphore.hpp>

#include "tick.hpp"


namespace aika{



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
	virtual void onExpired()noexcept = 0;

	/**
	 * @brief Constructor for new Timer instance.
	 * The newly created timer is initially not running.
	 */
	Timer(){
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
	void start(std::uint32_t millisec);

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
	bool stop()noexcept;
};



/**
 * @brief Timer library singleton class.
 * This is a singleton class which represents timer library which allows using
 * timers (see ting::Timer class). Before using timers one needs to initialize
 * the timer library, this is done just by creating the singleton object of
 * the timer library class.
 */
class Lib : public utki::IntrusiveSingleton<Lib>{
	friend class utki::IntrusiveSingleton<Lib>;
	static utki::IntrusiveSingleton<Lib>::T_Instance instance;
	
	friend class aika::Timer;

	class TimerThread : public nitki::Thread{
	public:
		volatile bool quitFlag = false;

		std::mutex mutex;
		nitki::Semaphore sema;

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
		std::uint64_t getTicks();



		TimerThread(){
			ASSERT(!this->quitFlag)
		}

		~TimerThread()noexcept{
			//at the time of TimerLib destroying there should be no active timers
			ASSERT(this->timers.size() == 0)
		}

		void addTimer_ts(Timer* timer, std::uint32_t timeout);

		bool removeTimer_ts(Timer* timer)noexcept;

		void setQuitFlagAndSignalSemaphore()noexcept{
			this->quitFlag = true;
			this->sema.signal();
		}

		void run()override;

	} thread;

	class HalfMaxTicksTimer : public Timer{
	public:
		//override
		void onExpired()noexcept{
			try{
				this->start(Timer::DMaxTicks() / 2);
			}catch(...){
				ASSERT(false)
			}
		}
	} halfMaxTicksTimer;

public:
	Lib(){
		this->thread.start();

		//start timer for half of the max ticks
		this->halfMaxTicksTimer.onExpired();
	}

	/**
	 * @brief Destructor.
	 * Note, that before destroying the timer library singleton object all the
	 * timers should be stopped. Otherwise, in debug mode it will result in assertion failure.
	 */
	~Lib()noexcept{
		//stop half max ticks timer
		while(!this->halfMaxTicksTimer.stop()){
			nitki::Thread::sleep(10);
		}
#ifdef DEBUG
		{
			std::lock_guard<decltype(this->thread.mutex)> mutexGuard(this->thread.mutex);
			ASSERT(this->thread.timers.size() == 0)
		}
#endif
		this->thread.setQuitFlagAndSignalSemaphore();
		this->thread.join();
	}
};



inline Timer::~Timer()noexcept{
	ASSERT_INFO(!this->isRunning, "trying to destroy running timer. Stop the timer first and make sure its OnExpired() method will not be called, then destroy the timer object.")
}



inline void Timer::start(std::uint32_t millisec){
	ASSERT_INFO(Lib::isCreated(), "Timer library is not initialized, you need to create TimerLib singletone object first")

	Lib::inst().thread.addTimer_ts(this, millisec);
}



inline bool Timer::stop()noexcept{
	ASSERT(Lib::isCreated())
	return Lib::inst().thread.removeTimer_ts(this);
}



inline std::uint64_t Lib::TimerThread::getTicks(){
	std::uint32_t ticks = aika::getTicks() % Timer::DMaxTicks();

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
