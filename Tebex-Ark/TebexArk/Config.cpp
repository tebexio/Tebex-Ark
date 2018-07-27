#pragma once
#include <API/ARK/Ark.h>

namespace tebexConfig {

	struct Config {
		bool buyEnabled = true;
		FString buyCommand = "!donate";
		FString secret = "";
		FString baseUrl = "https://plugin.buycraft.net";
	};
}