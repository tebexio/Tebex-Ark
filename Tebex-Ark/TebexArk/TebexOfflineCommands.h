#pragma once
#include <API/ARK/Ark.h>
#include "json.hpp"
#include "TebexDeleteCommands.h"

using json = nlohmann::json;

class TebexOfflineCommands
{
private:

public:
	static void Call(TebexArk *plugin);
	static void ApiCallback(TebexArk *plugin, TSharedRef<IHttpRequest> request);
};


void TebexOfflineCommands::Call(TebexArk *plugin)
{	
	TSharedRef<IHttpRequest> request;

	FHttpModule::Get()->CreateRequest(&request);
	request->SetHeader(&FString("X-Buycraft-Secret"), &FString(plugin->getConfig().secret));
	request->SetURL(&FString(plugin->getConfig().baseUrl + "/queue/offline-commands"));
	request->SetVerb(&FString("GET"));
	request->SetHeader(&FString("X-Buycraft-Handler"), &FString("TebexOfflineCommands"));
	request->ProcessRequest();

	plugin->addRequestToQueue(request);
}

void TebexOfflineCommands::ApiCallback(TebexArk *plugin, TSharedRef<IHttpRequest> request) {

	int deleteAfter = 5;

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
		auto commands = json["commands"];
		
		int commandCnt = 0;
		int exCount = 0;
		std::list<int> executedCommands;
		while (commandCnt < commands.size()) {
			auto command = commands[commandCnt];
			FString targetCommand = plugin->buildCommand(command["command"].get<std::string>(), command["player"]["name"].get<std::string>(), command["player"]["uuid"].get<std::string>(), "");
			

			APlayerController* FirstPlayer = ArkApi::GetApiUtils().GetWorld()->GetFirstPlayerController();
			FString *result = &FString();

			if (FirstPlayer != nullptr) {

				int delay = command["conditions"]["delay"].get<int>();
				if (delay == 0) {
					plugin->logWarning(FString("Exec ") + targetCommand);
					FirstPlayer->ConsoleCommand(result, &targetCommand, true);
				}
				else {
					FString *delayCommand = new FString(targetCommand.ToString());
					std::thread([plugin, &delayCommand, delay]() {						
						FString targetCommand = FString(delayCommand->ToString());
						FString *cmdPtr = &targetCommand;
						Sleep(delay * 1000);
						FString *result = &FString();
						APlayerController* FirstPlayer = ArkApi::GetApiUtils().GetWorld()->GetFirstPlayerController();
						if (FirstPlayer != nullptr) {								
							plugin->logWarning(FString("Exec ") + targetCommand);							
							FirstPlayer->ConsoleCommand(result, cmdPtr, true);
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

			}
			else {
				plugin->logWarning("No player available to target commands");
			}
			
			commandCnt++;
		}

		plugin->logWarning(FString::Format("{0} offline commands executed", exCount));
		if (exCount % deleteAfter != 0) {
			TebexDeleteCommands::Call(plugin, executedCommands);
			executedCommands.clear();
		}
	}
	
		
}

