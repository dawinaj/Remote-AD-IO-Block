#include "json_helper.h"

// static const char *TAG = "JsonHelp";

ordered_json create_empty_response()
{
	ordered_json doc = json::object();
	doc["status"] = "";
	doc["message"] = "";
	doc["data"] = json::object();
	return doc;
}

ordered_json create_ok_response()
{
	ordered_json doc = create_empty_response();
	doc["status"] = "ok";
	doc["message"] = "Ok";
	return doc;
}

ordered_json create_err_response(const std::vector<std::string> &errs)
{
	ordered_json doc = create_empty_response();
	doc["status"] = "error";
	doc["message"] = "Error(s) occured: ${data.errors}";
	doc["data"]["errors"] = errs;
	return doc;
}

//

// const char CMPLDATE[] = __DATE__;
// const char CMPLTIME[] = __TIME__;
// const char CMPLVRSN[] = __VERSION__;

ordered_json create_welcome_response()
{
	ordered_json doc = create_ok_response();

	doc["message"] = "Welcome!\n"
					 "Last compilation time: ${data.cmpl.date} ${data.cmpl.time}.\n"
					 "Compiler version: ${data.cmpl.ver}.\n"
					 "Go to ${data.url.settings} with appropriate POST JSON to write settings.\n"
					 "Go to ${data.url.io} to measure stuff.";

	doc["data"]["cmpl"]["date"] = __DATE__;
	doc["data"]["cmpl"]["time"] = __TIME__;
	doc["data"]["cmpl"]["vrsn"] = __VERSION__;
	doc["data"]["url"]["settings"] = "/settings";
	doc["data"]["url"]["io"] = "/io";
	return doc;
}
