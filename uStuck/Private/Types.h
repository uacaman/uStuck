#pragma once
#include <string>
#include <API/UE/BasicTypes.h>

struct Damaged
{
	Damaged(uint64 steamId, long long dtInsert);

	uint64 steamId;
	long long dtInsert;
};
