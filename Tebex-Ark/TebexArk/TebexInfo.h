#pragma once

#include "TebexArk.h"

#include <Requests.h>

using json = nlohmann::json;

class TebexInfo {
public:
	static void Call(TebexArk* plugin);
	static void ApiCallback(TebexArk* plugin, std::string responseText);
};

inline void TebexInfo::Call(TebexArk* plugin) {
	const std::string url = (plugin->getConfig().baseUrl + "/information").ToString();
	const std::vector<std::string> headers{
		fmt::format("X-Buycraft-Secret: {}", plugin->getConfig().secret.ToString()),
		"X-Buycraft-Handler: TebexInfo"
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

inline void TebexInfo::ApiCallback(TebexArk* plugin, std::string responseText) {
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
		plugin->logWarning("Server Information");
		plugin->logWarning("=======================");
		FString buffer;
		buffer += FString::Format(L"Server {0} for webstore {1}", json["server"]["name"].get<std::string>(),
		                          json["account"]["name"].get<std::string>());
		plugin->logWarning(buffer);
		buffer = FString("Server prices are in ") + FString(json["account"]["currency"]["iso_4217"].get<std::string>());
		plugin->logWarning(FString::Format(L"Server prices are in {0}",
		                                   json["account"]["currency"]["iso_4217"].get<std::string>()));
		plugin->logWarning(FString::Format(L"Webstore domain: {0}", json["account"]["domain"].get<std::string>()));

		plugin->setWebstore(json);
	}
}
