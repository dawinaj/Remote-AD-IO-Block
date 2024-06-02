#pragma once

class SocketReader
{
	static constexpr size_t BUF_SIZE = 1024;
	static constexpr const char *const TAG = "SocketReader";

public:
	httpd_req_t *req;
	esp_err_t err = ESP_OK;

private:
	char buffer[BUF_SIZE];
	size_t dataLen = 0;
	size_t ptr = 0;

	void recv()
	{
		int ret = httpd_req_recv(req, buffer, BUF_SIZE);

		if (ret < 0) // error
		{
			ESP_LOGW(TAG, "Recv error: %s", esp_err_to_name(ret));
			err = ret;
			return;
		}
		dataLen = ret;
		ptr = 0;

		ESP_LOGV(TAG, "Recv: `%.*s`", dataLen, buffer);
	}

public:
	SocketReader(httpd_req_t *r) : req(r)
	{
		ESP_LOGI(TAG, "SocketReader ctor");
	}
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
			assert(parent->ptr < parent->dataLen);
			return parent->buffer[parent->ptr];
		}

		iterator &operator++()
		{
			++parent->ptr;
			if (parent->ptr >= parent->dataLen)
				parent->recv();
			return *this;
		}

		bool operator==(const iterator &other) const
		{
			if (parent == other.parent) // If the same owner, then the same
				return true;

			if (other.parent == nullptr) // I am begin, other is end
				return parent->ptr >= parent->dataLen;

			if (parent == nullptr) // I am end (who TF uses this order?)
				return other.parent->ptr >= other.parent->dataLen;

			return false; // Completely different owners
		}
	};

public:
	iterator begin()
	{
		recv();
		return iterator(this);
	}

	iterator end()
	{
		return iterator(nullptr);
	}
};