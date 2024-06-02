#include "webserver.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <map>
#include <memory> // move
#include <type_traits>
using namespace std::literals;

#include <sys/socket.h>

#include <esp_event.h>

#include "to_integer.h"

// #include "rigtorp/SPSCQueue.h"
// using namespace rigtorp;

#include "Board.h"
#include "Generator.h"
#include "Interpreter.h"
#include "Communicator.h"
using namespace Interpreter;

#include "json_helper.h"

#include "SocketReader.h"

//

static const char *TAG = "WebServer";

//

// bool str_is_ascii(const std::string &s)
// {
// 	return std::all_of(s.begin(), s.end(),
// 					   [](unsigned char ch)
// 					   { return ch <= 127; });
// }

// void str_to_lower(std::string &s)
// {
// 	std::transform(s.begin(), s.end(), s.begin(),
// 				   [](unsigned char c)
// 				   { return std::tolower(c); });
// }

//

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
		if (middle > maxlen)				  // no value, key only
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

//

static esp_err_t favicon_handler(httpd_req_t *req)
{
	httpd_resp_set_status(req, HTTPD_200);
	httpd_resp_set_type(req, "image/x-icon");
	httpd_resp_set_hdr(req, "Cache-Control", "max-age=31536000, immutable");

	static constexpr char favicon[661] =
		{
			0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x10, 0x10, 0x10, 0x00, 0x01, 0x00, 0x04, 0x00, 0x28, 0x01,
			0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x20, 0x00,
			0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0xF5, 0xB0, 0x67, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00,
			0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x01, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x01,
			0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00,
			0x01, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x01, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
			0x00, 0x00, 0xFF, 0x07, 0x00, 0x00, 0xFE, 0x07, 0x00, 0x00, 0xFC, 0x07, 0x00, 0x00, 0xF8, 0x07,
			0x00, 0x00, 0xF0, 0x07, 0x00, 0x00, 0xE0, 0x07, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x80, 0x01,
			0x00, 0x00, 0xE0, 0x07, 0x00, 0x00, 0xF0, 0x07, 0x00, 0x00, 0xF8, 0x07, 0x00, 0x00, 0xFC, 0x07,
			0x00, 0x00, 0xFE, 0x07, 0x00, 0x00, 0xFF, 0x07, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x0D, 0x0A,
			0x00, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D,
			0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x31, 0x39, 0x38,
			0x37, 0x35, 0x39, 0x38, 0x34, 0x36, 0x39, 0x33, 0x36, 0x36, 0x34, 0x34, 0x31, 0x32, 0x39, 0x38,
			0x35, 0x33, 0x37, 0x30, 0x36, 0x39, 0x36, 0x34, 0x34, 0x36, 0x0D, 0x0A, 0x43, 0x6F, 0x6E, 0x74,
			0x65, 0x6E, 0x74, 0x2D, 0x44, 0x69, 0x73, 0x70, 0x6F, 0x73, 0x69, 0x74, 0x69, 0x6F, 0x6E, 0x3A,
			0x20, 0x66, 0x6F, 0x72, 0x6D, 0x2D, 0x64, 0x61, 0x74, 0x61, 0x3B, 0x20, 0x6E, 0x61, 0x6D, 0x65,
			0x3D, 0x22, 0x63, 0x6D, 0x64, 0x00, 0x0D, 0x0A, 0x0D, 0x0A, 0x43, 0x6F, 0x6E, 0x76, 0x65, 0x72,
			0x74, 0x00, 0x00, 0x00, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D,
			0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D,
			0x31, 0x39, 0x38, 0x37, 0x35, 0x39, 0x38, 0x34, 0x36, 0x39, 0x33, 0x36, 0x36, 0x34, 0x34, 0x31,
			0x32, 0x39, 0x38, 0x35, 0x33, 0x37, 0x30, 0x36, 0x39, 0x36, 0x34, 0x34, 0x36, 0x2D, 0x2D, 0x0D,
			0x0A, 0x00, 0x00, 0x00, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x43, 0x6F, 0x6E, 0x74,
			0x65, 0x6E, 0x74, 0x2D, 0x74, 0x79, 0x70, 0x65, 0x3A, 0x20, 0x74, 0x65, 0x78, 0x74, 0x2F, 0x70,
			0x6C, 0x61, 0x69, 0x6E, 0x0D, 0x0A, 0x0D, 0x0A, 0x2F, 0x2F, 0x20, 0x42, 0x4C, 0x4F, 0x42, 0x49,
			0x4E, 0x41, 0x54, 0x49, 0x4F, 0x52, 0x3A, 0x20, 0x66, 0x61, 0x76, 0x69, 0x63, 0x6F, 0x6E, 0x28,
			0x33, 0x29, 0x2E, 0x69, 0x63, 0x6F, 0x0A, 0x0A, 0x75, 0x6E, 0x73, 0x69, 0x67, 0x6E, 0x65, 0x64,
			0x20, 0x63, 0x68, 0x61, 0x72, 0x20, 0x66, 0x61, 0x76, 0x69, 0x63, 0x6F, 0x6E, 0x28, 0x33, 0x29,
			0x5B, 0x36, 0x36, 0x31, 0x5D, 0x20, 0x3D, 0x0A, 0x7B, 0x0A, 0x20, 0x20, 0x20, 0x30, 0x78, 0x30,
			0x30, 0x2C, 0x30, 0x78, 0x30, 0x30, 0x2C, 0x30, 0x78, 0x30, 0x31, 0x2C, 0x30, 0x78, 0x30, 0x30,
			0x2C, 0x30, 0x78, 0x30, 0x31, 0x2C, 0x30, 0x78, 0x30, 0x30, 0x2C, 0x30, 0x78, 0x31, 0x30, 0x2C,
			0x30, 0x78, 0x31, 0x30, 0x2C, 0x30, 0x78, 0x31, 0x30, 0x2C, 0x30, 0x78, 0x30, 0x30, 0x2C, 0x30,
			0x78, 0x30, 0x31, 0x2C, 0x30};

	return httpd_resp_send(req, favicon, 661);
}

