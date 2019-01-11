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

struct PendingCommand {
	int pluginPlayerId;
	uint64 playerId;
};

inline TArray<PendingCommand> pendingCommands;

class TebexArk {
public:
	TebexArk();

	void logWarning(const FString& message) const;
	void logError(const FString& message) const;
	void setWebstore(const json& json);
	WebstoreInfo getWebstore() const;
	Config getConfig() const;
	void setConfig(const std::string& key, const std::string& value);
	void readConfig(const std::string& address);
	std::string getSecret(const json& config, const std::string& address) const;
	void setNextCheck(int newValue);
	bool doCheck();
	bool loadServer();
	bool parsePushCommands(const std::string& body);
	std::string getConfigPath() const;
	std::string getGameType() const;
	time_t getLastCalled() const;
	int getNextCheck() const;

	static FString buildCommand(std::string command, std::string playerName, std::string playerId, std::string UE4ID);
	static void ConsoleCommand(APlayerController* player, FString command);

private:
	static void ReplaceStringInPlace(std::string& subject, const std::string& search, const std::string& replace);
	void saveConfig();

	std::shared_ptr<spdlog::logger> logger_;
	WebstoreInfo webstoreInfo_;
	Config config_;
	int nextCheck_ = 15 * 60;
	time_t last_called_ = time(nullptr);
	bool serverLoaded_ = false;
};
