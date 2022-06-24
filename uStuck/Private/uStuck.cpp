#include <fstream>
#include <Requests.h>
#include <API/ARK/Ark.h>

#include "uStuck.h"
#include "Types.h"
#include "uStuckTimer.h"
#include <Points.h>

#pragma comment(lib, "ArkApi.lib")
#pragma comment(lib, "arkshop.lib")

DECLARE_HOOK(AGameMode_InitGame, void, AGameMode*, FString*, FString*, FString*);
DECLARE_HOOK(APrimalCharacter_TakeDamage, float, APrimalCharacter*, float, FDamageEvent*, AController*, AActor*);

#define DEBUG          false
#define DEBUG_INFO     if(DEBUG) Log::GetLog()->info
#define DEBUG_WARN     if(DEBUG) Log::GetLog()->warn
#define DEBUG_ERROR    if(DEBUG) Log::GetLog()->error

namespace uStuck
{
	FLinearColor cyan{ 0, 1, 1, 1 };
	FLinearColor red{ 1, 0, 0, 1 };

	TArray<std::shared_ptr<Damaged>> damagedList;
	TArray<uint64> teleporting;

	FString GetMsg(const std::string& str)
	{
		return FString(ArkApi::Tools::Utf8Decode(config["messages"].value(str, "no message found")).c_str());
	}

	FString GetText(const std::string& str)
	{
		return FString(ArkApi::Tools::Utf8Decode(config["messages"].value(str, "no message found")).c_str());
	}

	FString GetPermission(const std::string& str)
	{
		auto command = FString(ArkApi::Tools::Utf8Decode(config["commands"][str].value("requiredPermission", "no command found")));
		return command;
	}

	void SetIsTeleporting(uint64 steamId)
	{
		teleporting.Add(steamId);

	}

	bool GetIsTeleporting(uint64 steamId)
	{
		return teleporting.Contains(steamId);
	}

	void RemoveTeleporting(uint64 steamId)
	{
		if (teleporting.Contains(steamId))
			teleporting.Remove(steamId);
	}

	void SetFightCooldown(AShooterPlayerController* spc)
	{
		if (!spc)
			return;

		const uint64 steamId = ArkApi::IApiUtils::GetSteamIdFromController(spc);
		long long dtInsert = time(nullptr);

		bool atualizou = false;

		for (const auto& damaged : damagedList)
		{
			if (damaged->steamId == steamId) {
				damaged->dtInsert = dtInsert;
				atualizou = true;
				break;
			}
		}

		if (atualizou == false)
		{
			damagedList.Add(std::make_shared<Damaged>(steamId, dtInsert));
		}
	}

	float Hook_APrimalCharacter_TakeDamage(APrimalCharacter* _this, float Damage, FDamageEvent* DamageEvent, AController* EventInstigator, AActor* DamageCauser)
	{
		if (_this)
		{
			if (_this->IsA(AShooterCharacter::GetPrivateStaticClass()))
			{
				AController* controller = _this->GetCharacterController();
				AShooterPlayerController* spc = static_cast<AShooterPlayerController*>(controller);

				if (spc)
				{
					SetFightCooldown(spc);
				}
			}
		}

		if (EventInstigator)
		{
			if (EventInstigator->IsA(AShooterPlayerController::StaticClass()))
			{
				AShooterPlayerController* killerShooterController = static_cast<AShooterPlayerController*>(EventInstigator);

				if (killerShooterController)
				{
					SetFightCooldown(killerShooterController);
				}
			}
		}

		return APrimalCharacter_TakeDamage_original(_this, Damage, DamageEvent, EventInstigator, DamageCauser);
	}

	int FightCooldown(AShooterPlayerController* spc)
	{
		int fightCooldown = config["fightCooldown"];
		if (fightCooldown == 0)
			return 0;

		const uint64 steamId = ArkApi::IApiUtils::GetSteamIdFromController(spc);

		for (const auto& damaged : damagedList)
		{
			if (damaged->steamId == steamId) {

				long long diff = time(nullptr) - damaged->dtInsert;

				int falta = diff - fightCooldown;

				if (falta < 0)
				{
					falta = falta * -1;
					return falta;
				}
			}
		}

		return 0;
	}


	//FString GetPermission(const std::string& str)
	//{

	//	auto command = FString(			ArkApi::Tools::Utf8Decode(config["commands"][str].value("requiredPermission", "no command found")));
	//	return command;
	//}


