#pragma once

#include "TebexArk.h"

#include "TebexDeleteCommands.h"

using json = nlohmann::json;

class TebexOnlineCommands {
public:
	static void Call(TebexArk* plugin, int pluginPlayerId, std::string playerId);
	static void ApiCallback(TebexArk* plugin, std::string responseText);
};

inline void TebexOnlineCommands::Call(TebexArk* plugin, int pluginPlayerId, std::string playerId) {
	plugin->logWarning(FString::Format("Check for commands for {0}", playerId));

	const std::string url = (plugin->getConfig().baseUrl + "/queue/online-commands/" + FString::Format(
		"{0}", pluginPlayerId)).ToString();
	const std::vector<std::string> headers{
		fmt::format("X-Buycraft-Secret: {}", plugin->getConfig().secret.ToString()),
		"X-Buycraft-Handler: TebexOnlineCommands"
	};

	const bool result = API::Requests::Get().CreateGetRequest(url, [plugin](bool success, std::string response) {
		if (!success) {
			plugin->logError("Unable to process API request");
			return;
		}

		ApiCallback(plugin, response);
	}, move(headers));
	if (!result) {
		plugin->logWarning("Call failed");
	}
}

inline void TebexOnlineCommands::ApiCallback(TebexArk* plugin, std::string responseText) {
	const int deleteAfter = 5;

	nlohmann::basic_json json;
	try {
		json = nlohmann::json::parse(responseText);
	}
	catch (const nlohmann::detail::parse_error&) {
		plugin->logError("Unable to parse JSON");
		return;
	}

	const std::string playerId = json["player"]["id"].get<std::string>();

	uint64 steamId64;
	try {
		steamId64 = std::stoull(playerId);
	}
	catch (const std::exception&) {
		plugin->logError("Parsing error");
		return;
	}

	AShooterPlayerController* player = ArkApi::GetApiUtils().FindPlayerFromSteamId(steamId64);
	if (player == nullptr) {
		return;
	}

	FString playerName;
	player->GetPlayerCharacterName(&playerName);

	const long long linkedPlayerIdField = player->LinkedPlayerIDField();

	if (!json["error_message"].is_null()) {
		plugin->logError(FString(json["error_message"].get<std::string>()));
	}
	else { // Improve
		auto commands = json["commands"];

		int commandCnt = 0;
		int exCount = 0;
		std::list<int> executedCommands;
		while (commandCnt < commands.size()) {
			auto command = commands[commandCnt];
			FString targetCommand = plugin->buildCommand(command["command"].get<std::string>(), playerName.ToString(),
			                                             playerId, std::to_string(linkedPlayerIdField));

			int delay = command["conditions"]["delay"].get<int>();
			if (delay == 0) {
				plugin->logWarning(FString("Exec ") + targetCommand);

				FString result;
				player->ConsoleCommand(&result, &targetCommand, true);
			}
			else {
				FString* delayCommand = new FString(targetCommand.ToString());
				std::thread([plugin, &delayCommand, delay]() {
					FString targetCommand = FString(delayCommand->ToString());
					FString* cmdPtr = &targetCommand;
					Sleep(delay * 1000);
					FString* result = &FString();
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

			commandCnt++;
		}

		plugin->logWarning(FString::Format("{0} online commands executed", exCount));
		if (exCount % deleteAfter != 0) {
			TebexDeleteCommands::Call(plugin, executedCommands);
			executedCommands.clear();
		}
	}
}
