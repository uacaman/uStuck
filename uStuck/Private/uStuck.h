#pragma once

#include "json.hpp"

namespace uStuck
{
	inline nlohmann::json config;
	FString GetCmd(const std::string& str);
} 
