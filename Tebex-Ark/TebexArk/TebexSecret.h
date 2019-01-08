#pragma once

#include "TebexArk.h"

#include <Requests.h>

using json = nlohmann::json;

class TebexSecret {
public:
	static void Call(TebexArk* plugin, const FString& cmd);
	static void ApiCallback(TebexArk* plugin, std::string responseText);
};

inline void TebexSecret::Call(TebexArk* plugin, const FString& cmd) {
	TArray<FString> parsed;
	cmd.ParseIntoArray(parsed, L" ", true);

	if (!parsed.IsValidIndex(1)) {
		plugin->logError("You must provide a secret for this command");
		return;
	}

	const FString secret = *parsed[1];

	plugin->setConfig("secret", secret.ToString());

	const std::string url = (plugin->getConfig().baseUrl + "/information").ToString();
	std::vector<std::string> headers{
		fmt::format("X-Buycraft-Secret: {}", plugin->getConfig().secret.ToString()),
		"X-Buycraft-Handler: TebexSecret"
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

inline void TebexSecret::ApiCallback(TebexArk* plugin, std::string responseText) {
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
		plugin->logWarning(FString::Format(L"Your secret key has been validated! Webstore Name: {0}",
		                                   json["account"]["name"].get<std::string>()));
		plugin->setWebstore(json);
	}
}
