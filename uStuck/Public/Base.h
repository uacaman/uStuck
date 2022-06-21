#pragma once

#include <API/ARK/Ark.h>

#ifdef USTUCK_EXPORTS
#define USTUCK_API __declspec(dllexport)
#else
#define USTUCK_API __declspec(dllimport)
#endif

namespace uStuck
{
}