#pragma once

#include <utki/debug.hpp>
#include "../../src/aika/timer.hpp"

#include "tests.hpp"


inline void TestTingTimer(){
	aika::Lib timerLib;

	BasicTimerTest::Run();
	SeveralTimersForTheSameInterval::Run();
	StoppingTimers::Run();

	TRACE_ALWAYS(<< "[PASSED]: Timer test" << std::endl)
}
