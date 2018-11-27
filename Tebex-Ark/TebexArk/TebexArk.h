#pragma once
#include <API/ARK/Ark.h>
#include <fstream>
#include <random>
#include "WebstoreInfo.cpp"
#include "Config.cpp"
#include "json.hpp"
#include "TebexPushCommands.hpp"

using json = nlohmann::json;
using Config = tebexConfig::Config;

class TebexArk
{
	private:
		std::shared_ptr<spdlog::logger> logger;
		WebstoreInfo webstoreInfo;
		Config config;
		void saveConfig();
		int nextCheck = 15 * 60;
		time_t lastCalled = time(nullptr);
		void ReplaceStringInPlace(std::string& subject, const std::string& search, const std::string& replace);
		std::list<TSharedRef<IHttpRequest>> requests;
		bool serverLoaded = false;
		uint64 strToSteamID(std::string steamId);

	public:			
		TebexArk();
		void logWarning(FString message);
		void logError(FString message);
		void setWebstore(json json);
		WebstoreInfo getWebstore();
		Config getConfig();
		void setConfig(std::string key, std::string value);
		void readConfig();
		void setNextCheck(int newValue);
		bool doCheck();
		FString buildCommand(std::string command, std::string playerName, std::string playerId, std::string UE4ID);
		void addRequestToQueue(TSharedRef<IHttpRequest> request);
		void removeRequestFromQueue();
		std::list<TSharedRef<IHttpRequest>> getRequests();
		bool loadServer();
		bool parsePushCommands(std::string commands);
;};


TebexArk::TebexArk() {
	Log::Get().Init("TebexArk");
	logger = Log::GetLog();

	logWarning("Plugin Loading...");
	lastCalled -= 14 * 60;
}

