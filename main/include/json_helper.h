#pragma once

#include "nlohmann/json.hpp"
using namespace nlohmann;

//

ordered_json create_empty_response();

ordered_json create_ok_response();
ordered_json create_err_response(const std::vector<std::string> &);

ordered_json create_welcome_response();
