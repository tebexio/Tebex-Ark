#define _WINSOCKAPI_
#include <WinSock2.h>
#include <WS2tcpip.h>

#include "TebexArk.h"

#include <fstream>
#include <filesystem>

#include "TebexInfo.h"
#include "TebexSecret.h"
#include "TebexForcecheck.h"
#include "TebexOnlineCommands.h"
#include "TebexPushCommands.hpp"
#include "TebexBuyCommand.h"

#ifdef TEBEX_ATLAS
#pragma comment(lib, "lib/AtlasApi.lib")
#else
#pragma comment(lib, "lib/ArkApi.lib")
#endif

DECLARE_HOOK(AShooterGameMode_InitGame, void, AShooterGameMode*, FString*, FString*, FString*);
DECLARE_HOOK(AShooterGameMode_HandleNewPlayer, bool, AShooterGameMode*, AShooterPlayerController*, UPrimalPlayerData*,
 AShooterCharacter*, bool);

TebexArk* gPlugin;

TebexArk::TebexArk() {
	Log::Get().Init("Tebex" + getGameType());
	logger_ = Log::GetLog();

	logWarning("Plugin Loading...");
	last_called_ -= 14 * 60;
}

bool TebexArk::parsePushCommands(const std::string& body) {
	const int deleteAfter = 5;

	nlohmann::basic_json commands = nlohmann::json::parse(body);
	unsigned commandCnt = 0;
	int exCount = 0;
	std::list<int> executedCommands;

	while (commandCnt < commands.size()) {
		auto command = commands[commandCnt];

		this->logWarning(FString("Push Command Received: " + command["command"].get<std::string>()));

		std::string playerId = command["username"].get<std::string>();

		uint64 steamId64;
		try {
			steamId64 = std::stoull(playerId);
		}
		catch (const std::exception&) {
			return false;
		}

		AShooterPlayerController* player = ArkApi::GetApiUtils().FindPlayerFromSteamId(steamId64);
		APlayerController* firstPlayer = ArkApi::GetApiUtils().GetWorld()->GetFirstPlayerController();

		std::string playerUsername;
		std::string ue4id;

		if (player == nullptr) {
			ue4id = "";
			playerUsername = command["username_name"].get<std::string>();
		}
		else {
			ue4id = std::to_string(player->LinkedPlayerIDField());

			FString playerName;
			player->GetPlayerCharacterName(&playerName);

			playerUsername = playerName.ToString();
		}

		FString targetCommand = buildCommand(command["command"].get<std::string>(), playerUsername, playerId,
		                                     ue4id);
		std::string requireOnline = command["require_online"].get<std::string>();

		if (requireOnline == "1" && player == nullptr) {
			commandCnt++;
			continue;
		}

		if (player == nullptr && firstPlayer == nullptr) {
			logWarning("No player available to execute");
			commandCnt++;
			continue;
		}

		bool result = false;

		const int commandId = command["id"].get<int>();

		const int delay = command["delay"].get<int>();
		if (delay == 0) {
			logWarning(FString("Exec ") + targetCommand);
			if (player != nullptr) {
				result = ConsoleCommand(player, targetCommand, true);
			}
			else {
				result = ConsoleCommand(firstPlayer, targetCommand, true);
			}
		}
		else {
			API::Timer::Get().DelayExecute([this, steamId64, targetCommand, commandId]() {
				AShooterPlayerController* player = ArkApi::GetApiUtils().FindPlayerFromSteamId(steamId64);
				if (player != nullptr) {
					this->logWarning(FString("Exec ") + targetCommand);

					const bool result = ConsoleCommand(player, targetCommand, true);
					if (result) {
						TebexDeleteCommands::Call(this, { commandId });

						this->logWarning("Done");
					}
					else {
						this->logWarning("Execution wasn't successful");
					}
				}
			}, delay);
		}

		if (!result) {
			commandCnt++;
			continue;
		}

		executedCommands.push_back(commandId);
		exCount++;

		if (exCount % deleteAfter == 0) {
			TebexDeleteCommands::Call(this, executedCommands);
			executedCommands.clear();
		}

		commandCnt++;
	}

	this->logWarning(FString::Format("{0} commands executed", exCount));
	if (exCount % deleteAfter != 0) {
		TebexDeleteCommands::Call(this, executedCommands);
		executedCommands.clear();
	}

	return true;
}

bool TebexArk::loadServer() {
	if (!serverLoaded_ && getConfig().enablePushCommands) {
		logWarning("Loading HTTP Server Async....");
		TebexPushCommands* pushCommands = new TebexPushCommands(
			getConfig().secret.ToString(),
			[this](std::string log) {
				this->logWarning(FString(log));
			},
			[this](std::string body) {
				return this->parsePushCommands(body);
			});

		pushCommands->startServer(this->getConfig().ipPushCommands.ToString(), this->getConfig().portPushCommands);
		serverLoaded_ = true;
	}

	return serverLoaded_;
}

