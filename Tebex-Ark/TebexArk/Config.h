#pragma once

#ifdef TEBEX_ARK
#include <API/ARK/Ark.h>
#else
#include <API/Atlas/Atlas.h>
#endif

namespace tebexConfig
{
	struct Config {
		bool buyEnabled = true;
		FString buyCommand = "!donate";
		FString baseUrl = "https://plugin.buycraft.net";
		bool enablePushCommands = false;
		FString ipPushCommands = "0.0.0.0";
		int portPushCommands = 1111;
		FString secret = "";
	};
}
