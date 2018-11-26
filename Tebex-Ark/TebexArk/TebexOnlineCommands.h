#pragma once
#include <API/ARK/Ark.h>
#include "json.hpp"
#include "TebexDeleteCommands.h"

using json = nlohmann::json;

class TebexOnlineCommands
{
private:
	
public:
	static void Call(TebexArk *plugin, int pluginPlayerId, std::string playerId);
	static void ApiCallback(TebexArk *plugin, TSharedRef<IHttpRequest> request);
	static uint64 strToSteamID(std::string steamId);
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

	plugin->logWarning(FString::Format("Check for commands for {0}", playerId));


	FHttpModule::Get()->CreateRequest(&request);
	request->SetHeader(&FString("X-Buycraft-Secret"), &FString(plugin->getConfig().secret));
	request->SetURL(&FString(plugin->getConfig().baseUrl + "/queue/online-commands/" + FString::Format("{0}", pluginPlayerId)));
	request->SetVerb(&FString("GET"));
	request->SetHeader(&FString("X-Buycraft-Handler"), &FString("TebexOnlineCommands"));
	request->ProcessRequest();

	plugin->addRequestToQueue(request);
}

void TebexOnlineCommands::ApiCallback(TebexArk *plugin, TSharedRef<IHttpRequest> request) {

	int deleteAfter = 5;

	//Parse the JSON first to get the steamID
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

	std::string playerId = json["player"]["id"].get<std::string>();

	uint64 steamId64 = TebexOnlineCommands::strToSteamID(playerId);

	AShooterPlayerController *player = ArkApi::GetApiUtils().FindPlayerFromSteamId(steamId64);
	if (player == nullptr) {
		return;
	}
	FString* playerName = new FString();

	player->GetPlayerCharacterName(playerName);

	long long linkedPlayerIdField = player->LinkedPlayerIDField();

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
			FString targetCommand = plugin->buildCommand(command["command"].get<std::string>(), playerName->ToString(), playerId, std::to_string(linkedPlayerIdField));

			FString *result = &FString();
			
			int delay = command["conditions"]["delay"].get<int>();
			if (delay == 0) {
				plugin->logWarning(FString("Exec ") + targetCommand);
				player->ConsoleCommand(result, &targetCommand, true);
			}
			else {
				FString *delayCommand = new FString(targetCommand.ToString());
				std::thread([plugin, &delayCommand, result, delay, steamId64]() {
					Sleep(delay * 1000);
					AShooterPlayerController *player = ArkApi::GetApiUtils().FindPlayerFromSteamId(steamId64);
					if (player != nullptr) {
						FString targetCommand = FString(delayCommand->ToString());
						FString *cmdPtr = &targetCommand;
						plugin->logWarning(FString("Exec ") + targetCommand);
						player->ConsoleCommand(result, cmdPtr, true);
					}
					return false;
				}).detach();
			}
			
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
	delete playerName;

}