static esp_err_t welcome_handler(httpd_req_t *req)
{
	httpd_resp_set_status(req, HTTPD_200);
	httpd_resp_set_type(req, "application/json");

	ordered_json doc = create_ok_response();

	doc["message"] = "Welcome!\n"
					 "Last compilation time: ${data.cmpl.date} ${data.cmpl.time}.\n"
					 "Compiler version: ${data.cmpl.vrsn}.\n"
					 "ESP-IDF version: ${data.cmpl.idfv}.\n"
					 "Go to ${data.url.sett} with POST JSON to write settings.\n"
					 "Go to ${data.url.meas} to GET measured stuff.\n"
					 "Settings JSON is an object with two keys:\n"
					 "\t- \"task\" is a string, made of semicolon-separated statements (commands with arguments)\n"
					 "\t- \"generators\" is an array of amplitudes and waveforms\n"
					 "List of existing commands with syntax and description: ${data.prg.cmds}.\n"
					 "Exemplary generator of a sine signal: ${data.prg.gnrtr}.\n"
					 "List of existing waveforms with syntax: ${data.prg.wvfrms}.\n"
					 "Input ranges, max values and resolution: ${data.ranges}.\n";

	doc["data"]["cmpl"]["date"] = __DATE__;
	doc["data"]["cmpl"]["time"] = __TIME__;
	doc["data"]["cmpl"]["vrsn"] = __VERSION__;
	doc["data"]["cmpl"]["idfv"] = esp_get_idf_version();

	doc["data"]["url"]["sett"] = "/settings";
	doc["data"]["url"]["meas"] = "/io";

	// Commands
	doc["data"]["prg"]["cmds"] = ordered_json::array();
	for (const auto &lut : CS_LUT)
	{
		auto cmd = ordered_json::object();
		cmd["sntx"] = ""s + lut.namestr + ' ' + lut.argstr;
		cmd["desc"] = lut.descstr;
		doc["data"]["prg"]["cmds"].push_back(cmd);
	}

	// Generators
	Generator gen;
	gen.add(1.0f, make_signal<SignalSine>(1000));
	doc["data"]["prg"]["gnrtr"] = static_cast<json>(gen);

	doc["data"]["prg"]["wvfrms"] = ordered_json::array();
	doc["data"]["prg"]["wvfrms"].push_back(SignalType::Const);
	doc["data"]["prg"]["wvfrms"].push_back(SignalType::Impulse);
	doc["data"]["prg"]["wvfrms"].push_back(SignalType::Sine);
	doc["data"]["prg"]["wvfrms"].push_back(SignalType::Square);
	doc["data"]["prg"]["wvfrms"].push_back(SignalType::Triangle);
	doc["data"]["prg"]["wvfrms"].push_back(SignalType::Chirp);

	doc["data"]["prg"]["wvfrms"].push_back(SignalType::Delay);
	doc["data"]["prg"]["wvfrms"].push_back(SignalType::Absolute);
	doc["data"]["prg"]["wvfrms"].push_back(SignalType::Clamp);
	doc["data"]["prg"]["wvfrms"].push_back(SignalType::LinearMap);
	doc["data"]["prg"]["wvfrms"].push_back(SignalType::Multiply);

	// Ranges
	doc["data"]["ranges"] = ordered_json::object();
	doc["data"]["ranges"]["Uin"] = ordered_json::object();
	doc["data"]["ranges"]["Iin"] = ordered_json::object();

	doc["data"]["ranges"]["Uin"]["range"] = ordered_json::object();
	doc["data"]["ranges"]["Uin"]["range"]["MIN"] = Board::u_ref * Board::volt_divs[1];
	doc["data"]["ranges"]["Uin"]["range"]["MED"] = Board::u_ref * Board::volt_divs[2];
	doc["data"]["ranges"]["Uin"]["range"]["MAX"] = Board::u_ref * Board::volt_divs[3];

	doc["data"]["ranges"]["Iin"]["range"] = ordered_json::object();
	doc["data"]["ranges"]["Iin"]["range"]["MIN"] = Board::u_ref / Board::curr_gains[1] * Board::ItoU_input;
	doc["data"]["ranges"]["Iin"]["range"]["MED"] = Board::u_ref / Board::curr_gains[2] * Board::ItoU_input;
	doc["data"]["ranges"]["Iin"]["range"]["MAX"] = Board::u_ref / Board::curr_gains[3] * Board::ItoU_input;

	doc["data"]["ranges"]["Uin"]["resolution"] = ordered_json::object();
	doc["data"]["ranges"]["Uin"]["resolution"]["MIN"] = Board::u_ref * Board::volt_divs[1] * 2 / MCP3204::ref;
	doc["data"]["ranges"]["Uin"]["resolution"]["MED"] = Board::u_ref * Board::volt_divs[2] * 2 / MCP3204::ref;
	doc["data"]["ranges"]["Uin"]["resolution"]["MAX"] = Board::u_ref * Board::volt_divs[3] * 2 / MCP3204::ref;

	doc["data"]["ranges"]["Iin"]["resolution"] = ordered_json::object();
	doc["data"]["ranges"]["Iin"]["resolution"]["MIN"] = Board::u_ref / Board::curr_gains[1] * Board::ItoU_input * 2 / MCP3204::ref;
	doc["data"]["ranges"]["Iin"]["resolution"]["MED"] = Board::u_ref / Board::curr_gains[2] * Board::ItoU_input * 2 / MCP3204::ref;
	doc["data"]["ranges"]["Iin"]["resolution"]["MAX"] = Board::u_ref / Board::curr_gains[3] * Board::ItoU_input * 2 / MCP3204::ref;

	doc["data"]["ranges"]["Uout"] = ordered_json::object();
	doc["data"]["ranges"]["Iout"] = ordered_json::object();

	doc["data"]["ranges"]["Uout"]["range"] = Board::out_ref;
	doc["data"]["ranges"]["Iout"]["range"] = Board::out_ref / Board::UtoI_output;
	doc["data"]["ranges"]["Uout"]["resolution"] = Board::out_ref * 2 / MCP3204::ref;
	doc["data"]["ranges"]["Iout"]["resolution"] = Board::out_ref / Board::UtoI_output * 2 / MCP3204::ref;

	// Godsend
	std::string out = doc.dump();

	ESP_LOGI(TAG, "Handler done.");
	return httpd_resp_send(req, out.c_str(), out.length());
}

