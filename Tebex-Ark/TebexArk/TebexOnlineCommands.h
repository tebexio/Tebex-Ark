#pragma once
#include <API/ARK/Ark.h>
#include "json.hpp"
#include "TebexDeleteCommands.h"

using json = nlohmann::json;

class TebexOnlineCommands
{
private:
	static uint64 strToSteamID(std::string steamId);
public:
	static void Call(TebexArk *plugin, int pluginPlayerId, std::string playerId);
	static void ApiCallback(TebexArk *plugin, TSharedRef<IHttpRequest> request);
};

uint64 TebexOnlineCommands::strToSteamID(std::string steamId) {
	uint64 result = 0;
	char const* p = steamId.c_str();
	char const* q = p + steamId.size();

	while (p < q) {
		result *= 10;
		result += *(p++) - '0';
	}
	return result;
}

void TebexOnlineCommands::Call(TebexArk *plugin, int pluginPlayerId, std::string playerId)
{
	TSharedRef<IHttpRequest> request;

	FHttpModule::Get()->CreateRequest(&request);
	request->SetHeader(&FString("X-Buycraft-Secret"), &FString(plugin->getConfig().secret));
	request->SetURL(&FString(plugin->getConfig().baseUrl + "/queue/online-commands/" + FString::Format("{0}", pluginPlayerId)));
	request->SetVerb(&FString("GET"));
	request->SetHeader(&FString("X-Buycraft-Handler"), &FString("TebexOnlineCommands"));
	request->SetHeader(&FString("X-Buycraft-SteamID"), &FString(playerId));
	request->ProcessRequest();

	plugin->addRequestToQueue(request);
}

void TebexOnlineCommands::ApiCallback(TebexArk *plugin, TSharedRef<IHttpRequest> request) {

	int deleteAfter = 5;
	FString *steamId = request->GetHeader(&FString(""), &FString("X-Buycraft-SteamID"));
	uint64 steamId64 = TebexOnlineCommands::strToSteamID(steamId->ToString());
	plugin->logWarning(FString("Execute commands for player") + *steamId);

	AShooterPlayerController *player = ArkApi::GetApiUtils().FindPlayerFromSteamId(steamId64);
	if (player == nullptr) {
		return;
	}

	FString *playerName = player->GetPlayerCharacterName(&FString(""));
	
	FString *Response = &FString();
	request->ResponseField()->GetContentAsString(Response);
	std::string responseText = Response->ToString();

	auto json = json::parse(responseText);

	if (!json["error_message"].is_null()) {
		plugin->logError(FString(json["error_message"].get<std::string>()));
	}
	else {
		auto commands = json["commands"];

		int commandCnt = 0;
		int exCount = 0;
		std::list<int> executedCommands;
		while (commandCnt < commands.size()) {
			auto command = commands[commandCnt];
			FString targetCommand = plugin->buildCommand(command["command"].get<std::string>(), playerName->ToString(), steamId->ToString());

			FString *result = &FString();
			
			plugin->logWarning(FString("Exec ") + targetCommand);
			player->ConsoleCommand(result, &targetCommand, true);
			executedCommands.push_back(command["id"].get<int>());
			exCount++;

			if (exCount % deleteAfter == 0) {
				TebexDeleteCommands::Call(plugin, executedCommands);
				executedCommands.clear();
			}
			
			commandCnt++;
		}

		plugin->logWarning(FString::Format("{0} online commands executed", exCount));
		if (exCount % deleteAfter != 0) {
			TebexDeleteCommands::Call(plugin, executedCommands);
			executedCommands.clear();
		}
	}


}