void TebexArk::logWarning(const FString& message) const {
	logger_->info(message.ToString());
}

void TebexArk::logError(const FString& message) const {
	logger_->critical(message.ToString());
}

void TebexArk::setWebstore(const json& json) {
	webstoreInfo_.id = json["account"]["id"].get<int>();
	webstoreInfo_.name = FString(json["account"]["name"].get<std::string>());
	webstoreInfo_.domain = FString(json["account"]["domain"].get<std::string>());
	webstoreInfo_.currency = FString(json["account"]["currency"]["iso_4217"].get<std::string>());
	webstoreInfo_.currencySymbol = FString(json["account"]["currency"]["symbol"].get<std::string>());
	webstoreInfo_.gameType = FString(json["account"]["game_type"].get<std::string>());

	webstoreInfo_.serverName = FString(json["server"]["name"].get<std::string>());
	webstoreInfo_.serverId = json["server"]["id"].get<int>();

}

WebstoreInfo TebexArk::getWebstore() const {
	return webstoreInfo_;
}

Config TebexArk::getConfig() const {
	return config_;
}

json TebexArk::getJson() const {
	return json_config_;
}

FString TebexArk::GetText(const std::string& str, const std::string& default_message) const {
	return FString(ArkApi::Tools::Utf8Decode(json_config_.value("Messages", json::object()).value(str, default_message)).c_str());
}

void TebexArk::setConfig(const std::string& key, const std::string& value) {
	if (key == "secret") {
		json_config_["secret"] = value;
		config_.secret = value.c_str();
	}

	saveConfig();
}

std::string TebexArk::getGameType() const {
	namespace fs = std::filesystem;

	const std::string current_dir = ArkApi::Tools::GetCurrentDir();

	for (const auto& directory_entry : fs::directory_iterator(current_dir)) {
		const auto& path = directory_entry.path();
		if (is_directory(path)) {
			const auto name = path.filename().stem().generic_string();
			if (name == "ArkApi") {
				return "Ark";
			}
			if (name == "AtlasApi") {
				return "Atlas";
			}
		}
	}

	return "";
}

time_t TebexArk::getLastCalled() const {
	return last_called_;
}

int TebexArk::getNextCheck() const {
	return nextCheck_;
}

std::string TebexArk::getConfigPath() const {
	const std::string gameType = getGameType();

	if (gameType == "Ark") {
		return ArkApi::Tools::GetCurrentDir() + R"(\ArkApi\Plugins\TebexArk\config.json)";
	}
	if (gameType == "Atlas") {
		return ArkApi::Tools::GetCurrentDir() + R"(\AtlasApi\Plugins\TebexAtlas\config.json)";
	}

	return "";
}

void TebexArk::saveConfig() {
	const std::string configPath = getConfigPath();

	logWarning(FString::Format("Save config to {0}", configPath));

	if (json_config_.find("baseUrl") == json_config_.end())
		json_config_["baseUrl"] = config_.baseUrl.ToString();
	if (json_config_.find("buyCommand") == json_config_.end())
		json_config_["buyCommand"] = config_.buyCommand.ToString();
	if (json_config_.find("buyEnabled") == json_config_.end())
		json_config_["buyEnabled"] = config_.buyEnabled;
	if (json_config_.find("enablePushCommands") == json_config_.end())
		json_config_["enablePushCommands"] = config_.enablePushCommands;
	if (json_config_.find("ipPushCommands") == json_config_.end())
		json_config_["ipPushCommands"] = config_.ipPushCommands.ToString();
	if (json_config_.find("portPushCommands") == json_config_.end())
		json_config_["portPushCommands"] = config_.portPushCommands;
	if (json_config_.find("secret") == json_config_.end())
		json_config_["secret"] = config_.secret.ToString();

	std::ofstream configFile{configPath};
	configFile << json_config_.dump(2);
	configFile.close();
}

void TebexArk::readConfig(const std::string& address) {
	const std::string configPath = getConfigPath();
	std::fstream configFile{configPath};

	if (!configFile.is_open()) {
		logWarning("No config file found, creating default");
		saveConfig();
	}
	else {
		configFile >> json_config_;

		if (!json_config_["baseUrl"].is_null()) {
			config_.baseUrl = FString(json_config_["baseUrl"].get<std::string>());
		}
		if (!json_config_["buyCommand"].is_null()) {
			config_.buyCommand = FString(json_config_["buyCommand"].get<std::string>());
		}
		if (!json_config_["secret"].is_null()) {
			config_.secret = FString(getSecret(json_config_, address));
		}
		if (!json_config_["buyEnabled"].is_null()) {
			config_.buyEnabled = json_config_["buyEnabled"].get<bool>();
		}
		if (!json_config_["enablePushCommands"].is_null()) {
			config_.enablePushCommands = json_config_["enablePushCommands"].get<bool>();
		}
		if (!json_config_["ipPushCommands"].is_null()) {
			config_.ipPushCommands = FString(json_config_["ipPushCommands"].get<std::string>());
		}
		if (!json_config_["portPushCommands"].is_null()) {
			config_.portPushCommands = json_config_["portPushCommands"].get<int>();
		}

		configFile.close();
	}
}

