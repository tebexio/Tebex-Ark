#pragma once
#include <API/ARK/Ark.h>

namespace tebexConfig {

	struct Config {
		bool buyEnabled = true;
		FString buyCommand = "!donate";
		FString secret = "";
		FString baseUrl = "https://plugin.buycraft.net";
		bool enablePushCommands = false;
		FString ipPushCommands = "0.0.0.0";
		int portPushCommands = 1111;
	};
}