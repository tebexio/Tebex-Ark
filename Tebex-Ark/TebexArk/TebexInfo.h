#pragma once
#include <API/ARK/Ark.h>
#include "TebexArk.h"
#include "json.hpp"
#include "WebstoreInfo.cpp"

using json = nlohmann::json;

class TebexInfo
{
private:
		
public:	
	static void Call(TebexArk *plugin);
	static void ApiCallback(TebexArk *plugin, TSharedRef<IHttpRequest> request);
};



void TebexInfo::Call(TebexArk *plugin)
{
	TSharedRef<IHttpRequest> request;
	FHttpModule::Get()->CreateRequest(&request);
	request->SetHeader(&FString("X-Buycraft-Secret"), &FString(plugin->getConfig().secret));
	request->SetURL(&FString(plugin->getConfig().baseUrl + "/information"));
	request->SetVerb(&FString("GET"));
	request->SetHeader(&FString("X-Buycraft-Handler"), &FString("TebexInfo"));
	request->ProcessRequest();

	plugin->addRequestToQueue(request);
}

void TebexInfo::ApiCallback(TebexArk *plugin, TSharedRef<IHttpRequest> request) {

	
	FString *Response = &FString();			
	request->ResponseField()->GetContentAsString(Response);
	std::string responseText = Response->ToString();

	nlohmann::basic_json json = nlohmann::json::parse("{}");
	try {
		json = nlohmann::json::parse(responseText);
	}
	catch (nlohmann::detail::parse_error ex) {
		plugin->logError("Unable to parse JSON");
		return;
	}

	if (!json["error_message"].is_null()) {
		plugin->logError(FString(json["error_message"].get<std::string>()));
	} else {
		plugin->logWarning("Server Information");
		plugin->logWarning("=======================");
		FString buffer;
		buffer += FString::Format(L"Server {0} for webstore {1}", json["server"]["name"].get<std::string>(), json["account"]["name"].get<std::string>());
		plugin->logWarning(buffer);
		buffer = FString("Server prices are in ") + FString(json["account"]["currency"]["iso_4217"].get<std::string>());
		plugin->logWarning(FString::Format(L"Server prices are in {0}", json["account"]["currency"]["iso_4217"].get<std::string>()));
		plugin->logWarning(FString::Format(L"Webstore domain: {0}", json["account"]["domain"].get<std::string>()));

		plugin->setWebstore(json);
	}
		
}