	std::vector<float> GetPosition(FString posName)
	{
		DEBUG_INFO("GetPosition");
		const auto& positions = config.value("positions", nlohmann::json::array());

		for (const auto& position : positions)
		{
			DEBUG_INFO("LOOP");
			auto name = FString(ArkApi::Tools::Utf8Decode(position.value("name", "")));

			DEBUG_INFO("Position {} ", name.ToString());

			auto destination = position.value("position", std::vector<float>{0, 0, 0});

			if (posName.Equals(*name, ESearchCase::IgnoreCase)) {
				return destination;
			}
		}


		return std::vector<float>{0, 0, 0};
	}

	AActor* SpawnBubble(FVector position, uint64 steamId)
	{
		bool requireStayInTheBubble = config.value("requireStayInTheBubble", true);
		if (!requireStayInTheBubble)
			return nullptr;

		std::stringstream ss;
		ss << steamId;

		FString name = FString(ss.str());

		bool bubbleSize = config.value("bubbleSize", 300);

		FString path("Blueprint'/Game/Extinction/CoreBlueprints/HordeCrates/StorageBox_HordeShield.StorageBox_HordeShield'");
		UClass* bubble_class = UVictoryCore::BPLoadClass(&path);

		if (!bubble_class)
		{
			Log::GetLog()->error("Failed to spawn bubble for zone {}", name.ToString());
			return nullptr;
		}

		FRotator rot(0, 0, 180);
		FActorSpawnParameters params;
		params.bDeferBeginPlay = true;

		AActor* actor = ArkApi::GetApiUtils().GetWorld()->SpawnActor(bubble_class, &position, &rot, &params);

		if (!actor)
		{
			Log::GetLog()->error("Failed to spawn bubble");
			return nullptr;
		}

		APrimalStructureItemContainer* structure = static_cast<APrimalStructureItemContainer*>(actor);

		// Shield color
		UProperty* color_prop = structure->FindProperty(FName("ShieldColor", EFindName::FNAME_Add));
		color_prop->Set(structure, cyan);
		structure->MulticastProperty(FName("ShieldColor", EFindName::FNAME_Add));

		structure->BeginPlay();
		structure->bDisableActivationUnderwater() = false;
		structure->SetContainerActive(true);
		structure->bCanBeDamaged() = false;
		structure->bDestroyOnStasis() = false;

		// Vars to disable some stuff of the shield so it doesn't bug
		UProperty* level_prop = structure->FindProperty(FName("CurrentLevel", EFindName::FNAME_Add));
		level_prop->Set(structure, 1);

		UProperty* bool_prop = structure->FindProperty(FName("bIntermissionShield", EFindName::FNAME_Add));
		bool_prop->Set(structure, true);

		UFunction* netUpdateFunc = actor->FindFunctionChecked(FName("NetRefreshRadiusScale", EFindName::FNAME_Add));
		if (netUpdateFunc)
		{
			int bubbleVisualSize = config.value("bubbleVisualSize", 1);
			actor->ProcessEvent(netUpdateFunc, &bubbleVisualSize);
		}

		actor->TargetingTeamField() = 26438;
		actor->ForceReplicateNow(false, false);

		return actor;
	}