#include "TebexDeleteCommands.h"
bool TebexArk::parsePushCommands(std::string body)
{
	int deleteAfter = 5;

	nlohmann::basic_json commands = nlohmann::json::parse("[]");
	commands = nlohmann::json::parse(body);
	int commandCnt = 0;
	int exCount = 0;
	std::list<int> executedCommands;
	while (commandCnt < commands.size()) {
		auto command = commands[commandCnt];

		this->logWarning(FString("Push Command Received: " + command["command"].get<std::string>()));
		
		std::string playerId = command["username"].get<std::string>();

		uint64 steamId64 = TebexArk::strToSteamID(playerId);

		AShooterPlayerController *player = ArkApi::GetApiUtils().FindPlayerFromSteamId(steamId64);
		APlayerController *FirstPlayer = ArkApi::GetApiUtils().GetWorld()->GetFirstPlayerController();

		std::string playerUsername;
		std::string ue4id;

		if (player == nullptr) {
			ue4id = "";			
			playerUsername = command["username_name"].get<std::string>();
		}
		else {
			ue4id = std::to_string(player->LinkedPlayerIDField());

			FString* playerName = new FString();
			player->GetPlayerCharacterName(playerName);

			playerUsername = playerName->ToString();
		}

		FString targetCommand = this->buildCommand(command["command"].get<std::string>(), playerUsername, playerId, ue4id);
		int requireOnline = command["require_online"].get<int>();

		if (requireOnline == 1 && player == nullptr) {
			commandCnt++;
			continue;
		}

		if (player == nullptr && FirstPlayer == nullptr) {
			this->logWarning("No player available to execute");
			commandCnt++;
			continue;
		}

		FString *result = &FString();

		int delay = command["delay"].get<int>();
		if (delay == 0) {
			this->logWarning(FString("Exec ") + targetCommand);
			if (player != nullptr) {
				player->ConsoleCommand(result, &targetCommand, true);
			}
			else {
				FirstPlayer->ConsoleCommand(result, &targetCommand, true);
			}
		}
		else {
			FString *delayCommand = new FString(targetCommand.ToString());
			std::thread([this, &delayCommand, result, delay, steamId64]() {
				FString targetCommand = FString(delayCommand->ToString());
				FString *cmdPtr = &targetCommand;

				Sleep(delay * 1000);
				APlayerController *FirstPlayer = ArkApi::GetApiUtils().GetWorld()->GetFirstPlayerController();
				if (FirstPlayer != nullptr) {
					this->logWarning(FString("Exec ") + targetCommand);
					FirstPlayer->ConsoleCommand(result, cmdPtr, true);
					this->logWarning("Done!");
				}
				return false;
			}).detach();
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

	if (!serverLoaded && this->getConfig().enablePushCommands) {
		logWarning("Loading HTTP Server Async....");
		TebexPushCommands *pushCommands = new TebexPushCommands(
			this->getConfig().secret.ToString(),
			[this](std::string log) {this->logWarning(FString(log)); },
			[this](std::string body) { return this->parsePushCommands(body); });


		pushCommands->startServer(this->getConfig().ipPushCommands.ToString(), this->getConfig().portPushCommands);
		serverLoaded = true;
	}

	return serverLoaded;
}

std::list<TSharedRef<IHttpRequest>> TebexArk::getRequests() {
	return requests;
}


void TebexArk::logWarning(FString message) {
	logger->info(message.ToString());
}

void TebexArk::logError(FString message) {
	logger->critical(message.ToString());
}

void TebexArk::setWebstore(json json) {
	webstoreInfo.id = json["account"]["id"].get<int>();
	webstoreInfo.name = FString(json["account"]["name"].get<std::string>());
	webstoreInfo.domain = FString(json["account"]["domain"].get<std::string>());
	webstoreInfo.currency = FString(json["account"]["currency"]["iso_4217"].get<std::string>());
	webstoreInfo.currencySymbol = FString(json["account"]["currency"]["symbol"].get<std::string>());
	webstoreInfo.gameType = FString(json["account"]["game_type"].get<std::string>());

	webstoreInfo.serverName = FString(json["server"]["name"].get<std::string>());
	webstoreInfo.serverId = json["server"]["id"].get<int>();

}

WebstoreInfo TebexArk::getWebstore() {
	return webstoreInfo;
}

Config TebexArk::getConfig()
{
	return config;
}

void TebexArk::setConfig(std::string key, std::string value) {
	if (key == "secret") {
		config.secret = FString(value);
	}

	saveConfig();
}

void TebexArk::saveConfig()
{
	const std::string config_path = ArkApi::Tools::GetCurrentDir() + "\\ArkApi\\Plugins\\TebexArk\\config.json";
	json configJson;

	logWarning(FString::Format("Save config to {0}", config_path));

	configJson["baseUrl"] = config.baseUrl.ToString();
	configJson["buyCommand"] = config.buyCommand.ToString();
	configJson["secret"] = config.secret.ToString();
	configJson["buyEnabled"] = config.buyEnabled;
	configJson["enablePushCommands"] = config.enablePushCommands;
	configJson["ipPushCommands"] = config.ipPushCommands.ToString();
	configJson["portPushCommands"] = config.portPushCommands;
	
	std::fstream configFile;
	configFile.open(config_path);
	configFile << configJson.dump();
	configFile.close();
}

void TebexArk::readConfig()
{
	const std::string config_path = ArkApi::Tools::GetCurrentDir() + "\\ArkApi\\Plugins\\TebexArk\\config.json";
	std::fstream configFile;
	configFile.open(config_path);

	if (!configFile.is_open()) {
		logWarning("No config file found, creating default");		
	} else {
		json configJson;
		configFile >> configJson;
		if (!configJson["baseUrl"].is_null()) {
			config.baseUrl = FString(configJson["baseUrl"].get<std::string>());
		}
		if (!configJson["buyCommand"].is_null()) {
			config.buyCommand = FString(configJson["buyCommand"].get<std::string>());
		}
		if (!configJson["secret"].is_null()) {
			config.secret = FString(configJson["secret"].get<std::string>());
		}
		if (!configJson["buyEnabled"].is_null()) {
			config.buyEnabled = configJson["buyEnabled"].get<bool>();
		}
		if (!configJson["enablePushCommands"].is_null()) {
			config.enablePushCommands = configJson["enablePushCommands"].get<bool>();
		}
		if (!configJson["ipPushCommands"].is_null()) {
			config.ipPushCommands = FString(configJson["ipPushCommands"].get<std::string>());
		}
		if (!configJson["portPushCommands"].is_null()) {
			config.portPushCommands = configJson["portPushCommands"].get<int>();
		}


		configFile.close();
	}
	saveConfig();
}

void TebexArk::setNextCheck(int newVal) {
	nextCheck = newVal;
}

bool TebexArk::doCheck() {
	time_t now = time(nullptr);
	if ((now - lastCalled) > nextCheck) {
		lastCalled = time(nullptr);
		return true;
	}
	return false;
}

FString TebexArk::buildCommand(std::string command, std::string playerName, std::string playerId, std::string UE4ID) {

	ReplaceStringInPlace(command, "{username}", playerName);
	ReplaceStringInPlace(command, "{id}", playerId);
	ReplaceStringInPlace(command, "{ue4id}", UE4ID);
	return FString(command);
}

void TebexArk::addRequestToQueue(TSharedRef<IHttpRequest> request) {
	requests.push_back(request);
}

void TebexArk::removeRequestFromQueue() {
	requests.pop_front();
}


FString getHttpRef()
{
	std::string str("00112233445566778899aabbccddeeffgghhiijjkkllmmoonnppooqq");
	std::random_device rd;
	std::mt19937 generator(rd());

	std::shuffle(str.begin(), str.end(), generator);

	return FString(str.substr(0, 10));
}

void TebexArk::ReplaceStringInPlace(std::string& subject, const std::string& search,
	const std::string& replace) {
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != std::string::npos) {
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
}

uint64 TebexArk::strToSteamID(std::string steamId) {
	uint64 result = 0;
	char const* p = steamId.c_str();
	char const* q = p + steamId.size();

	while (p < q) {
		result *= 10;
		result += *(p++) - '0';
	}
	return result;
}