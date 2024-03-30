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

#include "to_integer.h"

#include "rigtorp/SPSCQueue.h"
using namespace rigtorp;

#include "Interpreter.h"
using namespace Interpreter;
#include "Communicator.h"

#include "json_helper.h"

//

static const char *TAG = "WebServer";

// NLOHMANN_JSON_SERIALIZE_ENUM(AnIn_Range,
// 							 {
// 								 {AnIn_Range::OFF, "off"},
// 								 {AnIn_Range::Min, "min"},
// 								 {AnIn_Range::Med, "med"},
// 								 {AnIn_Range::Max, "max"},
// 							 })

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

	const json q = json::parse(reader.begin(), reader.end(), nullptr, false, true);
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
			// count = std::min(count, (size_t)16);
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

	Board::move_config(program, generators);

	//

	httpd_resp_set_type(req, "application/json");

	if (!errors.empty())
	{
		httpd_resp_set_status(req, HTTPD_400);

		ordered_json res = create_err_response(errors);
		errors.clear();
		std::string out = res.dump();

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
	auto qr = parse_query(req);

	size_t time_bytes = 0;
	if (auto it = qr.find("tb"); it != qr.end())
		try_parse_integer(it->second, time_bytes);

	if (time_bytes > 8)
		time_bytes = 8;

	// Make sure that the producer is *not* running
	Communicator::producer_running.wait(true, std::memory_order::relaxed);

	// Apply changes
	ESP_LOGI(TAG, "Preparing Communicator...");
	Communicator::time_settings(time_bytes);
	Communicator::cleanup();

	// Start producer
	ESP_LOGI(TAG, "Notifying producer...");
	Communicator::producer_running.store(true, std::memory_order::relaxed);
	Communicator::producer_running.notify_one();

	// Start consumer
	ESP_LOGI(TAG, "Running consumer...");
	httpd_resp_set_type(req, "application/octet-stream");
	while (1)
	{
		vTaskDelay(1);
		auto rsvd = Communicator::get_read();

		if (/*rsvd.data() == nullptr ||*/ rsvd.size() == 0)
		{
			if (Communicator::producer_running.load(std::memory_order::relaxed))
				continue;
			else
				break;
		}

		ESP_LOGI(TAG, "Sending %zu bytes...", rsvd.size());
		esp_err_t ret = httpd_resp_send_chunk(req, rsvd.data(), rsvd.size());

		Communicator::commit_read();

		if (ret != ESP_OK)
		{
			Communicator::please_exit.store(true, std::memory_order::relaxed);
			return ret;
		}
	}

	return httpd_resp_send_chunk(req, nullptr, 0);
}

//

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

//

static httpd_handle_t server = nullptr;

esp_err_t start_webserver()
{
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	config.task_priority = HTTP_PRT;
	config.stack_size = HTTP_MEM;
	config.core_id = CPU0;
	config.max_open_sockets = 1; // 3 for internal, 1 for external
	config.max_uri_handlers = 3;

	config.keep_alive_enable = true;
	config.keep_alive_idle = 5;
	config.keep_alive_interval = 5;
	config.keep_alive_count = 3;

	// Start the httpd server
	ESP_LOGI(TAG, "Starting server on port: %d", config.server_port);

	esp_err_t ret = httpd_start(&server, &config);

	if (ret != ESP_OK)
	{
		ESP_LOGI(TAG, "Error starting server!");
		return ret;
	}

	ESP_LOGI(TAG, "Registering URI handlers");
	httpd_register_uri_handler(server, &welcome_uri);
	httpd_register_uri_handler(server, &io_uri);
	httpd_register_uri_handler(server, &settings_uri);

	return ESP_OK;
}

esp_err_t stop_webserver()
{
	return httpd_stop(server);
}
