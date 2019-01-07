#pragma once

#include "TebexArk.h"

#include "TebexDeleteCommands.h"

using json = nlohmann::json;

class TebexOfflineCommands {
public:
	static void Call(TebexArk* plugin);
	static void ApiCallback(TebexArk* plugin, std::string responseText);
};


inline void TebexOfflineCommands::Call(TebexArk* plugin) {
	const std::string url = (plugin->getConfig().baseUrl + "/queue/offline-commands").ToString();
	const std::vector<std::string> headers{
		fmt::format("X-Buycraft-Secret: {}", plugin->getConfig().secret.ToString()),
		"X-Buycraft-Handler: TebexOfflineCommands"
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

inline void TebexOfflineCommands::ApiCallback(TebexArk* plugin, std::string responseText) {
	const int deleteAfter = 5;

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
		auto commands = json["commands"];

		int commandCnt = 0;
		int exCount = 0;
		std::list<int> executedCommands;
		while (commandCnt < commands.size()) {
			auto command = commands[commandCnt];
			FString targetCommand = plugin->buildCommand(command["command"].get<std::string>(),
			                                             command["player"]["name"].get<std::string>(),
			                                             command["player"]["uuid"].get<std::string>(), "");

			APlayerController* firstPlayer = ArkApi::GetApiUtils().GetWorld()->GetFirstPlayerController();

			if (firstPlayer != nullptr) {
				int delay = command["conditions"]["delay"].get<int>();
				if (delay == 0) {
					plugin->logWarning(FString("Exec ") + targetCommand);

					FString result;
					firstPlayer->ConsoleCommand(&result, &targetCommand, true); // TODO: Check
				}
				else {
					FString* delayCommand = new FString(targetCommand.ToString()); // TODO: Replace with timer
					std::thread([plugin, &delayCommand, delay]() {
						FString targetCommand = FString(delayCommand->ToString());
						FString* cmdPtr = &targetCommand;

						Sleep(delay * 1000);

						APlayerController* firstPlayer = ArkApi::GetApiUtils().GetWorld()->GetFirstPlayerController();
						if (firstPlayer != nullptr) {
							plugin->logWarning(FString("Exec ") + targetCommand);

							FString result;
							firstPlayer->ConsoleCommand(&result, cmdPtr, true);
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