std::string TebexArk::getSecret(const json& config, const std::string& address) const {
	std::string default_secret = config["secret"];

	return getGameType() == "Atlas"
		       ? config["secrets"].value(address, default_secret)
		       : default_secret;
}

void TebexArk::setNextCheck(int newVal) {
	nextCheck_ = newVal;
}

bool TebexArk::doCheck() {
	const time_t now = time(nullptr);
	if ((now - last_called_) > nextCheck_) {
		last_called_ = time(nullptr);
		return true;
	}
	return false;
}

FString TebexArk::buildCommand(std::string command, std::string playerName, std::string playerId,
                               std::string UE4ID) {
	ReplaceStringInPlace(command, "{username}", playerName);
	ReplaceStringInPlace(command, "{id}", playerId);
	ReplaceStringInPlace(command, "{ue4id}", UE4ID);
	return FString(command);
}

int TebexArk::GetTotalInventoryItems(AShooterPlayerController* player) {
	if (player && player->GetPlayerCharacter() && player->GetPlayerCharacter()->MyInventoryComponentField()) {
		UPrimalInventoryComponent* inv = player->GetPlayerCharacter()->MyInventoryComponentField();

		int totalAmount = 0;

		for (UPrimalItem* item : inv->InventoryItemsField()) {
			totalAmount += item->GetItemQuantity();
		}

		return totalAmount;
	}

	return 0;
}

bool IsCommandGiveItems(const FString& command) {
	std::vector<FString> commands{
		"GiveItem",
		"GiveItemNum",
		"GiveItemToPlayer",
		"GiveItemNumToPlayer",
		"GiveItemSet",
		"GiveSlotItem",
		"GiveSlotItemNum",
		"GFI" 
	};

	for (const FString& cmd : commands)	{
		if (command.StartsWith(cmd, ESearchCase::IgnoreCase))
			return true;
	}

	return false;
}

bool TebexArk::ConsoleCommand(APlayerController* player, FString command, bool checkInventory) {
	AShooterPlayerController* player_controller = static_cast<AShooterPlayerController*>(player);

	int oldTotalAmount = 0;

	if (checkInventory && !IsCommandGiveItems(command))	{
		checkInventory = false;
	}

	if (checkInventory) {
		oldTotalAmount = GetTotalInventoryItems(player_controller);
	}

	FString result;

//#ifdef TEBEX_ARK
//	player->ConsoleCommand(&result, &command, true);
//#else // In Atlas only admins can execute cheat commands
	const bool is_cheat = player->bCheatPlayer()();
	//player->bIsAdmin() = true;
	player->bCheatPlayer() = true;

	static_cast<APlayerController*>(player)->ConsoleCommand(&result, &command, false);

	//player->bIsAdmin() = is_admin;
	player->bCheatPlayer() = is_cheat;
//#endif

	if (checkInventory) {
		const int newTotalAmount = GetTotalInventoryItems(player_controller);

		const bool check = newTotalAmount > oldTotalAmount;
		if (!check) {
			const time_t now = time(nullptr);
			const time_t lastCalled = getLastCalled();
			const int nextCheck = static_cast<int>(abs(getNextCheck() - (now - lastCalled)));

			ArkApi::GetApiUtils().SendChatMessage(player_controller, GetText("Sender", "Tebex"), 
				*GetText("FailedItem", "Failed to add item. Another attempt will be made in {} seconds."), nextCheck);
		}

		return check;
	}

	return true;
}

/*FString getHttpRef() {
	std::string str("00112233445566778899aabbccddeeffgghhiijjkkllmmoonnppooqq");
	std::random_device rd;
	std::mt19937 generator(rd());

	std::shuffle(str.begin(), str.end(), generator);

	return FString(str.substr(0, 10));
}*/

void TebexArk::ReplaceStringInPlace(std::string& subject, const std::string& search,
                                    const std::string& replace) {
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != std::string::npos) {
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
}

