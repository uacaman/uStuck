
//"": {
 //		"sender": "JBR",
//		"teleport": "/fu",
//		"location" : "/pos"
//      "coordinate": { "x": 1,  "y" : 5 "z" : 5 }



#include <fstream>
#include <Requests.h>
#include <API/ARK/Ark.h>

#include "uStuck.h"

#pragma comment(lib, "ArkApi.lib")

DECLARE_HOOK(AGameMode_InitGame, void, AGameMode*, FString*, FString*, FString*);
DECLARE_HOOK(APrimalCharacter_TakeDamage, float, APrimalCharacter*, float, FDamageEvent*, AController*, AActor*);

namespace uStuck
{
	struct Damaged
	{
		Damaged(uint64 steamId, long long dtInsert);

		uint64 steamId;
		long long dtInsert;
	};

	TArray<std::shared_ptr<Damaged>> damagedList;

	void SetAsPVPVE(AShooterPlayerController* spc)
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
					SetAsPVPVE(spc);
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
					SetAsPVPVE(killerShooterController);
				}
			}
		}

		return APrimalCharacter_TakeDamage_original(_this, Damage, DamageEvent, EventInstigator, DamageCauser);
	}

	int PVPVECooldown(AShooterPlayerController* spc)
	{
		int PVPVECooldown = config["PVPVECooldown"];
		if (PVPVECooldown == 0)
			return 0;

		const uint64 steamId = ArkApi::IApiUtils::GetSteamIdFromController(spc);

		for (const auto& damaged : damagedList)
		{
			if (damaged->steamId == steamId) {

				long long diff = time(nullptr) - damaged->dtInsert;

				int falta = diff - PVPVECooldown;

				if (falta < 0)
				{
					falta = falta * -1;
					return falta;
				}
			}
		}

		return 0;
	}

	void uStuckCmd(AShooterPlayerController* player, FString* message, EChatSendMode::Type /*unused*/)
	{
		if (!player || !player->PlayerStateField() || ArkApi::IApiUtils::IsPlayerDead(player))
			return;

		const auto& coordinate = uStuck::config["coordinate"];
		const float x = coordinate["x"];
		const float y = coordinate["y"];
		const float z = coordinate["z"];

		//auto* primal_character = static_cast<APrimalCharacter*>(player->CharacterField());

		//if (primal_character)
		//{
			//Log::GetLog()->info("primal_character");

		if (player->IsRidingDino())
		{
			Log::GetLog()->info("IsRidingDino");
			ArkApi::GetApiUtils().SendChatMessage(player, uStuck::GetCmd("sender"), *uStuck::GetCmd("mounted"));
			return;
		}
		/*}*/


		player->SetPlayerPos(x, y, z);
	}

	void PrintLocation(AShooterPlayerController* player, FString* message, EChatSendMode::Type /*unused*/)
	{
		if (!player || !player->PlayerStateField() || ArkApi::IApiUtils::IsPlayerDead(player))
			return;

		FVector pos = player->DefaultActorLocationField();
		FString  aaa = pos.ToString();

		//Log::GetLog()->info("Posição ({})", aaa.ToString());

		ArkApi::GetApiUtils().SendChatMessage(player, uStuck::GetCmd("sender"), *aaa);


		//bool success = false;

		//const uint64 steamid = ArkApi::IApiUtils::GetSteamIdFromController(player);

		//if (DBHelper::IsPlayerExists(steamid) == false)
		//	return;

		//uint64 playerTribeId = player->TargetingTeamField();

		//const int final_price = 1;

		//const int points = Tokens::GetToken(steamid);

		//if (points < final_price)
		//{
		//	ArkApi::GetApiUtils().SendChatMessage(player, GetText("Sender"), *GetText("NotEnough"));
		//	return;
		//}

		//AActor* Actor = player->GetPlayerCharacter()->GetAimedActor(ECC_GameTraceChannel2, nullptr, 0.0, 0.0, nullptr, nullptr, false, false, false);

		//if (Actor && Actor->IsA(APrimalDinoCharacter::GetPrivateStaticClass()) == false) {

		//	ArkApi::GetApiUtils().SendChatMessage(player, GetText("Sender"), *GetText("Invalid"));
		//	return;
		//}

		//APrimalDinoCharacter* dino = static_cast<APrimalDinoCharacter*>(Actor);

		//const int dinoTribeId = dino->TargetingTeamField();

		//if (playerTribeId != dinoTribeId) {
		//	ArkApi::GetApiUtils().SendChatMessage(player, GetText("Sender"), *GetText("NotInTribe"));
		//	return;
		//}

		//if (dino->bIsBaby()() == false) {
		//	ArkApi::GetApiUtils().SendChatMessage(player, GetText("Sender"), *GetText("NotABaby"));
		//	return;
		//}

		//if (dino->bHasDied()() == true) {
		//	ArkApi::GetApiUtils().SendChatMessage(player, GetText("Sender"), *GetText("IsDead"));
		//	return;
		//}

		//UPrimalCharacterStatusComponent* status = dino->GetCharacterStatusComponent();

		//float d = status->DinoImprintingQualityField();

		//if (d == 1)
		//{
		//	ArkApi::GetApiUtils().SendChatMessage(player, GetText("Sender"), *GetText("AlreadyImprinted"));
		//	return;
		//}

		////FString reply = FString::Format("Qualidade {}", d);

		////ArkApi::GetApiUtils().SendChatMessage(player, GetText("Sender"), *reply);

		//dino->UpdateImprintingQuality(1.0);

		//Tokens::SpendToken(final_price, steamid);

		//ArkApi::GetApiUtils().SendChatMessage(player, GetText("Sender"), *GetText("Sucess"));
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
		uStuck:Load();
		break;
	case DLL_PROCESS_DETACH:
		uStuck::Unload();
		break;
	}
	return TRUE;
}


