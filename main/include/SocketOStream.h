#pragma once

#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include <array>
#include <iostream>

#include "Exceptions.h"

#include "socket_helper.h"

#define SOSTAG "SocketOStream"

//

template <size_t capacity = 1024>
class SocketOStream
{
public:
	using Ch = char;

private:
	int sock_fd = 0;
	std::array<Ch, capacity + 1> buffer = {};
	size_t totlLen = 0;
	size_t dataLen = 0;

public:
	SocketOStream(const int _sfd) : sock_fd(_sfd) {}
	~SocketOStream() = default;
	SocketOStream(const SocketOStream &) = delete;
	SocketOStream &operator=(const SocketOStream &) = delete;

	// for OStream ✓
	void Put(Ch c)
	{
		checkBufferAndSend();
		buffer[dataLen++] = c;
	}
	void Flush()
	{
		checkBufferAndSend(true);
	}
	size_t Tell() const
	{
		return totlLen;
	}

	// for IStream ✗
	Ch Peek() const
	{
		assert(false);
		return '\0';
	}
	Ch Take()
	{
		assert(false);
		return '\0';
	}
	// why is it needed? 🛇
	Ch *PutBegin()
	{
		assert(false);
		return nullptr;
	}
	size_t PutEnd(Ch *)
	{
		assert(false);
		return 0;
	}

private:
	// if no data, return immediately, otherwise send everything you have
	void checkBufferAndSend(bool force = false)
	{
		if (dataLen == 0) // if no data to send, return immediately
			return;

		if (dataLen < capacity && !force) // if not full and not forced, return immediately
			return;

		buffer[dataLen] = '\0'; // terminate string

		size_t written = 0;
		while (written < dataLen) // loop sending until nothing in buffer
		{
			int result = send(sock_fd, buffer.data() + written, dataLen - written, 0);

			if (result < 0)
			{
				ESP_LOGE(SOSTAG, "Error occurred during sending: errno %d:\n%s", errno, strerror(errno));
				ESP_LOGE(SOSTAG, "Error occurred during sending: errno %d:\n%s", sock_get_error(sock_fd), strerror(std::abs(sock_get_error(sock_fd))));
				throw socket_error(strerror(errno));
			}
			if (result == 0)
			{
				ESP_LOGW(SOSTAG, "Client has disconnected!");
				throw disconnect_warning("Client has disconnected!");
			}

			written += result;
		}
		totlLen += written;

		dataLen = 0;

		ESP_LOGI(SOSTAG, "Transmission ended, sent %u bytes:", written);
		std::cout << buffer.data() << std::endl;
	}
};

#undef SOSTAG
