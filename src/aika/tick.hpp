#pragma once

#include <cstdint>

#include <utki/config.hpp>

namespace aika{


/**
 * @brief Get constantly increasing millisecond ticks.
 * It is not guaranteed that the ticks counting started at the system start.
 * @return constantly increasing millisecond ticks.
 */
std::uint32_t getTicks();

	
}

