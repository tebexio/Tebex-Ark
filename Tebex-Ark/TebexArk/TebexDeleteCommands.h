#pragma once
#include <API/ARK/Ark.h>
#include "TebexArk.h"
#include "json.hpp"
#include "WebstoreInfo.cpp"

using json = nlohmann::json;

class TebexDeleteCommands
{
private:
	
public:
	static void Call(TebexArk *plugin, std::list<int> commandIds);
	static void ApiCallback(TebexArk *plugin, TSharedRef<IHttpRequest> request);
};



void TebexDeleteCommands::Call(TebexArk *plugin, std::list<int> commandIds)
{

	FString deleteUrl = FString();

	for (auto const& id : commandIds) {
		deleteUrl += FString::Format("ids[]={0}&", id);
	}

	TSharedRef<IHttpRequest> request;
	FHttpModule::Get()->CreateRequest(&request);
	request->SetHeader(&FString("X-Buycraft-Secret"), &FString(plugin->getConfig().secret));
	request->SetURL(&FString(plugin->getConfig().baseUrl + "/queue?" + deleteUrl));
	request->SetVerb(&FString("DELETE"));
	request->SetHeader(&FString("X-Buycraft-Handler"), &FString("TebexDeleteCommands"));
	request->ProcessRequest();

	plugin->addRequestToQueue(request);
}

void TebexDeleteCommands::ApiCallback(TebexArk *plugin, TSharedRef<IHttpRequest> request) {
	//Do nothing :)
}
