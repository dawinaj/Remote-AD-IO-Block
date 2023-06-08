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

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
namespace rj = rapidjson;

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

	rj::Document doc;
	create_welcome_response(doc);

	rj::StringBuffer strbuf;
	//	strbuf.Clear();

	rj::Writer<rj::StringBuffer> writer(strbuf);
	doc.Accept(writer);

	httpd_resp_send(req, strbuf.GetString(), HTTPD_RESP_USE_STRLEN);

	return ESP_OK;
}

static esp_err_t get_handler(httpd_req_t *req)
{
	httpd_resp_set_status(req, HTTPD_200);
	httpd_resp_set_type(req, "application/json");

	static const char *message = R"rawlit({"status":"ok","data":{}})rawlit";

	httpd_resp_send(req, message, HTTPD_RESP_USE_STRLEN);

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

	rj::Document doc;
	rj::Document::AllocatorType &alctr = doc.GetAllocator();

	create_ok_response(doc);
	doc["data"].AddMember("gpio_diode", lvl, alctr);
	doc["data"].AddMember("gpio_button", gpio_get_level(GPIO_NUM_19), alctr);

	doc["message"] = "Diode is: ${data.gpio_diode}\nButton is: ${data.gpio_button}";

	rj::StringBuffer strbuf;
	rj::Writer<rj::StringBuffer> writer(strbuf);
	doc.Accept(writer);

	httpd_resp_send(req, strbuf.GetString(), HTTPD_RESP_USE_STRLEN);

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

httpd_handle_t start_webserver()
{
	httpd_handle_t server = NULL;

	httpd_config_t config = {
		.task_priority = HTTP_PRT,
		.stack_size = HTTP_MEM,
		.core_id = CPU1,
		.server_port = 80,
		.ctrl_port = 32768,
		.max_open_sockets = 7,
		.max_uri_handlers = 8,
		.max_resp_headers = 8,
		.backlog_conn = 5,
		.lru_purge_enable = false,
		.recv_wait_timeout = 5,
		.send_wait_timeout = 5,
		.global_user_ctx = NULL,
		.global_user_ctx_free_fn = NULL,
		.global_transport_ctx = NULL,
		.global_transport_ctx_free_fn = NULL,
		.enable_so_linger = false,
		.linger_timeout = 0,
		.keep_alive_enable = false,
		.keep_alive_idle = 0,
		.keep_alive_interval = 0,
		.keep_alive_count = 0,
		.open_fn = NULL,
		.close_fn = NULL,
		.uri_match_fn = NULL,
	};

	// Start the httpd server
	ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);

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

static esp_err_t stop_webserver(httpd_handle_t server)
{
	// Stop the httpd server
	return httpd_stop(server);
}

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
