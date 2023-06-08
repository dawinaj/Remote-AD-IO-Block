
#include "json_helper.h"

#include "rapidjson/document.h"
// #include "rapidjson/writer.h"
// #include "rapidjson/error/en.h"

using namespace rapidjson;

// static const char *TAG = "JsonHelp";

Document &create_empty_response(Document &doc)
{
	Document::AllocatorType &alctr = doc.GetAllocator();
	doc.SetObject();
	doc.AddMember("status", "", alctr);
	doc.AddMember("message", "", alctr);
	doc.AddMember("data", Value(kObjectType), alctr);
	return doc;
}

Document &create_ok_response(Document &doc)
{
	Document::AllocatorType &alctr = doc.GetAllocator();
	create_empty_response(doc);
	doc.AddMember("status", "ok", alctr);
	doc.AddMember("message", "Ok", alctr);
	doc.AddMember("data", Value(kObjectType), alctr);
	return doc;
}

Document &create_err_response(Document &doc)
{
	doc.SetObject();
	Document::AllocatorType &alctr = doc.GetAllocator();
	doc.AddMember("status", "error", alctr);
	doc.AddMember("message", "Error occured: ${data.errmsg} Code: ${data.errcode}", alctr);

	doc.AddMember("data", Value(kObjectType), alctr);

	doc["data"].AddMember("errmsg", "", alctr);
	doc["data"].AddMember("errcode", 0, alctr);
	return doc;
}

Document &create_welcome_response(Document &doc)
{
	doc.SetObject();
	Document::AllocatorType &alctr = doc.GetAllocator();
	doc.AddMember("status", "ok", alctr);
	doc.AddMember("message", "Welcome!\n"
							 "Last compilation time: ${data.cmpldate} ${data.cmpltime}.\n"
							 "Compiler version: ${data.cmplver}.",
				  alctr);

	doc.AddMember("data", Value(kObjectType), alctr);

	doc["data"].AddMember("cmpldate", CMPLDATE, alctr);
	doc["data"].AddMember("cmpltime", CMPLTIME, alctr);
	doc["data"].AddMember("cmplvrsn", CMPLVRSN, alctr);
	return doc;
}
