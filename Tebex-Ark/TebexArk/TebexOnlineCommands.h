#pragma once

#include "TebexArk.h"

#include <Timer.h>

#include "TebexDeleteCommands.h"

using json = nlohmann::json;

class TebexOnlineCommands {
public:
	static void Call(TebexArk* plugin, int pluginPlayerId, std::string playerId);
	static void ApiCallback(TebexArk* plugin, std::string responseText);
};

inline void TebexOnlineCommands::Call(TebexArk* plugin, int pluginPlayerId, std::string playerId) {
	plugin->logWarning(FString::Format("Check for commands for {0} {1}", playerId, pluginPlayerId));

	const std::string url = (plugin->getConfig().baseUrl + "/queue/online-commands/" + FString::Format(
		"{0}", pluginPlayerId)).ToString();
	std::vector<std::string> headers{
		fmt::format("X-Buycraft-Secret: {}", plugin->getConfig().secret.ToString()),
		"X-Buycraft-Handler: TebexOnlineCommands"
	};

	const bool result = API::Requests::Get().CreateGetRequest(url, [plugin](bool success, std::string response) {
		if (!success) {
			plugin->logWarning("Unable to process API request");
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

	plugin->logWarning("Checking online commands..");

	nlohmann::basic_json json;
	try {
		json = nlohmann::json::parse(responseText);
	}
	catch (const nlohmann::detail::parse_error&) {
		plugin->logError("Unable to parse JSON");
		return;
	}

	if (json.find("player") == json.end()) {
		plugin->logWarning("TebexOnlineCommands: Json is invalid");
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
	if (player == nullptr || ArkApi::IApiUtils::IsPlayerDead(player)) {
		return;
	}

	FString playerName;
	player->GetPlayerCharacterName(&playerName);

	const long long linkedPlayerIdField = player->LinkedPlayerIDField();

	if (!json["error_message"].is_null()) {
		plugin->logError(FString(json["error_message"].get<std::string>()));
	}
	else {
		int exCount = 0;
		std::list<int> executedCommands;

		for (const auto& command : json["commands"]) {
			const int commandId = command["id"];

			// Check if command already executed
			if (executedCommandsId.find(commandId) != executedCommandsId.end())
				continue;

			// Check slots

			const int slots = command["conditions"].value("slots", 0);
			if (slots > 0 && player->GetPlayerCharacter() && player->GetPlayerCharacter()->MyInventoryComponentField()) {
				UPrimalInventoryComponent* inv = player->GetPlayerCharacter()->MyInventoryComponentField();

				const int itemsAmount = inv->InventoryItemsField().Num();
				const int maxItems = inv->AbsoluteMaxInventoryItemsField();

				if (maxItems - itemsAmount < slots) {
					continue;
				}
			}

			FString targetCommand = plugin->buildCommand(command["command"].get<std::string>(), playerName.ToString(),
			                                             playerId, std::to_string(linkedPlayerIdField));

			const int delay = command["conditions"].value("delay", 0);

			bool result = false;

			if (delay == 0) {
				plugin->logWarning(FString("Exec ") + targetCommand);
				if (player != nullptr) {
					result = plugin->ConsoleCommand(player, targetCommand, true);
				}
			}
			else {
				API::Timer::Get().DelayExecute([plugin, steamId64, targetCommand, commandId]() {
					// Check if command already executed
					if (executedCommandsId.find(commandId) != executedCommandsId.end())
						return;

					AShooterPlayerController* player = ArkApi::GetApiUtils().FindPlayerFromSteamId(steamId64);
					if (player != nullptr) {
						plugin->logWarning(FString("Exec ") + targetCommand);

						const bool result = plugin->ConsoleCommand(player, targetCommand, true);
						if (result) {
							TebexDeleteCommands::Call(plugin, { commandId });

							plugin->logWarning("Done");
						}
						else {
							plugin->logWarning("Execution wasn't successful");
						}
					}
				}, delay);
			}

			if (!result) {
				exCount++;
				continue;
			}

			executedCommands.push_back(commandId);
			exCount++;

			if (exCount % deleteAfter == 0) {
				TebexDeleteCommands::Call(plugin, executedCommands);
				executedCommands.clear();
			}
		}

		plugin->logWarning(FString::Format("{0} online commands executed", exCount));
		
		if (!executedCommands.empty()) {
			TebexDeleteCommands::Call(plugin, executedCommands);
			executedCommands.clear();
		}
	}
}
