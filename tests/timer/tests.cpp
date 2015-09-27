#include <vector>

#include <utki/debug.hpp>
#include "../../src/aika/timer.hpp"

#include "tests.hpp"



namespace BasicTimerTest{

struct TestTimer1 : public aika::Timer{
	volatile bool *e;

	TestTimer1(volatile bool* exitFlag) :
			e(exitFlag)
	{}

	void onExpired()noexcept override{
		TRACE_ALWAYS(<< "\t- timer1 fired!" << std::endl)
		*this->e = true;
	}
};



struct TestTimer2 : public aika::Timer{
	TestTimer2(){}

	void onExpired()noexcept override{
		TRACE_ALWAYS(<< "\t- timer2 fired!" << std::endl)

		this->start(2500);
	}
};



void Run(){
	TRACE_ALWAYS(<< "\tRunning BasicTimerTest, it will take about 5 seconds..." << std::endl)

	//should be volatile since can be changed by another threads. This is to avoid the checks of this variable to be optimized away by compiler.
	volatile bool exit = false;

	TestTimer1 timer1(&exit);
	TestTimer2 timer2;

	timer1.start(5000);
	timer2.start(2500);

//	TRACE_ALWAYS(<< "loop " << std::endl)
	
	for(unsigned i = 0; !exit; ++i){
		nitki::Thread::sleep(100);
		ASSERT_ALWAYS(i != 60)
	}

	nitki::Thread::sleep(50);
	
	while(!timer2.stop()){
		nitki::Thread::sleep(50);
	}
}

}//~namespace



namespace SeveralTimersForTheSameInterval{

struct TestTimer : public aika::Timer{
	unsigned *e;
	std::mutex* m;

	TestTimer(unsigned* cnt, std::mutex* mut) :
			e(cnt),
			m(mut)
	{
		ASSERT_ALWAYS(this->m)
		ASSERT_ALWAYS(this->e)
	}

	void onExpired()noexcept override{
//		TRACE_ALWAYS(<<"\t- timer fired!"<<std::endl)
		std::lock_guard<decltype(*this->m)> mutexGuard(*this->m);
		++(*this->e);
	}
};



void Run(){
	TRACE_ALWAYS(<< "\tRunning SeveralTimersForTheSameInterval, it will take about 1 second..." << std::endl)
	
	std::mutex mutex;
	unsigned counter = 0;
	
	const unsigned DNumTimers = 100;
	
	typedef std::vector<std::unique_ptr<TestTimer> > T_TimerList;
	typedef T_TimerList::iterator T_TimerIter;
	T_TimerList timers;
	
	for(unsigned i = 0; i != DNumTimers; ++i){
		timers.push_back(std::unique_ptr<TestTimer>(
				new TestTimer(&counter, &mutex)
			));
	}
	
	for(T_TimerIter i = timers.begin(); i != timers.end(); ++i){
		(*i)->start(500);
	}
	
	nitki::Thread::sleep(1000);
	
	ASSERT_ALWAYS(counter == DNumTimers)
}

}//~namespace



namespace StoppingTimers{

struct TestTimer : public aika::Timer{
	bool *e;

	TestTimer(bool* exitFlag) :
			e(exitFlag)
	{}

	void onExpired()noexcept override{
//		TRACE_ALWAYS(<<"\t- timer1 fired!"<<std::endl)
		*this->e = true;
	}
};



void Run(){
	TRACE_ALWAYS(<< "\tRunning StoppingTimers, it will take about 3 seconds..." << std::endl)
	
	bool exit1 = false;
	bool exit2 = false;
	
	
	TestTimer timer1(&exit1);
	TestTimer timer2(&exit2);
	
	timer1.start(3000);
	ASSERT_ALWAYS(!exit1)
	ASSERT_ALWAYS(!exit2)
	
	nitki::Thread::sleep(1000);
	ASSERT_ALWAYS(!exit1)
	ASSERT_ALWAYS(!exit2)
	
	timer2.start(1000);
	
	nitki::Thread::sleep(500);
	ASSERT_ALWAYS(!exit1)
	ASSERT_ALWAYS(!exit2)
	
	bool stopRes = timer2.stop();
	ASSERT_ALWAYS(stopRes)
	ASSERT_ALWAYS(!exit1)
	ASSERT_ALWAYS(!exit2)
	
	nitki::Thread::sleep(1000);
	ASSERT_ALWAYS(!exit1)
	ASSERT_ALWAYS(!exit2)
	
	nitki::Thread::sleep(1000);
	ASSERT_ALWAYS(exit1)
	ASSERT_ALWAYS(!exit2)
}

}//~namespace
