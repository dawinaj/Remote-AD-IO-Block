
#include "exchange_hdlr.h"

// #include <string>
// #include <sys/param.h>
// #include <sys/socket.h>
// #include <sys/time.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/error/en.h"
#include "SocketIStream.h"
#include "SocketOStream.h"

#include "Exceptions.h"

#include "socket_helper.h"
#include "json_helper.h"

static const char *TAG = "queu_hdlr";

//

void exchange_hdlr_task(void *pvParameters)
{
	conn_list_t &conn_list = *static_cast<conn_list_t *>(pvParameters);

	ESP_LOGW(TAG, "Exchange task started");

	vTaskDelay(pdMS_TO_TICKS(1000));

	while (true)
	{
		int conn_sock;
		int res;

		while (conn_list.empty())
			vTaskDelay(pdMS_TO_TICKS(100));

		conn_sock << conn_list;

		ESP_LOGW(TAG, "Exchange estabilished on socket %i", conn_sock);

		LOG_MEMORY_MAX;

		rapidjson::Document doc;

		create_queue_response(doc, 0);

		res = sock_get_error(conn_sock);
		if (res != 0)
			goto shutdown_conn_sock;

		res = send_document_to_socket(conn_sock, doc);
		if (res <= 0)
			goto shutdown_conn_sock;

		LOG_MEMORY_MAX;

		while (1)
		{
			while (true)
			{
				res = sock_has_data(conn_sock);
				if (res < 0)
				{
					ESP_LOGE(TAG, "Has rerr: %s", strerror(-res));
				}
				if (res == 0) // or just has nothing, ffs
				{
					ESP_LOGD(TAG, "Disconnect");
					goto shutdown_conn_sock;
				}
				if (res > 0)
				{
					ESP_LOGI(TAG, "Has data: %i", res);
					break;
				}

				vTaskDelay(pdMS_TO_TICKS(1000));
			}

			LOG_MEMORY_MAX;

			res = get_document_from_socket(conn_sock, doc);
			if (res <= 0)
				goto shutdown_conn_sock;
			LOG_MEMORY_MAX;
			//================
			// do stuff here

			vTaskDelay(1000 / portTICK_PERIOD_MS);

			// end doing here
			//================
			res = send_document_to_socket(conn_sock, doc);
			if (res <= 0)
				goto shutdown_conn_sock;
			LOG_MEMORY_MAX;

			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}

	shutdown_conn_sock:
		shutdown(conn_sock, 0);
		close(conn_sock);

		vTaskDelay(pdMS_TO_TICKS(1000));
	}

	vTaskDelete(NULL);
}