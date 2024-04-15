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
