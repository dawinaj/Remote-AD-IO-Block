
#include "connection_hdlr.h"

// #include <string>
// #include <sys/param.h>
// #include <sys/socket.h>
// #include <sys/time.h>

// #include "esp_event.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include "rapidjson/document.h"
#include "socket_helper.h"

static const char *TAG = "conn_hdlr";

//

static constexpr int keepAlive = 1;
static constexpr int keepIdle = KEEPALIVE_IDLE;
static constexpr int keepInterval = KEEPALIVE_INTERVAL;
static constexpr int keepCount = KEEPALIVE_COUNT;

//

template <int addr_family>
void connection_hdlr_task(void *pvParameters)
{
	conn_list_t &conn_list = *static_cast<conn_list_t *>(pvParameters);

	char addr_str[40];
	int ip_protocol = 0;
	struct sockaddr_storage dest_addr;

#if HAS_IPV4
	if constexpr (addr_family == AF_INET)
	{
		struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
		dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
		dest_addr_ip4->sin_family = AF_INET;
		dest_addr_ip4->sin_port = htons(PORT);
		ip_protocol = IPPROTO_IP;
	}
#endif
#if HAS_IPV6
	if constexpr (addr_family == AF_INET6)
	{
		struct sockaddr_in6 *dest_addr_ip6 = (struct sockaddr_in6 *)&dest_addr;
		bzero(&dest_addr_ip6->sin6_addr.un, sizeof(dest_addr_ip6->sin6_addr.un));
		dest_addr_ip6->sin6_family = AF_INET6;
		dest_addr_ip6->sin6_port = htons(PORT);
		ip_protocol = IPPROTO_IPV6;
	}
#endif

	int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
	if (listen_sock < 0)
	{
		ESP_LOGE(TAG, "Unable to create socket: errno %d:\n%s", errno, strerror(errno));
		vTaskDelete(NULL);
		// return;
	}

	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &YES, sizeof(int));

#if HAS_IPV4 && HAS_IPV6
	setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &YES, sizeof(int));
#endif

	ESP_LOGI(TAG, "Socket created");

	int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
	if (err != 0)
	{
		ESP_LOGE(TAG, "Socket unable to bind: errno %d:\n%s", errno, strerror(errno));
		ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
		goto CLEAN_UP;
	}
	ESP_LOGI(TAG, "Socket bound, port %d", PORT);

	err = listen(listen_sock, 1);
	if (err != 0)
	{
		ESP_LOGE(TAG, "Error occurred during listen: errno %d:\n%s", errno, strerror(errno));
		goto CLEAN_UP;
	}

	while (1)
	{
		ESP_LOGI(TAG, "Socket listening...");

		struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
		socklen_t addr_len = sizeof(source_addr);

		int conn_sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
		if (conn_sock < 0)
		{
			ESP_LOGE(TAG, "Unable to accept connection: errno %d:\n%s", errno, strerror(errno));
			errno = 0;
			continue;
		}

		// Convert ip address to string
		if (source_addr.ss_family == PF_INET)
			inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
		else if (source_addr.ss_family == PF_INET6)
			inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, addr_str, sizeof(addr_str) - 1);
		else
			ESP_LOGW(TAG, "Invalid IP address family (neither v4 nor v6)!");

		ESP_LOGI(TAG, "Socket accepted IP address: %s", addr_str);

		// Set tcp keepalive option
		setsockopt(conn_sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
		setsockopt(conn_sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
		setsockopt(conn_sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
		setsockopt(conn_sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

		// Set linger and timeout
		struct linger lngr = {1, 15}; // 10 seconds to send remaining data
		struct timeval tmr = {30, 0}; // timeout on inter-data receive/send

		setsockopt(conn_sock, SOL_SOCKET, SO_LINGER, &lngr, sizeof(struct linger));
		setsockopt(conn_sock, SOL_SOCKET, SO_RCVTIMEO, &tmr, sizeof(struct timeval));
		setsockopt(conn_sock, SOL_SOCKET, SO_SNDTIMEO, &tmr, sizeof(struct timeval));

		LOG_MEMORY_MAX;

		// Create response about queue
		rapidjson::Document doc;
		create_queue_response(doc, conn_list.size() + 1);

		LOG_MEMORY_MAX;
		// If succesfully sent, push to queue
		if (send_document_to_socket(conn_sock, doc) > 0)
			conn_sock >> conn_list;

		LOG_MEMORY_MAX;
	}

CLEAN_UP:
	errno = 0;
	close(listen_sock);
	vTaskDelete(NULL);
}

template void connection_hdlr_task<AF_INET>(void *pvParameters);
template void connection_hdlr_task<AF_INET6>(void *pvParameters);

//