	void ShowM(int teleportTime, AShooterPlayerController* player, FVector start, FVector destination, AActor* bubble)
	{
		const uint64 steamId = ArkApi::IApiUtils::GetSteamIdFromController(player);
		if (GetIsTeleporting(steamId) == false)
		{
			if (bubble)
				bubble->Destroy(false, false);

			return;
		}

		if (!player || !player->PlayerStateField() || ArkApi::IApiUtils::IsPlayerDead(player))
		{
			RemoveTeleporting(steamId);

			if (bubble)
				bubble->Destroy(false, false);

			return;
		}

		const float notificationScale = config.value("notificationScale", 2.0f);
		const float notificationDisplayTime = config.value("notificationDisplayTime", 5.0f);

		int allowRidingDino = config.value("allowRidingDino", false);

		if (allowRidingDino == false)
		{
			if (player->IsRidingDino())
			{
				ArkApi::GetApiUtils().SendNotification(player, red, notificationScale, notificationDisplayTime, nullptr, *GetMsg("mounted"));
				
				RemoveTeleporting(steamId);
				
				if (bubble)
					bubble->Destroy(false, false);

				return;
			}
		}

		// validar se não está em c.d de pvp
		int cd = FightCooldown(player);
		if (cd > 0)
		{
			ArkApi::GetApiUtils().SendNotification(player, red, notificationScale, notificationDisplayTime, nullptr, *GetMsg("fightCooldown"), cd);
			
			RemoveTeleporting(steamId);

			if (bubble)
				bubble->Destroy(false, false);

			return;
		}

		bool requireStayInTheBubble = config.value("requireStayInTheBubble", true);

		//ServerRemovePassenger_Implementation
		if (requireStayInTheBubble)
		{
			int bubbleSize = config.value("bubbleSize", 300);
			FVector currentLoc = player->GetPlayerCharacter()->RootComponentField()->RelativeLocationField();
			float distance = FVector::Distance(start, currentLoc);

			DEBUG_INFO("Distance {}", distance);

			if (distance > bubbleSize)
			{
				ArkApi::GetApiUtils().SendNotification(player, red, notificationScale, notificationDisplayTime, nullptr, *GetMsg("stayInTheBubble"));

				RemoveTeleporting(steamId);
				
				if (bubble)
					bubble->Destroy(false, false);

				return;
			}
		}

		// countdowm
		teleportTime--;
		//ArkApi::GetApiUtils().SendNotification(player, cyan, 1.5, 2, nullptr, *GetMsg("teleporting"), teleportTime);

		FString message1 = FString::Format(*GetMsg("teleporting"), teleportTime);
		player->ClientServerNotificationSingle(&message1, cyan, notificationScale, 2, nullptr, nullptr, 20300);

		
		if (teleportTime > 0)
		{
			API::Timer::Get().DelayExecute(&ShowM, 1, teleportTime, player, start, destination, bubble);
			return;
		}

		int teleportArkShopPoints = config.value("teleportArkShopPoints", 0);

		if (teleportArkShopPoints > 0 && ArkApi::Tools::IsPluginLoaded("ArkShop"))
		{
			const int spent = ArkShop::Points::SpendPoints(teleportArkShopPoints, steamId);
			if (spent == false) {
				
				ArkApi::GetApiUtils().SendChatMessage(player, uStuck::GetCmd("sender"), *GetMsg("noPoints"));
				
				RemoveTeleporting(steamId);

				if (bubble)
					bubble->Destroy(false, false);

				return;
			}
			else
			{
				ArkApi::GetApiUtils().SendChatMessage(player, uStuck::GetCmd("sender"), *GetMsg("spent"), teleportArkShopPoints);
			}
		}

		if (bubble)
			bubble->Destroy(false, false);

		player->ServerRemovePassenger_Implementation();
		player->SetPlayerPos(destination.X, destination.Y, destination.Z);
		RemoveTeleporting(steamId);
	}

	void uStuckCmd(AShooterPlayerController* player, FString* message, EChatSendMode::Type /*unused*/)
	{
		//fazer um comando de ajuda
		if (ArkApi::IApiUtils::IsPlayerDead(player))
			return;
		
		TArray<FString> parsed;
		message->ParseIntoArray(parsed, L" ", true);

		FString positionName = "default";
		if (parsed.IsValidIndex(1))
		{
			positionName = parsed[1];
		}

		auto destination = GetPosition(positionName);

		if (destination[0] == 0 && destination[1] == 0 && destination[2] == 0)
		{
			ArkApi::GetApiUtils().SendChatMessage(player, uStuck::GetCmd("sender"), *GetMsg("notFound"));
			return;
		}

		const uint64 steamId = ArkApi::IApiUtils::GetSteamIdFromController(player);
		if (GetIsTeleporting(steamId))
		{
			bool allowFastTeleport = config.value("allowFastTeleport", false);
			if (allowFastTeleport)
			{
				int fastTeleportArkShopPoints = config.value("fastTeleportArkShopPoints", 0);

				if (fastTeleportArkShopPoints > 0 && ArkApi::Tools::IsPluginLoaded("ArkShop"))
				{
					const int spent = ArkShop::Points::SpendPoints(fastTeleportArkShopPoints, steamId);
					if (spent == false) {
						ArkApi::GetApiUtils().SendChatMessage(player, uStuck::GetCmd("sender"), *GetMsg("noPoints"));
						return;
					}
					else
					{
						ArkApi::GetApiUtils().SendChatMessage(player, uStuck::GetCmd("sender"), *GetMsg("spent"), fastTeleportArkShopPoints);
					}
				}

				player->ServerRemovePassenger_Implementation();
				RemoveTeleporting(steamId);

				player->SetPlayerPos(destination[0], destination[1], destination[2]);
			}

			return;
		}

		const float notificationScale = config.value("notificationScale", 2.0f);
		const float notificationDisplayTime = config.value("notificationDisplayTime", 5.0f);

		int allowRidingDino = config.value("allowRidingDino", false);

		if (allowRidingDino == false)
		{
			if (player->IsRidingDino())
			{
				ArkApi::GetApiUtils().SendNotification(player, red, notificationScale, notificationDisplayTime, nullptr, *GetMsg("mounted"));
				return;
			}
		}

		int cd = FightCooldown(player);
		if (cd > 0)
		{
			ArkApi::GetApiUtils().SendNotification(player, red, 1.5, notificationDisplayTime, nullptr, *GetMsg("fightCooldown"), cd);
			return;
		}

		int teleportArkShopPoints = config.value("teleportArkShopPoints", 0);

		if (teleportArkShopPoints > 0 && ArkApi::Tools::IsPluginLoaded("ArkShop"))
		{
			const int player_points = ArkShop::Points::GetPoints(steamId);

			if (player_points < teleportArkShopPoints) {
				ArkApi::GetApiUtils().SendChatMessage(player, uStuck::GetCmd("sender"), *GetMsg("noPoints"));
				return;
			}
		}

		int teleportTime = config.value("teleportTime", 30);

		FString message1 = FString::Format(*GetMsg("teleporting"), teleportTime);
		player->ClientServerNotificationSingle(&message1, cyan, notificationScale, 2, nullptr, nullptr, 20300);

		FVector start = player->GetPlayerCharacter()->RootComponentField()->RelativeLocationField();
		FVector fDestination(destination[0], destination[1], destination[2]);

		SetIsTeleporting(steamId);
		AActor* bubble = SpawnBubble(start, steamId);

		API::Timer::Get().DelayExecute(&ShowM, 1, teleportTime, player, start, fDestination, bubble);
		return;
	}

