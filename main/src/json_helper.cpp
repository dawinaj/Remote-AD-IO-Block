#include "json_helper.h"

static const char *TAG = "JsonHelp";

json create_empty_response()
{
	json doc = json::object();
	doc["status"] = "";
	doc["message"] = "";
	doc["data"] = json::object();
	return doc;
}

json create_ok_response()
{
	json doc = create_empty_response();
	doc["status"] = "ok";
	doc["message"] = "Ok";
	return doc;
}

json create_err_response(int code, const std::string &msg)
{
	json doc = create_empty_response();
	doc["status"] = "error";
	doc["message"] = "Error occured: ${data.errmsg} Code: ${data.errcode}";
	doc["data"]["errmsg"] = msg;
	doc["data"]["errcode"] = code;
	return doc;
}

//

const char CMPLDATE[] = __DATE__;
const char CMPLTIME[] = __TIME__;
const char CMPLVRSN[] = __VERSION__;

json create_welcome_response()
{
	json doc = create_ok_response();

	doc["message"] = "Welcome!\n"
					 "Last compilation time: ${data.cmpldate} ${data.cmpltime}.\n"
					 "Compiler version: ${data.cmplver}.";

	doc["data"]["cmpldate"] = CMPLDATE;
	doc["data"]["cmpltime"] = CMPLTIME;
	doc["data"]["cmplvrsn"] = CMPLVRSN;
	return doc;
}
