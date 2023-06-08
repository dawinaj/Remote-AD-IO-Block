#pragma once

#include "rapidjson/document.h"

//

rapidjson::Document &create_empty_response(rapidjson::Document &);

rapidjson::Document &create_ok_response(rapidjson::Document &);
rapidjson::Document &create_err_response(rapidjson::Document &);

rapidjson::Document &create_welcome_response(rapidjson::Document &);

const char CMPLDATE[] = __DATE__;
const char CMPLTIME[] = __TIME__;
const char CMPLVRSN[] = __VERSION__;