//

static esp_err_t settings_handler(httpd_req_t *req)
{
	// Make sure that the producer is *not* running
	if (Communicator::is_running())
		return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Device is busy");

	ESP_LOGV(TAG, "Req len: %" PRIu16, req->content_len);

	Interpreter::Program program;
	std::vector<Generator> generators;
	std::vector<std::string> errors;

	ESP_LOGD(TAG, "Reading JSON...");
	SocketReader reader(req);
	ordered_json q = ordered_json::parse(reader.begin(), reader.end(), nullptr, false, true);

	if (reader.err) // failed to read from socket
	{
		ESP_LOGW(TAG, "Reader error");
		if (reader.err == HTTPD_SOCK_ERR_TIMEOUT)
			httpd_resp_send_408(req);
		return reader.err;
	}

	if (!q.is_discarded()) // if JSON is valid
	{
		if (q.contains("generators"))
		{
			ESP_LOGD(TAG, "Generators exists, trying...");
			if (q.at("generators").is_array())
			{
				size_t count = q.at("generators").size();
				generators.reserve(count);

				for (auto &[key, val] : q.at("generators").items())
				{
					try
					{
						Generator g = val.get<Generator>();
						generators.push_back(std::move(g));
					}
					catch (ordered_json::exception &e)
					{
						generators.emplace_back();
						errors.push_back("Generator #"s + key + " failed to parse: " + e.what());
						ESP_LOGW(TAG, "Generator failed to parse: %s", e.what());
					}
					val = nullptr;
				}
			}
			else
				errors.push_back("\"generators\" is not an array!");
			q.erase("generators");
		}
		if (q.contains("task"))
		{
			ESP_LOGD(TAG, "Task exists, trying...");
			if (q.at("task").is_string())
			{
				const std::string &prg = q.at("task").get_ref<const std::string &>();
				program.parse(prg, errors);
				ESP_LOGD(TAG, "Task has %u statements", program.size());
			}
			else
				errors.push_back("\"task\" is not a string!");
			q.erase("task");
		}
	}
	else
		errors.push_back("JSON is invalid!");

	q.clear();

	ESP_LOGD(TAG, "Moving configs...");
	if (Board::move_config(program, generators) != ESP_OK)
		return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Device is busy");

	//
	ESP_LOGD(TAG, "Responding...");

	httpd_resp_set_type(req, "application/json");

	if (!errors.empty())
	{
		ordered_json res = create_err_response(errors);
		errors.clear();
		std::string out = res.dump();
		res.clear();
		httpd_resp_set_status(req, HTTPD_400);
		return httpd_resp_send(req, out.c_str(), out.length());
	}

	ordered_json res = create_ok_response();
	res["message"] = "Settings have been validated. No errors found.";

	std::string out = res.dump();
	res.clear();

	ESP_LOGI(TAG, "Handler done.");
	return httpd_resp_send(req, out.c_str(), out.length());
}

