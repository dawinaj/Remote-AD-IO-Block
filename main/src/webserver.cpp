#include "settings.h"

// #include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <esp_netif.h>
#include <esp_eth.h>

#include <esp_http_server.h>

#include "json_helper.h"

//

#include <driver/gpio.h>

//

static const char *TAG = "WebServer";

/* An HTTP GET handler */
// static esp_err_t basic_auth_get_handler(httpd_req_t *req)
// {
// 	char *buf = NULL;
// 	size_t buf_len = 0;
// 	basic_auth_info_t *basic_auth_info = req->user_ctx;
// 	buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
// 	if (buf_len > 1)
// 	{
// 		buf = calloc(1, buf_len);
// 		if (!buf)
// 		{
// 			ESP_LOGE(TAG, "No enough memory for basic authorization");
// 			return ESP_ERR_NO_MEM;
// 		}
// 		if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK)
// 		{
// 			ESP_LOGI(TAG, "Found header => Authorization: %s", buf);
// 		}
// 		else
// 		{
// 			ESP_LOGE(TAG, "No auth value received");
// 		}
// 		char *auth_credentials = http_auth_basic(basic_auth_info->username, basic_auth_info->password);
// 		if (!auth_credentials)
// 		{
// 			ESP_LOGE(TAG, "No enough memory for basic authorization credentials");
// 			free(buf);
// 			return ESP_ERR_NO_MEM;
// 		}
// 		if (strncmp(auth_credentials, buf, buf_len))
// 		{
// 			ESP_LOGE(TAG, "Not authenticated");
// 			httpd_resp_set_status(req, HTTPD_401);
// 			httpd_resp_set_type(req, "application/json");
// 			httpd_resp_set_hdr(req, "Connection", "keep-alive");
// 			httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
// 			httpd_resp_send(req, NULL, 0);
// 		}
// 		else
// 		{
// 			ESP_LOGI(TAG, "Authenticated!");
// 			char *basic_auth_resp = NULL;
// 			httpd_resp_set_status(req, HTTPD_200);
// 			httpd_resp_set_type(req, "application/json");
// 			httpd_resp_set_hdr(req, "Connection", "keep-alive");
// 			asprintf(&basic_auth_resp, "{\"authenticated\": true,\"user\": \"%s\"}", basic_auth_info->username);
// 			if (!basic_auth_resp)
// 			{
// 				ESP_LOGE(TAG, "No enough memory for basic authorization response");
// 				free(auth_credentials);
// 				free(buf);
// 				return ESP_ERR_NO_MEM;
// 			}
// 			httpd_resp_send(req, basic_auth_resp, strlen(basic_auth_resp));
// 			free(basic_auth_resp);
// 		}
// 		free(auth_credentials);
// 		free(buf);
// 	}
// 	else
// 	{
// 		ESP_LOGE(TAG, "No auth header received");
// 		httpd_resp_set_status(req, HTTPD_401);
// 		httpd_resp_set_type(req, "application/json");
// 		httpd_resp_set_hdr(req, "Connection", "keep-alive");
// 		httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
// 		httpd_resp_send(req, NULL, 0);
// 	}
// 	return ESP_OK;
// }

static esp_err_t welcome_handler(httpd_req_t *req)
{
	httpd_resp_set_status(req, HTTPD_200);
	httpd_resp_set_type(req, "application/json");

	json doc = create_welcome_response();
	std::string out = doc.dump();
	httpd_resp_send(req, out.c_str(), out.length());

	return ESP_OK;
}

static char randommem[1000];

