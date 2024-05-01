#pragma once
#include "COMMON.h"

#include <atomic>
#include <utility>

#include "etl/span.h"

namespace Communicator
{
	constexpr const char *const TAG = "Communicator";

	constexpr size_t buf_len = 128 * 1024;

	//

	esp_err_t cleanup();
	esp_err_t init();
	esp_err_t deinit();

	esp_err_t time_settings(size_t);

	bool write_4bytes(const int64_t &, const uint32_t &);

	template <typename T>
	bool write_data(const int64_t &time, const T &val)
		requires(sizeof(T) == sizeof(uint32_t))
	{
		return write_4bytes(time, std::bit_cast<uint32_t>(val));
		// return write_uint32(time, *reinterpret_cast<uint32_t *>(&val));
	}

	etl::span<char> get_read();
	void commit_read();

	bool check_if_running();
	void start_running();
	void ask_for_exit();
	bool check_if_should_exit();
	void confirm_exit();

};
