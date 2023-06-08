#pragma once
#include "settings.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include "json_helper.h"

//

const int YES = 1;
const int NOO = 0;

enum sockdoc_err_t
{
	ALL_OK = 1,
	DISCONNECT = 0,
	EXCEPTION = ~0,
	UNKN_ERR = ~1,
};

//

int sock_get_error(const int);

int sock_has_data(const int);

ssize_t sendall(int, const void *, size_t, int = 0);

int send_document_to_socket(int, const rapidjson::Document &);

int get_document_from_socket(int, rapidjson::Document &);