//

static esp_err_t io_handler(httpd_req_t *req)
{
	// Make sure that the producer is *not* running
	if (Communicator::is_running())
		return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Device is busy");

	auto qr = parse_query(req);

	size_t time_bytes = 0;
	if (auto it = qr.find("tb"); it != qr.end())
		try_parse_integer(it->second, time_bytes);

	if (time_bytes > 8)
		time_bytes = 8;

	// Apply changes
	ESP_LOGI(TAG, "Preparing Communicator...");
	Communicator::time_settings(time_bytes);
	Communicator::cleanup();

	// Start consumer
	ESP_LOGI(TAG, "Running consumer...");
	httpd_resp_set_type(req, "application/octet-stream");

	size_t total_sent = 0;
	esp_err_t ret = ESP_OK;

	// Start producer
	ESP_LOGI(TAG, "Notifying producer...");
	Communicator::start_running();

	while (Communicator::is_running() || Communicator::has_data())
	{
		auto rsvd = Communicator::get_read();

		if (rsvd.size() != 0) // if something to send, send
		{
			ESP_LOGI(TAG, "Sending %zu bytes...", rsvd.size());

			ret = httpd_resp_send_chunk(req, rsvd.data(), rsvd.size());
			total_sent += rsvd.size();

			Communicator::commit_read();
			vTaskDelay(pdMS_TO_TICKS(1));
		}
		else // if no new data
		{
			if (!httpd_req_check_live(req)) // check if dead, ask to stop
			{
				ESP_LOGW(TAG, "Client disconnected...");
				ret = HTTPD_SOCK_ERR_TIMEOUT;
			}
			vTaskDelay(pdMS_TO_TICKS(10));
		}

		if (ret != ESP_OK)
			break;
	}

	Communicator::ask_to_exit();

	while (Communicator::is_running())
		vTaskDelay(pdMS_TO_TICKS(10));

	ESP_LOGW(TAG, "Total sent: %zu bytes...", total_sent);

	if (ret != ESP_OK)
		return ret;

	ESP_LOGI(TAG, "Handler done.");
	return httpd_resp_send_chunk(req, nullptr, 0);
}

