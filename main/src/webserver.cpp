#include "settings.h"

#include <string>
#include <map>
#include <memory> // move
#include <type_traits>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>

#include <esp_http_server.h>

#include "json_helper.h"

//

static const char *TAG = "WebServer";

//
/*/
std::string get_query(httpd_req_t *r)
{
	// size_t qr_len = httpd_req_get_url_query_len(r) + 1;
	// std::string qr(qr_len, '\0');
	// httpd_req_get_url_query_str(r, qr.data(), qr_len);
	// qr.erase(qr.find('\0'));
	size_t qr_len = httpd_req_get_url_query_len(r);
	std::string qr(qr_len, '\0');
	httpd_req_get_url_query_str(r, qr.data(), qr_len + 1);
	return qr;
}

auto parse_query(httpd_req_t *r)
{
	std::map<std::string, std::string> ret;
	size_t qr_len = httpd_req_get_url_query_len(r) + 1;
	std::string query(qr_len, '\0');
	httpd_req_get_url_query_str(r, query.data(), qr_len);
	query.erase(query.find('\0'));

	size_t pos = 0;
	do
	{
		size_t maxlen = query.find('&', pos); // find end of current param
		if (maxlen == std::string::npos)
			maxlen = query.length();

		size_t middle = query.find('=', pos); // find break of current param

		if (middle > maxlen) // no value, key only
		{
			std::string key = query.substr(pos, maxlen - pos);
			ret.emplace(std::move(key), "");
		}
		else // key and value
		{
			std::string key = query.substr(pos, middle - pos);
			std::string val = query.substr(middle + 1, maxlen - middle - 1);
			ret.emplace(std::move(key), std::move(val));
		}
		pos = maxlen + 1;
	} //
	while (pos <= query.length());

	// ret.erase("");

	return ret;
}

template <typename T, std::enable_if_t<std::is_unsigned<T>::value, bool> = true>
void string_to_uint(const std::string &str, T &ref)
{
	char *p;
	unsigned long long conv = std::strtoull(str.c_str(), &p, 10);
	if (*p == '\0')
		ref = conv;
}
//*/

void send_measurements(const int16_t *buf, size_t len, void *ctx)
{
	 httpd_req_t *req = static_cast<httpd_req_t *>(ctx);

	const char *str = reinterpret_cast<const char *>(buf);
	len *= sizeof(int16_t) / sizeof(char);

	httpd_resp_send_chunk(req, str, len);
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

	//

	Board::GeneralCfg general;
	Board::TriggerCfg trigger;
	Board::InputCfg input;
	Board::OutputCfg output;

	std::vector<std::string> errors;

	const json q = json::parse(post, nullptr, false);
	if (!q.is_discarded())
	{
		if (q.contains("general"))
		{
			const json &j = q.at("general");
			if (j.is_object())
			{
				if (j.contains("sample_count") && j.at("sample_count").is_number_unsigned())
					general.sample_count = j.at("sample_count").get<uint32_t>();
				else
					errors.push_back("general.sample_count is invalid!");

				if (j.contains("period_us") && j.at("period_us").is_number_unsigned())
					general.period_us = j.at("period_us").get<uint32_t>();
				else
					errors.push_back("general.period_us is invalid!");
			}
			else
				errors.push_back("general is not an object!");
		}
		else
			errors.push_back("general not given!");

		//

		if (q.contains("trigger"))
		{
			const json &j = q.at("trigger");
			if (j.is_object())
			{
				if (j.contains("port"))
				{
					if (j.at("port").is_string())
					{
						const std::string &str = j.at("port").get_ref<const json::string_t &>();
						if (str.length() == 1)
						{
							if (str[0] >= '1' && str[0] <= '4')
								trigger.port = static_cast<Input>(str[0] - '0');
							else
								errors.push_back("trigger.port is invalid!");
						}
						else
							errors.push_back("trigger.port is invalid!");
					}
					else if (j.at("port").is_number_unsigned())
					{
						size_t prt = j.at("port").get<size_t>();
						if (prt >= 1 && prt <= 4)
							trigger.port = static_cast<Input>(prt);
						else
							errors.push_back("trigger.port is invalid!");
					}
					else
						errors.push_back("trigger.port is invalid!");
				}
				else
					errors.push_back("trigger.port is invalid!");
			}
			else
				errors.push_back("trigger is not an object!");
		}

		//

		if (q.contains("input"))
		{
			const json &j = q.at("input");
			if (j.is_object())
			{
				if (j.contains("port_order") && j.at("port_order").is_string())
				{
					const std::string &str = j.at("port_order").get_ref<const json::string_t &>();
					if (!str.empty())
						for (char c : str)
						{
							if (c >= '1' && c <= '4')
								input.port_order.push_back(static_cast<Input>(c - '0'));
							else
							{
								errors.push_back("input.port_order is invalid!");
								break;
							}
						}
					else
						errors.push_back("input.port_order is invalid!");
				}
				else
					errors.push_back("input.port_order is invalid!");

				if (j.contains("repeats"))
				{
					if (j.at("repeats").is_number_unsigned())
						input.repeats = j.at("period_us").get<uint32_t>();
					else
						errors.push_back("input.repeats is invalid!");
				}

				input.callback = send_measurements;
			}
			else
				errors.push_back("input is not an object!");
		}

		//

		if (q.contains("output"))
		{
			const json &j = q.at("output");
			if (j.is_object())
			{
				if (j.contains("voltage_gen"))
				{
					if (j.at("voltage_gen").is_object())
					{
						try
						{
							output.voltage_gen = j.at("voltage_gen").get<Generator>();
						}
						catch (json::exception &e)
						{
							errors.push_back("output.voltage_gen is invalid!");
						}
					}
					else
						errors.push_back("output.voltage_gen is invalid!");
				}

				if (j.contains("current_gen"))
				{
					if (j.at("current_gen").is_object())
					{
						try
						{
							output.current_gen = j.at("current_gen").get<Generator>();
						}
						catch (json::exception &e)
						{
							errors.push_back("output.voltage_gen is invalid!");
						}
					}
					else
						errors.push_back("output.voltage_gen is invalid!");
				}

				if (j.contains("reverse_order"))
				{
					if (j.at("reverse_order").is_boolean())
						output.reverse_order = j.at("reverse_order").get<bool>();
					else
						errors.push_back("output.reverse_order is invalid!");
				}
			}
			else
				errors.push_back("output is not an object!");
		}
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

	const Board::ExecCfg &exec = board->validate_configs(std::move(general), std::move(trigger), std::move(input), std::move(output));

	httpd_resp_set_status(req, HTTPD_200);
	httpd_resp_set_type(req, "application/json");

	ordered_json res = create_ok_response();
	res["message"] = "Settings have been validated. See execution plan: ${data.exec}.";
	res["data"]["exec"]["do_trigger"] = exec.do_trg;
	res["data"]["exec"]["do_input"] = exec.do_inp;
	res["data"]["exec"]["do_output"] = exec.do_out;
	std::string out = res.dump();
	httpd_resp_send(req, out.c_str(), out.length());

	return ESP_OK;
}

//

static esp_err_t io_handler(httpd_req_t *req)
{
	httpd_resp_set_status(req, HTTPD_200);
	httpd_resp_set_type(req, "application/octet-stream");

	board->execute(req);

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
	config.core_id = CPU1;
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