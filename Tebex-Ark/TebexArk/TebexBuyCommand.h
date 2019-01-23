#pragma once

#include "TebexArk.h"

#include <Requests.h>

inline json items_listing;

class TebexBuyChatCommand {
public:
	static void Call(TebexArk* plugin);
	static void ShowCategoriesCommand(TebexArk* plugin, AShooterPlayerController* player, FString* message);
	static void ShowtemsCommand(TebexArk* plugin, AShooterPlayerController* player, int category_id);

private:
	static void ApiCallback(TebexArk* plugin, std::string responseText);
	static json FindPackages(int category_id);
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
		int id;

		try {
			id = std::stoi(*parsed[1]);
		}
		catch (const std::exception&) {
			return;
		}

		ShowtemsCommand(plugin, player, id);
		return;
	}

	const float display_time = 15.0f;
	const float text_size_title = 1.4f;
	const float text_size = 1.1f;

	FString store_str = L"";

	for (const auto& item : items_listing["categories"]) {
		int id = item["id"];
		std::string name = item["name"];

		store_str += FString::Format(*plugin->GetText("CategoriesFormat"), id, name);

		for (const auto& item2 : item.value("subcategories", json::object())) {
			id = item2["id"];
			name = item2["name"];

			store_str += FString::Format(*plugin->GetText("SubCategoriesFormat"), id, name);
		}
	}

	FString donate_str = FString::Format(*plugin->GetText("DonateUsage"), *plugin->getConfig().buyCommand);
	FString title_str = FString::Format(*plugin->GetText("CategoriesTitle"), plugin->getWebstore().name.ToString());

	ArkApi::GetApiUtils().SendNotification(player, FColorList::Green, text_size_title, display_time, nullptr, *title_str);
	ArkApi::GetApiUtils().SendNotification(player, FColorList::Orange, text_size, display_time, nullptr, *store_str);
	ArkApi::GetApiUtils().SendNotification(player, FColorList::Green, text_size, display_time, nullptr, *donate_str);
}

inline void TebexBuyChatCommand::ShowtemsCommand(TebexArk* plugin, AShooterPlayerController* player, int category_id) {
	const float display_time = 15.0f;
	const float text_size_title = 1.4f;
	const float text_size = 1.1f;

	json packages = FindPackages(category_id);

	if (packages.empty()) {
		ArkApi::GetApiUtils().SendNotification(player, FColorList::Orange, text_size, display_time, nullptr,
		                                       *plugin->GetText("WrongCategory"));
		return;
	}

	FString store_str = L"";

	for (const auto& item : packages) {
		const int id = item.value("id", 0);
		const std::string name = item.value("name", "");
		const std::string price = item.value("price", "0");

		store_str += FString::Format(*plugin->GetText("ItemsFormat"), id, name, price, plugin->getWebstore().currencySymbol.ToString());
	}

	FString title_str = FString::Format(*plugin->GetText("PackagesTitle"), plugin->getWebstore().name.ToString(), category_id);

	ArkApi::GetApiUtils().SendNotification(player, FColorList::Green, text_size_title, display_time, nullptr, *title_str);
	ArkApi::GetApiUtils().SendNotification(player, FColorList::Orange, text_size, display_time, nullptr,
	                                       *store_str);
}

inline json TebexBuyChatCommand::FindPackages(int category_id) {
	for (const auto& item : items_listing["categories"]) {
		int id = item["id"];
		if (id == category_id) {
			return item["packages"];
		}

		for (const auto& item2 : item.value("subcategories", json::array())) {
			id = item2["id"];
			if (id == category_id) {
				return item2["packages"];
			}
		}
	}

	return json::array();
}
