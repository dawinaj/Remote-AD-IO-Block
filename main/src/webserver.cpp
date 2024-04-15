#include "webserver.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <map>
#include <memory> // move
#include <type_traits>
using namespace std::literals;

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

//

static const char *TAG = "WebServer";

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

class SocketReader
{
	static constexpr int BUF_SIZE = 1024;

public:
	httpd_req_t *req;
	esp_err_t err = ESP_OK;

private:
	char buffer[BUF_SIZE];
	size_t dataLen = 0;
	size_t ptr = 0;
	// size_t totalOut = 0;
	// size_t totalIn = 0;

	void recv()
	{
		int ret = httpd_req_recv(req, buffer, BUF_SIZE);

		if (ret < 0) // error
		{
			err = ret;
			return;
		}
		dataLen = ret;
		ptr = 0;
		// totalOut = totalIn;
		// totalIn += dataLen;
	}

public:
	SocketReader(httpd_req_t *r) : req(r) {}
	~SocketReader() = default;

	class iterator
	{
		friend class SocketReader;

	public:
		using iterator_category = std::input_iterator_tag;
		using value_type = char;
		using difference_type = std::ptrdiff_t;
		using pointer = char *;
		using reference = char &;

	private:
		SocketReader *parent;

		iterator(SocketReader *p) : parent(p) {}

	public:
		~iterator() = default;

	public:
		char operator*() const
		{
			if (parent->ptr >= parent->dataLen) [[unlikely]]
				return '\0';
			return parent->buffer[parent->ptr];
		}

		iterator &operator++()
		{
			if (parent->ptr >= parent->dataLen)
				parent->recv();
			return *this;
		}

		// iterator operator++(int) // does not exist, iterator pos is inside the parent
		// {
		// 	Iterator tmp = *this;
		// 	++(*this);
		// 	return tmp;
		// }

		bool operator==(const iterator &other) const
		{
			if (parent == other.parent) // If the same owner, then the same
				return true;

			if (other.parent == nullptr) // I am begin, other is end
			{
				if (parent->ptr >= parent->dataLen)
					return true;
			}
			if (parent == nullptr) // I am end (who TF uses this order?)
			{
				if (other.parent->ptr >= other.parent->dataLen)
					return true;
			}

			return false; // Completely different owners
		}

		// bool operator!=(const Iterator &other) const
		// {
		// 	return !(*this == other);
		// }
	};

public:
	iterator begin()
	{
		dataLen = 0;
		ptr = 0;
		return iterator(this);
	}

	iterator end()
	{
		return iterator(nullptr);
	}
};

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
					 "Compiler version: ${data.cmpl.ver}.\n"
					 "Go to ${data.url.sett} with POST JSON to write settings.\n"
					 "Go to ${data.url.meas} to GET measured stuff.\n"
					 "Settings JSON is an object with two keys:\n"
					 "\t- \"program\" is a string, made of semicolon-separated statements (commands with arguments)\n"
					 "\t- \"generators\" is an array of waveform generator objects\n"
					 "List of existing commands: ${data.prg.cmds}.\n"
					 "List of existing generators: ${data.prg.gnrtrs}.\n";

	doc["data"]["cmpl"]["date"] = __DATE__;
	doc["data"]["cmpl"]["time"] = __TIME__;
	doc["data"]["cmpl"]["vrsn"] = __VERSION__;

	doc["data"]["url"]["sett"] = "/settings";
	doc["data"]["url"]["meas"] = "/io";

	doc["data"]["prg"]["cmds"] = ordered_json::array();

	for (const auto &lut : CS_LUT)
	{
		auto cmd = ordered_json::object();
		cmd["sntx"] = ""s + lut.namestr + ' ' + lut.argstr;
		cmd["desc"] = lut.descstr;
		doc["data"]["prg"]["cmds"].push_back(cmd);
	}

	std::string out = doc.dump();

	return httpd_resp_send(req, out.c_str(), out.length());
}

//

static esp_err_t settings_handler(httpd_req_t *req)
{
	// Make sure that the producer is *not* running
	if (Communicator::check_if_running())
		return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Device is busy");

	ESP_LOGV(TAG, "Req len: %i" PRIu32, req->content_len);

	// std::string post(req->content_len, '\0'); // +- 1?
	// int ret = httpd_req_recv(req, post.data(), req->content_len);
	// if (ret <= 0) // failed to read
	// {
	// 	if (ret == HTTPD_SOCK_ERR_TIMEOUT)
	// 		httpd_resp_send_408(req);
	// 	return ESP_FAIL;
	// }
	// ESP_LOGV(TAG, "%s", post.c_str());

	Interpreter::Program program;
	std::vector<Generator> generators;
	std::vector<std::string> errors;

	//	if (str_is_ascii(post))
	//	{
	SocketReader reader(req);

	json q = json::parse(reader.begin(), reader.end(), nullptr, false, true);
	//	post.clear();

	if (reader.err) // failed to read
	{
		if (reader.err == HTTPD_SOCK_ERR_TIMEOUT)
			httpd_resp_send_408(req);
		return reader.err;
	}

	if (!q.is_discarded())
	{
		//*/
		if (q.contains("generators") && q.at("generators").is_array())
		{
			// parse generators
			size_t count = q.at("generators").size();
			generators.reserve(count);

			for (auto &[key, val] : q.at("generators").items())
			{
				try
				{
					Generator g = val.get<Generator>();
					generators.push_back(std::move(g));
				}
				catch (json::exception &e)
				{
					generators.emplace_back();
					errors.push_back("Generator #"s + key + " failed to parse!");
					ESP_LOGE(TAG, "%s", e.what());
				}
				val = nullptr;
			}
			q.erase("generators");
		}
		//*/
		//*/
		if (q.contains("program") && q.at("program").is_string())
		{
			// parse program
			const std::string &prg = q.at("program").get_ref<const json::string_t &>();
			program.parse(prg, errors);
			q.erase("program");
		}
		//*/
	}
	else
		errors.push_back("JSON is invalid!");

	q.clear();

	if (Board::move_config(program, generators) != ESP_OK)
		return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Device is busy");

	//

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

	return httpd_resp_send(req, out.c_str(), out.length());
}

//

static esp_err_t io_handler(httpd_req_t *req)
{
	// Make sure that the producer is *not* running
	if (Communicator::check_if_running())
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

	// Start producer
	ESP_LOGI(TAG, "Notifying producer...");
	Communicator::start_running();

	// Start consumer
	ESP_LOGI(TAG, "Running consumer...");
	httpd_resp_set_type(req, "application/octet-stream");
	while (true)
	{
		vTaskDelay(1);
		auto rsvd = Communicator::get_read();

		if (/*rsvd.data() == nullptr ||*/ rsvd.size() == 0)
		{
			if (Communicator::check_if_running())
				continue;
			else
				break;
		}

		ESP_LOGI(TAG, "Sending %zu bytes...", rsvd.size());
		esp_err_t ret = httpd_resp_send_chunk(req, rsvd.data(), rsvd.size());

		Communicator::commit_read();

		if (ret != ESP_OK)
		{
			Communicator::ask_for_exit();
			return ret;
		}
	}

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
