#pragma once

#ifdef TEBEX_ARK
#include <API/ARK/Ark.h>
#else
#include <API/Atlas/Atlas.h>
#endif

struct WebstoreInfo {
	int id;
	FString name;
	FString domain;
	FString currency;
	FString currencySymbol;
	FString gameType;
	FString serverName;
	int serverId;
};
