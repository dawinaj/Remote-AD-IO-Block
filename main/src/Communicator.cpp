#include "Communicator.h"

#include "etl/bip_buffer_spsc_atomic.h"

namespace Communicator
{
	// STATE MACHINE
	etl::bip_buffer_spsc_atomic<char, buf_len> bipbuf;

	etl::span<char> current_read;

	size_t time_bytes = 0;
	size_t res_wrt = 0;

	//================================//
	//         IMPLEMENTATION         //
	//================================//

	//----------------//
	//    FRONTEND    //
	//----------------//

	esp_err_t cleanup()
	{
		res_wrt = sizeof(data_t) + time_bytes;
		bipbuf.clear();
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
		time_bytes = std::min(b, 8);
		return ESP_OK;
	}

	bool write_uint(int64_t &time, uint32_t &val)
	{
		etl::span<char> rsvd = bipbuf.write_reserve_optimal(res_wrt);

		if (rsvd.data() == nullptr || rsvd.size() != res_wrt) [[unlikely]]
		{
			ESP_LOGE(TAG, "Failed to reserve space for buffer writing!");
			return false;
		}
		std::copy(reinterpret_cast<const char *>(&val),
				  reinterpret_cast<const char *>(&val) + sizeof(data_t),
				  rsvd.data());

		std::copy(reinterpret_cast<const char *>(&time),
				  reinterpret_cast<const char *>(&time) + time_bytes,
				  rsvd.data() + sizeof(data_t));

		bipbuf.read_commit(rsvd);
		return true;
	}
	bool write_float(int64_t &, float &)
	{
		return 1;
	}

	void get_read()
	{
		current_read = bipbuf.read_reserve();
		return current_read;
	}
	void commit_read()
	{
		bipbuf.read_commit(current_read);
		return;
	}

};
