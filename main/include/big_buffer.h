#pragma once

#include <array>
#include <mutex>

template <typename T, size_t len, size_t cnt>
class ADC_Storage
{
	using buffer_type = std::array<T, len>;
	std::array<buffer_type, cnt> buffers;
	std::array<std::mutex, cnt> mutexes;
}