	void PrintLocation(AShooterPlayerController* player, FString* message, EChatSendMode::Type /*unused*/)
	{
		if (!player || !player->PlayerStateField() || ArkApi::IApiUtils::IsPlayerDead(player))
			return;

		FString map_name;
		ArkApi::GetApiUtils().GetShooterGameMode()->GetMapName(&map_name);

		FVector pos = player->DefaultActorLocationField();

		ArkApi::GetApiUtils().SendChatMessage(player, uStuck::GetCmd("sender"), "{}::{}", map_name.ToString(), pos.ToString().ToString());
	}


	void GetCallback(bool success, std::string result) {
	}

	void StartLicenseValidation()
	{
		HW_PROFILE_INFO hwProfileInfo;
		GetCurrentHwProfile(&hwProfileInfo);

		std::wstring ws(hwProfileInfo.szHwProfileGuid);
		std::string hwid(ws.begin(), ws.end());

		API::Requests::Get().CreateGetRequest(fmt::format("http://jogabrasil.cc/api/License/Exists/uStuck/{}", hwid), &GetCallback);
	}

	void Hook_AGameMode_InitGame(AGameMode* _this, FString* MapName, FString* Options, FString* ErrorMessage)
	{
		StartLicenseValidation();
		AGameMode_InitGame_original(_this, MapName, Options, ErrorMessage);
	}

	FString uStuck::GetCmd(const std::string& str)
	{
		return FString(ArkApi::Tools::Utf8Decode(config.value(str, "No message")));
	}

	void ReadConfig()
	{
		const std::string config_path = ArkApi::Tools::GetCurrentDir() + "/ArkApi/Plugins/uStuck/config.json";
		std::ifstream file{ config_path };
		if (!file.is_open())
		{
			throw std::runtime_error("Can't open config.json");
		}

		file >> uStuck::config;

		file.close();
	}

	void Load()
	{
		Log::Get().Init("uStuck");

		try
		{
			ReadConfig();
		}
		catch (const std::exception& error)
		{
			Log::GetLog()->error(error.what());
			throw;
		}

		try
		{
			ArkApi::GetHooks().SetHook("AGameMode.InitGame", &Hook_AGameMode_InitGame, &AGameMode_InitGame_original);
			ArkApi::GetHooks().SetHook("APrimalCharacter.TakeDamage", &Hook_APrimalCharacter_TakeDamage, &APrimalCharacter_TakeDamage_original);

			if (ArkApi::GetApiUtils().GetStatus() == ArkApi::ServerStatus::Ready)
			{
				StartLicenseValidation();
			}

			auto& commands = ArkApi::GetCommands();
			commands.AddChatCommand(uStuck::GetCmd("teleport"), &uStuckCmd);
			commands.AddChatCommand(uStuck::GetCmd("location"), &PrintLocation);

		}
		catch (const std::exception& error)
		{
			Log::GetLog()->error(error.what());
			throw;
		}
	}

	void Unload()
	{
		ArkApi::GetHooks().DisableHook("AGameMode.InitGame", &Hook_AGameMode_InitGame);
		ArkApi::GetHooks().DisableHook("APrimalCharacter.TakeDamage", &Hook_APrimalCharacter_TakeDamage);

		auto& commands = ArkApi::GetCommands();
		commands.RemoveChatCommand(uStuck::GetCmd("teleport"));
		commands.RemoveChatCommand(uStuck::GetCmd("location"));
	}
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD ul_reason_for_call, LPVOID /*lpReserved*/)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		uStuck::Load();
		break;
	case DLL_PROCESS_DETACH:
		uStuck::Unload();
		break;
	}
	return TRUE;
}


