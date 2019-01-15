#pragma once

#include "TebexArk.h"

#include <Requests.h>

#include "TebexOfflineCommands.h"
#include "TebexOnlineCommands.h"

class TebexForcecheck {
public:
	static void Call(TebexArk* plugin);
	static void ApiCallback(TebexArk* plugin, std::string responseText);
};

inline void TebexForcecheck::Call(TebexArk* plugin) {
	plugin->logWarning("TebexForcecheck");

	const std::string url = (plugin->getConfig().baseUrl + "/queue").ToString();
	std::vector<std::string> headers{
		fmt::format("X-Buycraft-Secret: {}", plugin->getConfig().secret.ToString()),
		"X-Buycraft-Handler: TebexForcecheck"
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

inline void TebexForcecheck::ApiCallback(TebexArk* plugin, std::string responseText) {
	nlohmann::basic_json json;
	try {
		json = nlohmann::json::parse(responseText);
	}
	catch (const nlohmann::detail::parse_error&) {
		plugin->logError("Unable to parse JSON");
		return;
	}

	if (!json["error_message"].is_null()) {
		plugin->logError(FString(json["error_message"].get<std::string>()));
	}
	else {
		if (!json["meta"]["next_check"].is_null()) {
			plugin->setNextCheck(json["meta"]["next_check"].get<int>());
		}

		//plugin->setNextCheck(30);
		if (!json["meta"]["execute_offline"].is_null() && json["meta"]["execute_offline"].get<bool>()) {
			plugin->logWarning("Do offline commands");
			TebexOfflineCommands::Call(plugin);
		}

		if (!json["players"].is_null() && !json["players"].empty()) {
			for (const auto& player : json["players"]) {
				uint64 steamId64;
				try {
					steamId64 = std::stoull(player["uuid"].get<std::string>());
				}
				catch (const std::exception&) {
					plugin->logError("Parsing error");
					return;
				}

				AShooterPlayerController* targetPlayer = ArkApi::GetApiUtils().FindPlayerFromSteamId(steamId64);
				if (targetPlayer != nullptr) {
					plugin->logWarning(FString::Format("Process commands for {0}", player["name"].get<std::string>()));
					TebexOnlineCommands::Call(plugin, player["id"].get<int>(), player["uuid"].get<std::string>());
				}
				else {
					const int playerId = player["id"].get<int>();

					PendingCommand* result = pendingCommands.FindByPredicate([playerId](const auto& data) {
						return data.pluginPlayerId == playerId;
					});

					if (!result) {
						pendingCommands.Add({playerId, steamId64});

						plugin->logWarning(FString::Format("PendingCommands add {}", steamId64));

					}
				}
			}
		}
	}
}
