#include "Communicator.h"

#include "etl/bip_buffer_spsc_atomic.h"

namespace Communicator
{
	// STATE MACHINE
	etl::bip_buffer_spsc_atomic<char, buf_len> bipbuf;

	etl::span<char> current_read;

	size_t time_bytes = 0;
	size_t res_wrt_4b = 0;

	//================================//
	//         IMPLEMENTATION         //
	//================================//

	//----------------//
	//    FRONTEND    //
	//----------------//

	esp_err_t cleanup()
	{
		bipbuf.clear();
		please_exit.store(false, std::memory_order::relaxed);
		producer_running.store(false, std::memory_order::relaxed);
		return ESP_OK;
	}

	esp_err_t init()
	{
		cleanup();
		return ESP_OK;
	}

	esp_err_t deinit()
	{
		cleanup();
		return ESP_OK;
	}

	esp_err_t time_settings(size_t b)
	{
		time_bytes = std::min<size_t>(b, 8);
		res_wrt_4b = time_bytes + 4;
		return ESP_OK;
	}

	bool write_4bytes(const int64_t &time, const uint32_t &val)
	{
		etl::span<char> rsvd = bipbuf.write_reserve_optimal(res_wrt_4b);

		if (/*rsvd.data() == nullptr ||*/ rsvd.size() != res_wrt_4b) [[unlikely]]
		{
			ESP_LOGE(TAG, "Failed to reserve space for buffer writing!");
			return false;
		}
		std::copy(reinterpret_cast<const char *>(&val),
				  reinterpret_cast<const char *>(&val) + 4,
				  rsvd.data());

		std::copy(reinterpret_cast<const char *>(&time),
				  reinterpret_cast<const char *>(&time) + time_bytes,
				  rsvd.data() + 4);

		bipbuf.read_commit(rsvd);
		return true;
	}

	etl::span<char> get_read()
	{
		current_read = bipbuf.read_reserve();
		return current_read;
	}
	void commit_read()
	{
		bipbuf.read_commit(current_read);
	}

};
