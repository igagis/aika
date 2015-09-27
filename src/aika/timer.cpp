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



#include "timer.hpp"



using namespace aika;



utki::IntrusiveSingleton<Lib>::T_Instance Lib::instance;



bool Lib::TimerThread::removeTimer_ts(Timer* timer)noexcept{
	ASSERT(timer)
	std::lock_guard<decltype(this->mutex)> mutexGuard(this->mutex);

	if(!timer->isRunning){
		//lock and unlock the 'expired' mutex to make sure that the timer's callback
		//has been called if the timer has expired and is awaiting the notification callback to be called.
		std::lock_guard<decltype(this->expiredTimersNotifyMutex)> mutexGuard(this->expiredTimersNotifyMutex);
		return false;
	}

	//if isStarted flag is set then the timer will be stopped now, so
	//change the flag
	timer->isRunning = false;

	ASSERT(timer->i != this->timers.end())

	//if that was the first timer, signal the semaphore about timer deletion in order to recalculate the waiting time
	if(this->timers.begin() == timer->i){
		this->sema.signal();
	}

	this->timers.erase(timer->i);

	//was running
	return true;
}



void Lib::TimerThread::addTimer_ts(Timer* timer, std::uint32_t timeout){
	ASSERT(timer)
	std::lock_guard<decltype(this->mutex)> mutexGuard(this->mutex);

	if(timer->isRunning){
		throw utki::Exc("Lib::TimerThread::AddTimer(): timer is already running!");
	}

	timer->isRunning = true;

	std::uint64_t stopTicks = this->getTicks() + std::uint64_t(timeout);

	timer->i = this->timers.insert(
			std::pair<std::uint64_t, Timer*>(stopTicks, timer)
		);

	ASSERT(timer->i != this->timers.end())
	ASSERT(timer->i->second)

	//signal the semaphore about new timer addition in order to recalculate the waiting time
	this->sema.signal();
}



//override
void Lib::TimerThread::run(){
	while(!this->quitFlag){
		std::uint32_t millis;

		while(true){
			std::vector<Timer*> expiredTimers;

			{
				std::lock_guard<decltype(this->mutex)> mutexGuard(this->mutex);

				std::uint64_t ticks = this->getTicks();

				for(Timer::T_TimerIter b = this->timers.begin(); b != this->timers.end(); b = this->timers.begin()){
					if(b->first > ticks){
						break;//~for
					}

					Timer *timer = b->second;
					//add the timer to list of expired timers
					ASSERT(timer)
					expiredTimers.push_back(timer);

					//Change the expired timer state to not running.
					//This should be done before the expired signal of the timer will be emitted.
					timer->isRunning = false;

					this->timers.erase(b);
				}

				if(expiredTimers.size() == 0){
					ASSERT(this->timers.size() > 0) //if we have no expired timers here, then at least one timer should be running (the half-max-ticks timer).

					//calculate new waiting time
					ASSERT(this->timers.begin()->first > ticks)
					ASSERT(this->timers.begin()->first - ticks <= std::uint64_t(std::uint32_t(-1)))
					millis = std::uint32_t(this->timers.begin()->first - ticks);

					//zero out the semaphore for optimization purposes
					while(this->sema.wait(0)){}

					break;//~while(true)
				}
				
				this->expiredTimersNotifyMutex.lock();
			}

			try{
				//emit expired signal for expired timers
				for(std::vector<Timer*>::iterator i = expiredTimers.begin(); i != expiredTimers.end(); ++i){
					ASSERT(*i)
					(*i)->onExpired();
				}
			}catch(...){
				//no exceptions should be thrown by this code. Especially we don't want them here because
				//mutex locking/unlocking is used without mutex guard.
				ASSERT(false)
			}
			
			this->expiredTimersNotifyMutex.unlock();
		}

		this->sema.wait(millis);
	}//~while(!this->quitFlag)
}//~Run()