void Hook_AShooterGameMode_InitGame(AShooterGameMode* a_shooter_game_mode, FString* map_name, FString* options,
                                    FString* error_message) {
	AShooterGameMode_InitGame_original(a_shooter_game_mode, map_name, options, error_message);

#ifdef TEBEX_ATLAS
	FString ip;
	int port;

	static_cast<UShooterGameInstance*>(
			ArkApi::GetApiUtils().GetWorld()->OwningGameInstanceField())->
		GridInfoField()->GetCurrentServerIPAndPort(&ip, &port);

	std::string address = fmt::format("{}:{}", ip.ToString(), port);

	gPlugin->logWarning(FString::Format("Server address: {}", address));

	gPlugin->logWarning("Loading Config...");
	gPlugin->readConfig(address);
#else
	gPlugin->logWarning("Loading Config...");
	gPlugin->readConfig("");
#endif

	tebexConfig::Config tmpconfig = gPlugin->getConfig();

	if (tmpconfig.secret.ToString().empty()) {
		gPlugin->logError("You have not yet defined your secret key. Use /tebex:secret <secret> to define your key");
		return;
	}

	TebexInfo::Call(gPlugin);

	ArkApi::GetCommands().AddOnTimerCallback("commandChecker", []() {
		if (gPlugin->doCheck()) {
			gPlugin->loadServer();
			TebexForcecheck::Call(gPlugin);
			TebexBuyChatCommand::Call(gPlugin);
		}
	});
}

bool Hook_AShooterGameMode_HandleNewPlayer(AShooterGameMode* _this, AShooterPlayerController* new_player,
                                           UPrimalPlayerData* player_data, AShooterCharacter* player_character,
                                           bool is_from_login) {
	const bool res = AShooterGameMode_HandleNewPlayer_original(_this, new_player, player_data, player_character, is_from_login);

	const uint64 steamId = ArkApi::IApiUtils::GetSteamIdFromController(new_player);

	PendingCommand* result = pendingCommands.FindByPredicate([steamId](const auto& data) {
		return data.playerId == steamId;
	});
	if (result) {
		const int pluginPlayerId = result->pluginPlayerId;
		const std::string playerId = std::to_string(result->playerId);

		API::Timer::Get().DelayExecute([pluginPlayerId, playerId]() {
			const time_t now = time(nullptr);
			const time_t lastCalled = gPlugin->getLastCalled();
			const int nextCheck = gPlugin->getNextCheck();

			if (abs(nextCheck - (now - lastCalled)) >= 30) {
				gPlugin->logWarning(FString::Format("Give items to connected player {} {}", pluginPlayerId, playerId));

				TebexOnlineCommands::Call(gPlugin, pluginPlayerId, playerId);
			}
		}, 30);

		pendingCommands.RemoveAllSwap([steamId](const auto& data) {
			return data.playerId == steamId;
		});
	}

	return res;
}

void Load() {
	TebexArk* plugin = new TebexArk();
	gPlugin = plugin;

	ArkApi::GetCommands().AddConsoleCommand(
		"tebex:info", [plugin](APlayerController*, FString*, bool) {
			TebexInfo::Call(plugin);
		});
	ArkApi::GetCommands().AddRconCommand(
		"tebex:info",
		[plugin](RCONClientConnection*, RCONPacket*, UWorld*) {
			TebexInfo::Call(plugin);
		});

	ArkApi::GetCommands().AddConsoleCommand(
		"tebex:forcecheck",
		[plugin](APlayerController*, FString*, bool) {
			TebexForcecheck::Call(plugin);
		});
	ArkApi::GetCommands().AddRconCommand(
		"tebex:forcecheck",
		[plugin](RCONClientConnection*, RCONPacket*, UWorld*) {
			TebexForcecheck::Call(plugin);
		});

	ArkApi::GetCommands().AddConsoleCommand(
		"tebex:secret",
		[plugin](APlayerController*, FString* command, bool) {
			TebexSecret::Call(plugin, *command);
		});
	ArkApi::GetCommands().AddRconCommand(
		"tebex:secret",
		[plugin](RCONClientConnection*, RCONPacket* rcon_packet, UWorld*) {
			TebexSecret::Call(plugin, rcon_packet->Body);
		});

	ArkApi::GetCommands().AddChatCommand(
		plugin->getConfig().buyCommand, [plugin](AShooterPlayerController* player, FString* message, EChatSendMode::Type) {
			try {
				TebexBuyChatCommand::ShowCategoriesCommand(plugin, player, message);
			}
			catch(const std::exception& exception) {
				plugin->logError(exception.what());
			}

			ArkApi::GetApiUtils().SendServerMessage(player, FColorList::Red,
			                                        *FString::Format(*plugin->GetText("BuyPackages"),
			                                                         plugin->getWebstore().domain.ToString()));
		});

	ArkApi::GetHooks().SetHook("AShooterGameMode.HandleNewPlayer_Implementation",
	                           &Hook_AShooterGameMode_HandleNewPlayer,
	                           &AShooterGameMode_HandleNewPlayer_original);
	ArkApi::GetHooks().SetHook("AShooterGameMode.InitGame", &Hook_AShooterGameMode_InitGame,
	                           &AShooterGameMode_InitGame_original);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		Load();
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
