#pragma once

#include "TebexArk.h"

#include <Requests.h>

using json = nlohmann::json;

class TebexDeleteCommands {
public:
	static void Call(TebexArk* plugin, const std::list<int>& commandIds);
	static void ApiCallback(TebexArk* plugin, std::string responseText);
};

inline void TebexDeleteCommands::Call(TebexArk* plugin, const std::list<int>& commandIds) {
	FString deleteUrl;

	for (auto const& id : commandIds) {
		deleteUrl += FString::Format("ids[]={0}&", id);

		// Save command ID
		executedCommandsId.insert(id);
	}

	const std::string url = (plugin->getConfig().baseUrl + L"/queue?" + deleteUrl).ToString();
	std::vector<std::string> headers{
		fmt::format("X-Buycraft-Secret: {}", plugin->getConfig().secret.ToString()),
		"X-Buycraft-Handler: TebexDeleteCommands"
	};

	const bool result = API::Requests::Get().CreateDeleteRequest(url, [plugin](bool success, std::string response) {
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

inline void TebexDeleteCommands::ApiCallback(TebexArk* plugin, std::string responseText) {
	//Do nothing :)
}
