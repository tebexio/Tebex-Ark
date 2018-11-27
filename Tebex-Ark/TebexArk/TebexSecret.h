#pragma once
#include <API/ARK/Ark.h>
#include "TebexArk.h"
#include "json.hpp"
#include "WebstoreInfo.cpp"

using json = nlohmann::json;

class TebexSecret
{
private:

public:
	static void Call(TebexArk *plugin, FString command);
	static void ApiCallback(TebexArk *plugin, TSharedRef<IHttpRequest> request);
};


void TebexSecret::Call(TebexArk *plugin, FString cmd)
{
	TSharedRef<IHttpRequest> request;
	TArray<FString> parsed;
	cmd.ParseIntoArray(parsed, L" ", true);

	if (!parsed.IsValidIndex(1)) {
		plugin->logError("You must provide a secret for this command");
		return;
	}	

	const FString secret = *parsed[1];

	plugin->setConfig("secret", secret.ToString());

	FHttpModule::Get()->CreateRequest(&request);
	request->SetHeader(&FString("X-Buycraft-Secret"), &FString(plugin->getConfig().secret));
	request->SetURL(&FString(plugin->getConfig().baseUrl + "/information"));
	request->SetVerb(&FString("GET"));
	request->SetHeader(&FString("X-Buycraft-Handler"), &FString("TebexSecret"));
	request->ProcessRequest();

	plugin->addRequestToQueue(request);

}

void TebexSecret::ApiCallback(TebexArk *plugin, TSharedRef<IHttpRequest> request) {

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
	}
	else {
		plugin->logWarning(FString::Format(L"Your secret key has been validated! Webstore Name: {0}", json["account"]["name"].get<std::string>()));
		plugin->setWebstore(json);
	}

}

