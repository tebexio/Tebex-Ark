#pragma once

#include "TebexArk.h"

#include <Requests.h>

inline json items_listing;

class TebexBuyChatCommand {
public:
	static void Call(TebexArk* plugin);
	static void ApiCallback(TebexArk* plugin, std::string responseText);
	static void ShowCategoriesCommand(TebexArk* plugin, AShooterPlayerController* player, FString* message);
	static void ShowtemsCommand(TebexArk* plugin, AShooterPlayerController* player, const FString& category);
};

inline void TebexBuyChatCommand::Call(TebexArk* plugin) {
	const std::string url = (plugin->getConfig().baseUrl + L"/listing").ToString();
	std::vector<std::string> headers{
		fmt::format("X-Buycraft-Secret: {}", plugin->getConfig().secret.ToString()),
		"X-Buycraft-Handler: TebexBuyChatCommand"
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

inline void TebexBuyChatCommand::ApiCallback(TebexArk* plugin, std::string responseText) {
	try {
		items_listing = nlohmann::json::parse(responseText);
	}
	catch (const nlohmann::detail::parse_error&) {
		plugin->logError("Unable to parse JSON");
	}
}

inline void TebexBuyChatCommand::ShowCategoriesCommand(TebexArk* plugin, AShooterPlayerController* player, FString* message) {
	if (items_listing.find("categories") == items_listing.end()) {
		return;
	}

	TArray<FString> parsed;
	message->ParseIntoArray(parsed, L" ", true);

	if (parsed.IsValidIndex(1)) {
		ShowtemsCommand(plugin, player, parsed[1]);
		return;
	}

	const float display_time = 15.0f;
	const float text_size = 1.3f;

	FString store_str = L"";

	for (const auto& item : items_listing["categories"]) {
		const std::string name = item["name"];

		store_str += FString::Format("{}\n", name);
	}

	store_str += FString::Format(*plugin->GetText("DonateUsage"), *plugin->getConfig().buyCommand);

	ArkApi::GetApiUtils().SendNotification(player, FColorList::Orange, text_size, display_time, nullptr,
	                                       *store_str);
}

inline void TebexBuyChatCommand::ShowtemsCommand(TebexArk* plugin, AShooterPlayerController* player, const FString& category) {
	const float display_time = 15.0f;
	const float text_size = 1.3f;

	json packages = json({});

	for (const auto& item : items_listing["categories"]) {
		const std::string name = item["name"];
		if (FString(name) == category) {
			packages = item["packages"];
			break;
		}
	}

	if (packages.empty()) {
		ArkApi::GetApiUtils().SendNotification(player, FColorList::Orange, text_size, display_time, nullptr,
		                                       *plugin->GetText("WrongCategory"));
		return;
	}

	FString store_str = L"";

	for (const auto& item : packages) {
		const std::string name = item.value("name", "");
		const std::string price = item.value("price", "0");

		store_str += FString::Format(*plugin->GetText("ItemsFormat"), name, price);
	}

	ArkApi::GetApiUtils().SendNotification(player, FColorList::Orange, text_size, display_time, nullptr,
	                                       *store_str);
}