static esp_err_t get_handler(httpd_req_t *req)
{
	httpd_resp_set_status(req, HTTPD_200);
	httpd_resp_set_type(req, "multipart/mixed; boundary=" BOUNDARY);

	json doc = create_ok_response();
	std::string out = doc.dump();

	httpd_resp_sendstr_chunk(req, "--" BOUNDARY "\r\n");
	httpd_resp_sendstr_chunk(req, "Content-Type: application/json\r\n\r\n");
	httpd_resp_send_chunk(req, out.c_str(), out.length());

	for (int i = 100; i > 0; --i)
	{
		httpd_resp_sendstr_chunk(req, "\r\n--" BOUNDARY "\r\n");
		httpd_resp_sendstr_chunk(req, "Content-Type: application/octet-stream\r\n\r\n");
		httpd_resp_send_chunk(req, randommem, 1000);
	}

	httpd_resp_sendstr_chunk(req, "\r\n--" BOUNDARY "--\r\n");
	httpd_resp_sendstr_chunk(req, nullptr);

	return ESP_OK;
}

static esp_err_t put_handler(httpd_req_t *req)
{
	char buf;
	int ret;

	if ((ret = httpd_req_recv(req, &buf, 1)) <= 0)
	{
		if (ret == HTTPD_SOCK_ERR_TIMEOUT)
		{
			httpd_resp_send_408(req);
		}
		return ESP_FAIL;
	}

	/* Respond with empty body */
	httpd_resp_send(req, NULL, 0);
	return ESP_OK;
}

static esp_err_t gpio_handler(httpd_req_t *req)
{
	static int lvl = 0;
	if (lvl == 0)
		gpio_set_level(GPIO_NUM_18, lvl = 1);
	else
		gpio_set_level(GPIO_NUM_18, lvl = 0);

	httpd_resp_set_status(req, HTTPD_200);
	httpd_resp_set_type(req, "application/json");

	json doc = create_ok_response();
	doc["data"]["gpio_diode"] = lvl;
	doc["data"]["gpio_button"] = gpio_get_level(GPIO_NUM_19);

	doc["message"] = "Diode is: ${data.gpio_diode}\nButton is: ${data.gpio_button}";

	std::string out = doc.dump();
	httpd_resp_send(req, out.c_str(), out.length());

	return ESP_OK;
}

static const httpd_uri_t get_uri = {
	.uri = "/get",
	.method = HTTP_GET,
	.handler = get_handler,
	.user_ctx = nullptr,
};

static const httpd_uri_t put_uri = {
	.uri = "/put",
	.method = HTTP_PUT,
	.handler = put_handler,
	.user_ctx = nullptr,
};

static const httpd_uri_t welcome_uri = {
	.uri = "/",
	.method = HTTP_GET,
	.handler = welcome_handler,
	.user_ctx = nullptr,
};

static const httpd_uri_t gpio_uri = {
	.uri = "/gpio",
	.method = HTTP_GET,
	.handler = gpio_handler,
	.user_ctx = nullptr,
};

//

httpd_handle_t start_webserver()
{
	httpd_handle_t server = nullptr;

	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	// config.task_priority = HTTP_PRT;
	config.stack_size = HTTP_MEM;
	config.core_id = CPU0;
	// config.max_open_sockets = 1;

	// Start the httpd server
	ESP_LOGI(TAG, "Starting server on port: %d", config.server_port);

	if (httpd_start(&server, &config) == ESP_OK)
	{
		// Set URI handlers
		ESP_LOGI(TAG, "Registering URI handlers");
		httpd_register_uri_handler(server, &welcome_uri);
		httpd_register_uri_handler(server, &get_uri);
		httpd_register_uri_handler(server, &put_uri);
		httpd_register_uri_handler(server, &gpio_uri);
		return server;
	}

	ESP_LOGI(TAG, "Error starting server!");
	return NULL;
}

esp_err_t stop_webserver(httpd_handle_t server)
{
	// Stop the httpd server
	return httpd_stop(server);
}

/*/
void disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	httpd_handle_t *server = (httpd_handle_t *)arg;
	if (*server)
	{
		ESP_LOGI(TAG, "Stopping webserver");
		if (stop_webserver(*server) == ESP_OK)
		{
			*server = NULL;
		}
		else
		{
			ESP_LOGE(TAG, "Failed to stop http server");
		}
	}
}

void connect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	httpd_handle_t *server = (httpd_handle_t *)arg;
	if (*server == NULL)
	{
		ESP_LOGI(TAG, "Starting webserver");
		*server = start_webserver();
	}
}
//*/