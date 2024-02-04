#include "webserver.h"

#include "settings.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <map>
#include <memory> // move
#include <type_traits>

using namespace std::literals;

#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>

// #include <esp_http_server.h>

#include "rigtorp/SPSCQueue.h"
using namespace rigtorp;

#include "Interpreter.h"
using namespace Executing;

#include "json_helper.h"

//

static const char *TAG = "WebServer";

NLOHMANN_JSON_SERIALIZE_ENUM(In_Range,
							 {
								 {In_Range::OFF, "off"},
								 {In_Range::Min, "min"},
								 {In_Range::Med, "med"},
								 {In_Range::Max, "max"},
							 })

//

bool str_is_ascii(const std::string &s)
{
	return std::all_of(s.begin(), s.end(),
					   [](unsigned char ch)
					   { return ch <= 127; });
}

void str_to_lower(std::string &s)
{
	std::transform(s.begin(), s.end(), s.begin(),
				   [](unsigned char c)
				   { return std::tolower(c); });
}

//

static esp_err_t welcome_handler(httpd_req_t *req)
{
	httpd_resp_set_status(req, HTTPD_200);
	httpd_resp_set_type(req, "application/json");

	ordered_json doc = create_welcome_response();

	std::string out = doc.dump();
	httpd_resp_send(req, out.c_str(), out.length());

	return ESP_OK;
}

//

static esp_err_t settings_handler(httpd_req_t *req)
{
	std::string post(req->content_len, '\0');

	int ret = httpd_req_recv(req, post.data(), req->content_len);
	if (ret <= 0)
	{
		if (ret == HTTPD_SOCK_ERR_TIMEOUT)
			httpd_resp_send_408(req);

		return ESP_FAIL;
	}

	ESP_LOGV(TAG, "%s", post.c_str());

	std::vector<std::string> errors;

	if (str_is_ascii(post))
	{
		const json q = json::parse(post, nullptr, false);
		if (!q.is_discarded())
		{
			if (q.contains("program") && q.at("program").is_string())
			{
				// parse program
			}

			if (q.contains("generators") && q.at("generators").is_array())
			{
				// parse generators
				size_t count = q.at("generators").size();
				count = std::min(count, 16);

				while (count--)
					if (j.at("voltage_gen").is_array())
					{
						try
						{
							output.voltage_gen = j.at("voltage_gen").get<Generator>();
						}
						catch (json::exception &e)
						{
							errors.push_back("output.voltage_gen failed to parse!");
							ESP_LOGE(TAG, "%s", e.what());
						}
					}
			}
		}
		else
			errors.push_back("JSON is invalid!");
	}
	else
		errors.push_back("JSON is invalid!");

	if (!errors.empty())
	{
		httpd_resp_set_status(req, HTTPD_400);
		httpd_resp_set_type(req, "application/json");

		ordered_json res = create_err_response(errors);
		errors.clear();
		std::string out = res.dump();
		httpd_resp_send(req, out.c_str(), out.length());

		return ESP_OK;
	}

	board->set_config(std::move(program), std::move(generators));

	httpd_resp_set_status(req, HTTPD_200);
	httpd_resp_set_type(req, "application/json");

	ordered_json res = create_ok_response();
	res["message"] = "Settings have been validated. No errors found. Units: ${data.unit}";
	res["data"]["unit"]["in1"] = "V";
	res["data"]["unit"]["in2"] = "V";
	res["data"]["unit"]["in3"] = "V";
	res["data"]["unit"]["in4"] = "mA";
	std::string out = res.dump();
	httpd_resp_send(req, out.c_str(), out.length());

	return ESP_OK;
}

//

void board_io_task(void *arg)
{
	{
		auto *queue = static_cast<SPSCQueue<int16_t> *>(arg);

		auto writefcn = [queue](int16_t in) -> bool
		{
			return queue->try_push(in);
		};

		board->execute(std::move(writefcn));
	}
	vTaskSuspend(NULL);
	// wait for external wakeup
	vTaskDelete(NULL);
}

static esp_err_t io_handler(httpd_req_t *req)
{
	httpd_resp_set_status(req, HTTPD_200);
	httpd_resp_set_type(req, "application/octet-stream");

	ESP_LOGI(TAG, "Preparing queue...");

	constexpr size_t BUF_LEN = 1024;

	SPSCQueue<int16_t> queue(BUF_LEN * 32 - 1);

	auto send_measurements = [req, &queue](size_t len) -> esp_err_t
	{
		ESP_LOGI(TAG, "Sending %zu measurements... Approx size %zu", len, queue.size());
		const char *str = reinterpret_cast<const char *>(queue.front());
		esp_err_t ret = httpd_resp_send_chunk(req, str, len * sizeof(int16_t));
		while (len--)
			queue.pop();
		return ret;
	};

	ESP_LOGI(TAG, "Spawning task...");

	TaskHandle_t io_hdl = nullptr;
	xTaskCreatePinnedToCore(board_io_task, "BoardTask", BOARD_MEM, static_cast<void *>(&queue), BOARD_PRT, &io_hdl, CPU1);

	ESP_LOGI(TAG, "Running consumer...");

	do
	{
		if (queue.size() >= BUF_LEN)
			send_measurements(BUF_LEN);
		else
			vTaskDelay(1);
	} //
	while (eTaskGetState(io_hdl) != eTaskState::eSuspended);

	vTaskResume(io_hdl); // resumes to delete itself

	while (!queue.empty())
	{
		send_measurements(std::min(queue.size(), BUF_LEN));
		vTaskDelay(1);
	}

	httpd_resp_send_chunk(req, nullptr, 0);

	return ESP_OK;
}

/*/
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

	httpd_resp_send(req, NULL, 0);
	return ESP_OK;
}
//*/

/*/
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
//*/

static const httpd_uri_t welcome_uri = {
	.uri = "/",
	.method = HTTP_GET,
	.handler = welcome_handler,
	.user_ctx = nullptr,
};

static const httpd_uri_t io_uri = {
	.uri = "/io",
	.method = HTTP_GET,
	.handler = io_handler,
	.user_ctx = nullptr,
};

static const httpd_uri_t settings_uri = {
	.uri = "/settings",
	.method = HTTP_POST,
	.handler = settings_handler,
	.user_ctx = nullptr,
};

// static const httpd_uri_t put_uri = {
// 	.uri = "/put",
// 	.method = HTTP_PUT,
// 	.handler = put_handler,
// 	.user_ctx = nullptr,
// };

// static const httpd_uri_t gpio_uri = {
// 	.uri = "/gpio",
// 	.method = HTTP_GET,
// 	.handler = gpio_handler,
// 	.user_ctx = nullptr,
// };

//

httpd_handle_t start_webserver()
{

	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	config.task_priority = HTTP_PRT;
	config.stack_size = HTTP_MEM;
	config.core_id = CPU0;
	// config.max_open_sockets = 1;

	// Start the httpd server
	ESP_LOGI(TAG, "Starting server on port: %d", config.server_port);

	httpd_handle_t server = nullptr;
	if (httpd_start(&server, &config) == ESP_OK)
	{
		// Set URI handlers
		ESP_LOGI(TAG, "Registering URI handlers");
		httpd_register_uri_handler(server, &welcome_uri);
		httpd_register_uri_handler(server, &io_uri);
		httpd_register_uri_handler(server, &settings_uri);
		return server;
	}

	ESP_LOGI(TAG, "Error starting server!");
	return nullptr;
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