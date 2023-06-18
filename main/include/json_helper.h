#pragma once

#include "nlohmann/json.hpp"
using namespace nlohmann;

//

json create_empty_response();

json create_ok_response();
json create_err_response(int = 0, const std::string & = "");

json create_welcome_response();
