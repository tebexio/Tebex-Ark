#pragma once
#include <API/ARK/Ark.h>
#include "json.hpp"
#include "TebexOfflineCommands.h"
#include "TebexOnlineCommands.h"


class TebexForcecheck
{
private:
	static uint64 strToSteamID(std::string steamId);
public:
	static void Call(TebexArk *plugin);
	static void ApiCallback(TebexArk *plugin, TSharedRef<IHttpRequest> request);
};

uint64 TebexForcecheck::strToSteamID(std::string steamId) {
	uint64 result = 0;
	char const* p = steamId.c_str();
	char const* q = p + steamId.size();

	while (p < q) {
		result *= 10;
		result += *(p++) - '0';
	}
	return result;
}

void TebexForcecheck::Call(TebexArk *plugin)
{
	TSharedRef<IHttpRequest> request;

	FHttpModule::Get()->CreateRequest(&request);
	request->SetHeader(&FString("X-Buycraft-Secret"), &FString(plugin->getConfig().secret));
	request->SetURL(&FString(plugin->getConfig().baseUrl + "/queue"));
	request->SetVerb(&FString("GET"));
	request->SetHeader(&FString("X-Buycraft-Handler"), &FString("TebexForcecheck"));
	request->ProcessRequest();

	plugin->addRequestToQueue(request);
	

}

void TebexForcecheck::ApiCallback(TebexArk *plugin, TSharedRef<IHttpRequest> request) {

	FString *Response = &FString();
	request->ResponseField()->GetContentAsString(Response);
	std::string responseText = Response->ToString();

	auto json = nlohmann::json::parse(responseText);

	if (!json["error_message"].is_null()) {
		plugin->logError(FString(json["error_message"].get<std::string>()));
	}
	else {
		if (!json["meta"]["next_check"].is_null()) {
			plugin->setNextCheck(json["meta"]["next_check"].get<int>());
		}

		plugin->setNextCheck(45);
		if (!json["meta"]["execute_offline"].is_null() && json["meta"]["execute_offline"].get<bool>()) {
			plugin->logWarning("Do offline commands");
			TebexOfflineCommands::Call(plugin);
		}

		if (!json["players"].is_null() && json["players"].size() > 0) {
			plugin->logWarning("Process player commands....");
			int playerCnt = 0;
			while (playerCnt < json["players"].size()) {
				auto player = json["players"][playerCnt];

				uint64 steamId64 = TebexForcecheck::strToSteamID(player["uuid"].get<std::string>());

				AShooterPlayerController *targetPlayer = ArkApi::GetApiUtils().FindPlayerFromSteamId(steamId64);
				if (targetPlayer != nullptr) {
					plugin->logWarning(FString::Format("Process commands for {0}", player["name"].get<std::string>()));
					TebexOnlineCommands::Call(plugin, player["id"].get<int>(), player["uuid"].get<std::string>());
				}
				else {
					plugin->logWarning(FString::Format("{0} is not online", player["name"].get<std::string>()));
				}
				playerCnt++;
			}
		}
	}

}