//

static constexpr httpd_uri_t favicon_uri = {
	.uri = "/favicon.ico",
	.method = HTTP_GET,
	.handler = favicon_handler,
	.user_ctx = nullptr,
};

static constexpr httpd_uri_t welcome_uri = {
	.uri = "/",
	.method = HTTP_GET,
	.handler = welcome_handler,
	.user_ctx = nullptr,
};

static constexpr httpd_uri_t io_uri = {
	.uri = "/io",
	.method = HTTP_GET,
	.handler = io_handler,
	.user_ctx = nullptr,
};

static constexpr httpd_uri_t settings_uri = {
	.uri = "/settings",
	.method = HTTP_POST,
	.handler = settings_handler,
	.user_ctx = nullptr,
};

//

static httpd_handle_t server = nullptr;

esp_err_t start_webserver()
{
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	config.task_priority = HTTP_PRT;
	config.stack_size = HTTP_MEM;
	config.core_id = CPU0;
	config.max_open_sockets = 1; // 3 for internal, 1 for external
	config.max_uri_handlers = 4;

	config.lru_purge_enable = true;

	config.keep_alive_enable = true;
	config.keep_alive_idle = 5;
	config.keep_alive_interval = 5;
	config.keep_alive_count = 3;

	// Start the httpd server
	ESP_LOGI(TAG, "Starting HTTP server on port: %d", config.server_port);

	ESP_RETURN_ON_ERROR(
		httpd_start(&server, &config),
		TAG, "Failed to httpd_start!");

	ESP_LOGI(TAG, "Registering URI handlers...");

	ESP_RETURN_ON_ERROR(
		httpd_register_uri_handler(server, &welcome_uri),
		TAG, "Failed to httpd_register_uri_handler!");

	ESP_RETURN_ON_ERROR(
		httpd_register_uri_handler(server, &io_uri),
		TAG, "Failed to httpd_register_uri_handler!");

	ESP_RETURN_ON_ERROR(
		httpd_register_uri_handler(server, &settings_uri),
		TAG, "Failed to httpd_register_uri_handler!");

	ESP_RETURN_ON_ERROR(
		httpd_register_uri_handler(server, &favicon_uri),
		TAG, "Failed to httpd_register_uri_handler!");

	return ESP_OK;
}

esp_err_t stop_webserver()
{
	ESP_RETURN_ON_ERROR(
		httpd_stop(server),
		TAG, "Failed to httpd_stop!");

	return ESP_OK;
}

//

bool httpd_req_check_live(httpd_req_t *req)
{
	static char buf;
	static TickType_t last = 0;

	if (req == nullptr)
		return false;

	TickType_t now = xTaskGetTickCount();
	if (now - last < pdMS_TO_TICKS(1000))
		return true;

	last = now;

	int sockfd = httpd_req_to_sockfd(req);

	return httpd_socket_recv(server, sockfd, &buf, 1, MSG_PEEK | MSG_DONTWAIT) == HTTPD_SOCK_ERR_TIMEOUT;
	//	return recv(sockfd, &buf, 1, MSG_PEEK | MSG_DONTWAIT) != 0;
}