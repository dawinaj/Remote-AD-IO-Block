#pragma once

#include <utility>

#include <esp_log.h>
#include <esp_check.h>

#include "etl/span.h"

namespace Communicator
{
	constexpr const char *const TAG = "Communicator";

	constexpr size_t buf_len = 8 * 1024;

	union data_t
	{
		float f;
		uint32_t u;
	};

	//	using WriteCb = std::function<bool(int16_t)>;

	esp_err_t cleanup();
	esp_err_t init();
	esp_err_t deinit();

	esp_err_t time_settings(bool = false, bool = false);

	bool write_uint(int64_t &, uint32_t &);
	bool write_float(int64_t &, float &);

	etl::span<char> get_read();
	void commit_read();

};
