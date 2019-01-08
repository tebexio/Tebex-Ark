#pragma once

#ifdef TEBEX_ARK
#include <API/ARK/Ark.h>
#else
#include <API/Atlas/Atlas.h>
#endif

#include "WebstoreInfo.h"
#include "Config.h"
#include "json.hpp"

using json = nlohmann::json;
using Config = tebexConfig::Config;

class TebexArk {
public:
	TebexArk();

	void logWarning(const FString& message) const;
	void logError(const FString& message) const;
	void setWebstore(const json& json);
	WebstoreInfo getWebstore() const;
	Config getConfig() const;
	void setConfig(const std::string& key, const std::string& value);
	void readConfig();
	void setNextCheck(int newValue);
	bool doCheck();
	FString buildCommand(std::string command, std::string playerName, std::string playerId, std::string UE4ID) const;
	bool loadServer();
	bool parsePushCommands(std::string body);
	std::string getConfigPath() const;
	std::string getGameType() const;

	static void ConsoleCommand(APlayerController* player, FString command);

private:
	void saveConfig();
	void ReplaceStringInPlace(std::string& subject, const std::string& search, const std::string& replace) const;

	std::shared_ptr<spdlog::logger> logger_;
	WebstoreInfo webstoreInfo_;
	Config config_;
	int nextCheck_ = 15 * 60;
	time_t lastCalled = time(nullptr);
	bool serverLoaded_ = false;
};
