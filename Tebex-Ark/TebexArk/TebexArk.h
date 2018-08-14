#pragma once
#include <API/ARK/Ark.h>
#include <fstream>
#include <random>
#include "WebstoreInfo.cpp"
#include "Config.cpp"
#include "json.hpp"

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
;};

TebexArk::TebexArk() {
	Log::Get().Init("TebexArk");
	logger = Log::GetLog();

	logWarning("Plugin Loading...");
	lastCalled -= 14 * 60;
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