
#include "socket_helper.h"

#include "esp_log.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/error/en.h"

#include "SocketIStream.h"
#include "SocketOStream.h"

#include "Exceptions.h"

using namespace rapidjson;

static const char *TAG = "SockHelp";

//

int sock_get_error(const int sock_fd)
{
	int ret;
	socklen_t ret_size = sizeof(ret);
	if (getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &ret, &ret_size) == -1)
		return -errno;
	return ret;
}

// int sock_has_data(const int sock_fd)
// {
// 	int ret;
// 	socklen_t ret_size = sizeof(ret);
// 	if (getsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &ret, &ret_size) != 0)
// 	{
// 		ESP_LOGE(TAG, "sock_has_data error: %s", strerror(errno));
// 		return -errno;
// 	}
// 	ESP_LOGW(TAG, "sock_has_data gud, RCVBUF: %i", ret);
// 	return ret;
// }
int sock_has_data(const int sock_fd)
{
	int ret;

	if (ioctl(sock_fd, FIONREAD, &ret) == -1)
	{
		//		ESP_LOGE(TAG, "sock_has_data error: %s", strerror(errno));
		return -errno;
	}
	//	ESP_LOGW(TAG, "sock_has_data gud, RCVBUF: %i", ret);
	return ret;
}

//

ssize_t sendall(int sock_fd, const void *buffer, size_t length, int flags)
{
	const uint8_t *data = static_cast<const uint8_t *>(buffer);
	size_t written = 0;

	while (written < length)
	{
		int res = send(sock_fd, data + written, length - written, flags);
		if (res < 0)
		{
			ESP_LOGE(TAG, "Error occurred during sending: errno %d:\n%s", errno, strerror(errno));
			return res;
		}
		written += res;
	}
	return written;
}

//

int send_document_to_socket(int sock_fd, const Document &doc)
{
	errno = 0;

	try
	{
		SocketOStream<OUT_BUF> sock_stream(sock_fd);
		Writer json_writer(sock_stream);
		doc.Accept(json_writer);
		return sock_stream.Tell();
	}
	catch (const socket_error &e)
	{
		ESP_LOGW(TAG, "Socket error: \n%s", e.what());
		return -std::abs(sock_get_error(sock_fd));
	}
	catch (const disconnect_warning &e)
	{
		ESP_LOGW(TAG, "Warning: \n%s", e.what());
		return DISCONNECT;
	}
	catch (const std::exception &e)
	{
		ESP_LOGW(TAG, "Generic exception: \n%s", e.what());
		return EXCEPTION;
	}

	return 0;
}

error_t get_document_from_socket(int sock_fd, Document &doc)
{
	errno = 0;

	try
	{
		SocketIStream<INP_BUF> sock_stream(sock_fd);
		ParseResult result = doc.ParseStream<kParseStopWhenDoneFlag>(sock_stream);

		if (result.IsError())
		{
			ESP_LOGW(TAG, "JSON parse error at pos (%u): errno %d:\n%s", result.Offset(), result.Code(), GetParseError_En(result.Code()));
			throw json_error("JSON data has error!");
		}

		return sock_stream.Tell();
	}
	catch (const socket_error &e)
	{
		ESP_LOGW(TAG, "Socket error: \n%s", e.what());
		return -std::abs(sock_get_error(sock_fd));
	}
	catch (const json_error &e)
	{
		ESP_LOGW(TAG, "Json error: \n%s", e.what());
		return sock_get_error(sock_fd);
	}
	catch (const disconnect_warning &e)
	{
		ESP_LOGW(TAG, "Warning: \n%s", e.what());
		return sockdoc_err_t::DISCONNECT;
	}
	catch (const std::exception &e)
	{
		ESP_LOGW(TAG, "Generic exception: \n%s", e.what());
		return sockdoc_err_t::EXCEPTION;
	}

	return 0;
}
