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

#ifdef TEBEX_ATLAS
#pragma comment(lib, "lib/AtlasApi.lib")
#else
#pragma comment(lib, "lib/ArkApi.lib")
#endif

DECLARE_HOOK(AShooterGameMode_HandleNewPlayer, bool, AShooterGameMode*, AShooterPlayerController*, UPrimalPlayerData*,
 AShooterCharacter*, bool);

TebexArk* gPlugin;

TebexArk::TebexArk() {
	Log::Get().Init("Tebex" + getGameType());
	logger_ = Log::GetLog();

	logWarning("Plugin Loading...");
	lastCalled -= 14 * 60;
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

		const int delay = command["delay"].get<int>();
		if (delay == 0) {
			logWarning(FString("Exec ") + targetCommand);
			if (player != nullptr) {
				ConsoleCommand(player, targetCommand);
			}
			else {
				ConsoleCommand(firstPlayer, targetCommand);
			}
		}
		else {
			API::Timer::Get().DelayExecute([this, steamId64, targetCommand]() {
				AShooterPlayerController* player = ArkApi::GetApiUtils().FindPlayerFromSteamId(steamId64);
				if (player != nullptr) {
					this->logWarning(FString("Exec ") + targetCommand);

					ConsoleCommand(player, targetCommand);

					this->logWarning("Done!");
				}
			}, delay);
		}

		executedCommands.push_back(command["id"].get<int>());
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
			[this](std::string log) { this->logWarning(FString(log)); },
			[this](std::string body) { return this->parsePushCommands(body); });

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

void TebexArk::setConfig(const std::string& key, const std::string& value) {
	if (key == "secret") {
		config_.secret = FString(value);
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
	json configJson;

	logWarning(FString::Format("Save config to {0}", configPath));

	configJson["baseUrl"] = config_.baseUrl.ToString();
	configJson["buyCommand"] = config_.buyCommand.ToString();
	configJson["secret"] = config_.secret.ToString();
	configJson["buyEnabled"] = config_.buyEnabled;
	configJson["enablePushCommands"] = config_.enablePushCommands;
	configJson["ipPushCommands"] = config_.ipPushCommands.ToString();
	configJson["portPushCommands"] = config_.portPushCommands;

	std::fstream configFile{configPath};
	configFile << configJson.dump();
	configFile.close();
}

void TebexArk::readConfig() {
	const std::string configPath = getConfigPath();
	std::fstream configFile{configPath};

	if (!configFile.is_open()) {
		logWarning("No config file found, creating default");
	}
	else {
		json configJson;
		configFile >> configJson;

		if (!configJson["baseUrl"].is_null()) {
			config_.baseUrl = FString(configJson["baseUrl"].get<std::string>());
		}
		if (!configJson["buyCommand"].is_null()) {
			config_.buyCommand = FString(configJson["buyCommand"].get<std::string>());
		}
		if (!configJson["secret"].is_null()) {
			config_.secret = FString(configJson["secret"].get<std::string>());
		}
		if (!configJson["buyEnabled"].is_null()) {
			config_.buyEnabled = configJson["buyEnabled"].get<bool>();
		}
		if (!configJson["enablePushCommands"].is_null()) {
			config_.enablePushCommands = configJson["enablePushCommands"].get<bool>();
		}
		if (!configJson["ipPushCommands"].is_null()) {
			config_.ipPushCommands = FString(configJson["ipPushCommands"].get<std::string>());
		}
		if (!configJson["portPushCommands"].is_null()) {
			config_.portPushCommands = configJson["portPushCommands"].get<int>();
		}

		configFile.close();
	}

	saveConfig();
}

void TebexArk::setNextCheck(int newVal) {
	nextCheck_ = newVal;
}

bool TebexArk::doCheck() {
	const time_t now = time(nullptr);
	if ((now - lastCalled) > nextCheck_) {
		lastCalled = time(nullptr);
		return true;
	}
	return false;
}

FString TebexArk::buildCommand(std::string command, std::string playerName, std::string playerId,
                               std::string UE4ID) const {
	ReplaceStringInPlace(command, "{username}", playerName);
	ReplaceStringInPlace(command, "{id}", playerId);
	ReplaceStringInPlace(command, "{ue4id}", UE4ID);
	return FString(command);
}

void TebexArk::ConsoleCommand(APlayerController* player, FString command) {
	FString result;

#ifdef TEBEX_ARK
	player->ConsoleCommand(&result, &command, true);
#else // In Atlas only admins can execute cheat commands
	const bool is_admin = player->bIsAdmin()(), is_cheat = player->bCheatPlayer()();
	player->bIsAdmin() = true;
	player->bCheatPlayer() = true;

	player->ConsoleCommand(&result, &command, true);

	player->bIsAdmin() = is_admin;
	player->bCheatPlayer() = is_cheat;
#endif
}

/*FString getHttpRef() {
	std::string str("00112233445566778899aabbccddeeffgghhiijjkkllmmoonnppooqq");
	std::random_device rd;
	std::mt19937 generator(rd());

	std::shuffle(str.begin(), str.end(), generator);

	return FString(str.substr(0, 10));
}*/

void TebexArk::ReplaceStringInPlace(std::string& subject, const std::string& search,
                                    const std::string& replace) const {
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != std::string::npos) {
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
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
		gPlugin->logWarning(FString::Format("HandleNewPlayer {} {}", result->pluginPlayerId, std::to_string(result->playerId)));

		const int pluginPlayerId = result->pluginPlayerId;
		const std::string playerId = std::to_string(result->playerId);

		API::Timer::Get().DelayExecute([pluginPlayerId, playerId]() {
			TebexOnlineCommands::Call(gPlugin, pluginPlayerId, playerId);
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
		plugin->getConfig().buyCommand, [plugin](AShooterPlayerController* player, FString*, EChatSendMode::Type) {
			ArkApi::GetApiUtils().SendServerMessage(player, FColorList::Red, *FString::Format(
				                                        "To buy packages from our webstore, please visit {0}",
				                                        plugin->getWebstore().domain.ToString()));
		});

	plugin->logWarning("Loading Config...");
	plugin->readConfig();

	tebexConfig::Config tmpconfig = plugin->getConfig();

	if (tmpconfig.secret.ToString().empty()) {
		plugin->logError("You have not yet defined your secret key. Use /tebex:secret <secret> to define your key");
	}
	else {
		TebexInfo::Call(plugin);
	}

	ArkApi::GetCommands().AddOnTimerCallback("commandChecker", [plugin]() {
		if (plugin->doCheck()) {
			plugin->loadServer();
			TebexForcecheck::Call(plugin);
		}
	});

	ArkApi::GetHooks().SetHook("AShooterGameMode.HandleNewPlayer_Implementation",
	                           &Hook_AShooterGameMode_HandleNewPlayer,
	                           &AShooterGameMode_HandleNewPlayer_original);
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
