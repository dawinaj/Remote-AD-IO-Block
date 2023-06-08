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

#define SISTAG "SocketIStream"

//

template <size_t capacity = 1024>
class SocketIStream
{
public:
	using Ch = char;

private:
	int sock_fd = 0;
	std::array<Ch, capacity + 1> buffer = {};
	size_t totlLen = 0;
	size_t readPos = 0;
	size_t dataLen = 0;

public:
	SocketIStream(const int _sfd) : sock_fd(_sfd) {}
	~SocketIStream() = default;
	SocketIStream(const SocketIStream &) = delete;
	SocketIStream &operator=(const SocketIStream &) = delete;

	// for IStream ✓
	Ch Peek()
	{
		checkBufferAndReceive();
		return buffer[readPos];
	}
	Ch Take()
	{
		checkBufferAndReceive();
		++totlLen;
		return buffer[readPos++];
	}
	size_t Tell() const
	{
		return totlLen;
	}

	// for OStream ✗
	void Put(Ch)
	{
		assert(false);
	}
	void Flush()
	{
		assert(false);
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
	// if data is still available, return immediately, otherwise try to receive more
	void checkBufferAndReceive()
	{
		if (readPos < dataLen) // if data is still available, return immediately
			return;

		int result = recv(sock_fd, buffer.data(), capacity, 0); // try to receive more

		if (result < 0)
		{
			ESP_LOGE(SISTAG, "Error occurred during sending: errno %d:\n%s", errno, strerror(errno));
			ESP_LOGE(SISTAG, "Error occurred during sending: errno %d:\n%s", sock_get_error(sock_fd), strerror(sock_get_error(sock_fd)));
			throw socket_error(strerror(errno));
		}
		if (result == 0)
		{
			ESP_LOGW(SISTAG, "Client has disconnected!");
			throw disconnect_warning("Client has disconnected!");
		}

		readPos = 0; // reset and setup buffer parameters
		dataLen = result;
		buffer[dataLen] = '\0';

		ESP_LOGI(SISTAG, "Transmission ended, received %u bytes:", dataLen);
		std::cout << buffer.data() << std::endl;
	}
};

#undef SISTAG
